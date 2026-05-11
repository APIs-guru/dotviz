import type { Attributes, Graph } from './graph.d.ts';
import type { Location } from './location.ts';
import { NormalizedGraph, normalizeGraph } from './normalize-graph.ts';
import { parseDot } from './parser.ts';

export interface OverrideAttributes {
  /** Sets the default graph attributes. This corresponds to the {@link https://www.graphviz.org/doc/info/command.html#-G | `-G`} Graphviz command-line option. */
  readonly graphAttributes?: Readonly<Attributes> | undefined;

  /** Sets the default node attributes. This corresponds to the {@link https://www.graphviz.org/doc/info/command.html#-N `-N`} Graphviz command-line option. */
  readonly nodeAttributes?: Readonly<Attributes> | undefined;

  /** Sets the default edge attributes. This corresponds to the {@link https://www.graphviz.org/doc/info/command.html#-E | `-E`} Graphviz command-line option. */
  readonly edgeAttributes?: Readonly<Attributes> | undefined;
}

export interface RenderOptions {
  /** {@link https://www.graphviz.org/docs/outputs/ | Graphviz output formats} to render. For example, `"dot"` or `"svg"`. */
  formats?: OutputFormat[];

  /** The {@link https://www.graphviz.org/docs/layouts/ | Graphviz layout engine} to use for graph layout. For example, `"dot"` or `"neato"`. */
  engine?: LayoutEngine;

  /** Invert y coordinates in output. This corresponds to the {@link https://www.graphviz.org/doc/info/command.html#-y | `-y`} Graphviz command-line option. */
  yInvert?: boolean;

  /** Reduce the graph. This corresponds to the {@link https://www.graphviz.org/doc/info/command.html#-x | `-x`} Graphviz command-line option. */
  reduce?: boolean;

  overrideAttributes?: OverrideAttributes;
  /**
   * The name of the image used as a key of the object.
   * In addition to filenames, names that look like absolute filesystem paths or URLs can be used.
   * For example:
   *
   * - `"example.png"`
   * - `"/images/example.png"`
   * - `"http://example.com/image.png"`
   *
   * Names that look like relative filesystem paths, such as `"../example.png"`, are not supported.
   * Image sizes to use when rendering nodes with <code>image</code> attributes.
   *
   * For example, to indicate to Graphviz that the image <code>test.png</code> has size 300x200:
   *
   * ```js
   * viz.renderDot('graph { a[image="test.png"] }', {
   *   images: {
   *     'test.png': { width: 300, height: 200 },
   *   },
   * });
   * ```
   */
  images?: Record<string, ImageSize>;
}

export type RenderResult = SuccessResult | FailureResult;

/**
 * Returned if rendering was successful. `diagnostics` may contain warning messages even if the graph rendered successfully.
 */
export interface SuccessResult {
  status: 'success';
  output: { dot: string | undefined; svg: string | undefined };
  diagnostics: Diagnostic[];
}

/**
 * Returned if rendering failed.
 */
export interface FailureResult {
  status: 'failure';
  output: undefined;
  diagnostics: Diagnostic[];
}

export interface Diagnostic {
  readonly level: 'error' | 'warning';
  readonly message: string;
  readonly location: Location | undefined;

  toString(): string;
}

/**
 * Specifies the size of an image used as a node's `image` attribute. See {@link RenderOptions.images}.
 *
 * `width` and `height` may be specified as numbers or strings with units: in, px, pc, pt, cm, or mm. If no units are given or measurements are given as numbers, points (pt) are used.
 */
export interface ImageSize {
  /** The width of the image. */
  width: string | number;
  /** The height of the image. */
  height: string | number;
}

/**
 * The names of the {@link https://www.graphviz.org/docs/outputs/ | Graphviz output formats} supported at runtime.
 */
export const outputFormats = ['dot', 'svg'] as const;
export type OutputFormat = (typeof outputFormats)[number];

/** The names of the {@link https://www.graphviz.org/docs/layouts/ | Graphviz layout engines} supported at runtime. */
export const layoutEngines = [
  'dot',
  'circo',
  'neato',
  'fdp',
  'twopi',
  'patchwork',
  'osage',
  'sfdp',
] as const;

export type LayoutEngine = (typeof layoutEngines)[number];
export function isLayoutEngine(value: unknown): value is LayoutEngine {
  return (
    typeof value === 'string' && layoutEngines.includes(value as LayoutEngine)
  );
}

/** The {@link Viz} class isn't exported, but it can be instantiated using the {@link instance} function. */
export class Viz {
  _stdoutBuf = '';
  _stderrBuf = '';
  _utf8Encoder: TextEncoder = new TextEncoder();
  _utf8Decoder: TextDecoder = new TextDecoder('utf8');
  _wasm: {
    memory: Uint8Array;
    wasm_alloc(length: number): number;
    wasm_free(ptr: number, length: number): void;
    render(jsonPtr: number, jsonLength: number): bigint;
  };
  /**
   * @internal
   */
  constructor(instance: WebAssembly.Instance) {
    // @ts-expect-error not sure how to properly type it
    this._wasm = instance.exports;
  }

  /**
   * Renders the graph described by a {@link Graph | graph object} and returns the result as an object.
   *
   * This method does not throw an error if rendering failed, but it will throw for invalid types in input or unexpected runtime errors.
   */
  renderGraph(input: Graph, options: RenderOptions = {}): RenderResult {
    const graph = normalizeGraph(input, options.overrideAttributes ?? {});
    return this.#renderNormalizedGraph(graph, options);
  }

  /**
   * Renders the graph described by a string in {@link https://www.graphviz.org/doc/info/lang.html | DOT syntax}  and returns the result as an object.
   *
   * This method does not throw an error if rendering failed, including for invalid DOT syntax, but it will throw for invalid types in input or unexpected runtime errors.
   */
  renderDot(input: string, options: RenderOptions = {}): RenderResult {
    const graphList = parseDot(input, options.overrideAttributes ?? {});
    if (graphList.length === 0) {
      return failureResult([
        new RenderingBackendError(
          "Missing graph definition. Start your file with 'graph {}' or 'digraph {}'.",
        ),
      ]);
    }

    const diagnostics: Diagnostic[] = graphList.flatMap(
      ({ diagnostics }) => diagnostics,
    );
    if (graphList.length > 1) {
      diagnostics.push(
        new RenderingBackendWarning(
          'Multiple graphs found. Using the first one.',
        ),
      );
    }

    const graph = graphList[0].graph;
    if (graph === undefined) {
      return failureResult(diagnostics);
    }
    const result = this.#renderNormalizedGraph(graph, options);
    return {
      ...result,
      diagnostics: [...diagnostics, ...result.diagnostics],
    };
  }

  #renderNormalizedGraph(
    graph: NormalizedGraph,
    options: RenderOptions,
  ): RenderResult {
    const { layout, charset } = graph.graphAttributes;

    if (layout != undefined && !isLayoutEngine(layout)) {
      return failureResult([
        new RenderingBackendError(
          `Layout type: ${JSON.stringify(layout)} not recognized. Use one of: ${layoutEngines.join(' ')}`,
        ),
      ]);
    }

    if (
      layout != undefined &&
      options.engine != undefined &&
      layout !== options.engine
    ) {
      return failureResult([
        new RenderingBackendError(
          `Engine mismatch: layout attribute in graph ("${layout}") conflicts with engine option ("${options.engine}"). Remove one or make them match.`,
        ),
      ]);
    }

    // eslint-disable-next-line unicorn/text-encoding-identifier-case
    if (charset && charset !== 'utf-8' && charset !== 'utf8') {
      return failureResult([
        new RenderingBackendError(
          `Unsupported charset: ${JSON.stringify(charset)}. Only 'utf-8' and 'utf8' are supported.`,
        ),
      ]);
    }

    const formats = options.formats ?? ['dot'];
    const request = {
      graph,
      engine: options.engine ?? layout ?? 'dot',
      yInvert: options.yInvert ?? false,
      reduce: options.reduce ?? false,
      images: this._normalizeImages(options.images),
      renderDot: formats.includes('dot'),
      renderSvg: formats.includes('svg'),
    };
    const requestJSON = JSON.stringify(request);
    const cJson = this._utf8Encoder.encode(requestJSON);
    const jsonPtr = this._wasm.wasm_alloc(cJson.length);
    const inputJSONBuf = new Uint8Array(
      this._wasm.memory.buffer,
      jsonPtr,
      cJson.length,
    );
    inputJSONBuf.set(cJson);
    const sliceU64 = this._wasm.render(
      inputJSONBuf.byteOffset,
      inputJSONBuf.length,
    );
    const ptr = Number(BigInt.asUintN(32, sliceU64));
    const len = Number(BigInt.asUintN(32, sliceU64 >> 32n));
    const outputJSONBuf = new Uint8Array(this._wasm.memory.buffer, ptr, len);
    try {
      const str: string = this._utf8Decoder.decode(outputJSONBuf);
      const response = JSON.parse(str) as RenderResult;
      const diagnostics = response.diagnostics.map((error) =>
        error.level === 'warning'
          ? new RenderingBackendWarning(error.message)
          : new RenderingBackendError(error.message),
      );
      if (response.status === 'failure') {
        return { status: 'failure', output: undefined, diagnostics };
      }

      const output = {
        dot: response.output.dot ?? undefined,
        svg: response.output.svg ?? undefined,
      };
      return { status: 'success', output, diagnostics };
    } finally {
      this._wasm.wasm_free(outputJSONBuf.byteOffset, outputJSONBuf.length);
    }
  }

  _normalizeImages(
    images: Record<string, ImageSize> = {},
  ): Record<string, ImageSize> {
    return Object.fromEntries(
      Object.entries(images).map(([name, { height, width }]) => [
        name,
        { height: height.toString(), width: width.toString() },
      ]),
    );
  }

  /* v8 ignore next -- used only for debugging */
  _wasi_fd_write(
    fd: number,
    iovs_ptr: number,
    iovs_len: number,
    nwritten_ptr: number,
  ): number {
    const mem = new Uint8Array(this._wasm.memory.buffer);
    const view = new DataView(this._wasm.memory.buffer);

    let totalWritten = 0;

    let bufferStr = '';
    for (let i = 0; i < iovs_len; i++) {
      const base = view.getUint32(iovs_ptr + i * 8, true);
      const len = view.getUint32(iovs_ptr + i * 8 + 4, true);
      const chunk = mem.subarray(base, base + len);
      bufferStr += this._utf8Decoder.decode(chunk);

      totalWritten += len;
    }

    switch (fd) {
      case 1: {
        const lines = (this._stdoutBuf + bufferStr).split('\n');
        this._stdoutBuf = lines.pop() ?? '';
        for (const line of lines) {
          console.log(line);
        }
        break;
      }
      case 2: {
        const lines = (this._stderrBuf + bufferStr).split('\n');
        this._stderrBuf = lines.pop() ?? '';
        for (const line of lines) {
          console.error(line);
        }
        break;
      }
      default:
        console.trace(`fd_write: unknown fd ${fd.toString()}`);
        return 52; // WASI_ERRNO_NOTSUP
    }

    view.setUint32(nwritten_ptr, totalWritten, true);
    return 0;
  }
}

function failureResult(diagnostics: Diagnostic[]): RenderResult {
  return {
    status: 'failure',
    output: undefined,
    diagnostics,
  };
}

class RenderingBackendError implements Diagnostic {
  readonly level = 'error';
  readonly message: string;
  readonly location = undefined;

  constructor(message: string) {
    this.message = message;
  }

  toString() {
    return 'RenderingBackendError: ' + this.message;
  }
}

class RenderingBackendWarning implements Diagnostic {
  readonly level = 'warning';
  readonly message: string;
  readonly location = undefined;

  constructor(message: string) {
    this.message = message;
  }

  toString() {
    return 'RenderingBackendWarning: ' + this.message;
  }
}
