import type { Attributes, Graph } from './graph.d.ts';
import type { Location } from './location.ts';
import {
  NormalizedGraph,
  normalizeGraph,
  type OverrideAttributes,
} from './normalize-graph.ts';
import { parseDot } from './parser.ts';

/**
 * @property formats
 * {@link https://www.graphviz.org/docs/outputs/ | Graphviz output formats} to render. For example, `"dot"` or `"svg"`.
 *
 * @property engine
 * The {@link https://www.graphviz.org/docs/layouts/ | Graphviz layout engine} to use for graph layout. For example, `"dot"` or `"neato"`.
 *
 * @property yInvert
 * Invert y coordinates in output. This corresponds to the {@link https://www.graphviz.org/doc/info/command.html#-y | `-y`} Graphviz command-line option</a>.
 *
 * @property reduce
 * Reduce the graph. This corresponds to the {@link https://www.graphviz.org/doc/info/command.html#-x | `-x`} Graphviz command-line option</a>.
 *
 * @property graphAttributes
 * Sets the default graph attributes. This corresponds the {@link https://www.graphviz.org/doc/info/command.html#-G | `-G`} Graphviz command-line option</a>.
 *
 * @property nodeAttributes
 * Sets the default node attributes. This corresponds the {@link https://www.graphviz.org/doc/info/command.html#-N `-N`} Graphviz command-line option</a>.
 *
 * @property edgeAttributes
 * Sets the default edge attributes. This corresponds the {@link https://www.graphviz.org/doc/info/command.html#-E | `-E`} Graphviz command-line option</a>.
 *
 * @property images
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
 * viz.render('graph { a[image="test.png"] }', {
 *   images: {
 *     'test.png': { width: 300, height: 200 },
 *   },
 * });
 * ```
 */
export interface RenderOptions {
  formats?: OutputFormat[];
  engine?: LayoutEngine;
  yInvert?: boolean;
  reduce?: boolean;
  graphAttributes?: Attributes;
  nodeAttributes?: Attributes;
  edgeAttributes?: Attributes;
  images?: Record<string, ImageSize>;
}

/**
 * The result object returned by {@link Viz.render}.
 */
export type RenderResult = SuccessResult | FailureResult;

/**
 * Returned if rendering was successful. `errors` may contain warning messages even if the graph rendered successfully.
 */
export interface SuccessResult {
  status: 'success';
  output: { dot: string | undefined; svg: string | undefined };
  errors: RenderError[];
}

/**
 * Returned by {@link Viz.render} or {@link Viz.renderFormats} if rendering failed.
 */
export interface FailureResult {
  status: 'failure';
  output: undefined;
  errors: RenderError[];
}

export interface RenderError {
  readonly level: 'error' | 'warning';
  readonly message: string;
  readonly location: Location | undefined;

  toString(): string;
}

/**
 * Specifies the size of an image used as a node's `image` attribute. See {@link RenderOptions.images}.
 *
 * `width` and `height` may be specified as numbers or strings with units: in, px, pc, pt, cm, or mm. If no units are given or measurements are given as numbers, points (pt) are used.
 *
 * @property width
 * The width of the image.
 *
 * @property height
 * The height of the image.
 */
export interface ImageSize {
  width: string | number;
  height: string | number;
}

/**
 * The names of the {@link https://www.graphviz.org/docs/outputs/ | Graphviz output formats} supported at runtime.
 */
export const outputFormats = ['dot', 'svg'] as const;
export type OutputFormat = (typeof outputFormats)[number];

/**
 * The names of the {@link https://www.graphviz.org/docs/layouts/ | Graphviz layout engines} supported at runtime.
 */
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

/**
 * The {@link Viz} class isn't exported, but it can be instantiated using the {@link instance} function.
 */
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
   * Renders the graph described by the input and returns the result as an object.
   *
   * `input` may be a string in {@link https://www.graphviz.org/doc/info/lang.html | DOT syntax} or a {@link Graph | graph object}.
   *
   * This method does not throw an error if rendering failed, including for invalid DOT syntax, but it will throw for invalid types in input or unexpected runtime errors.
   */
  render(input: string | Graph, options: RenderOptions = {}): RenderResult {
    const overrideAttributes: OverrideAttributes = {
      graphAttributes: options.graphAttributes,
      nodeAttributes: options.nodeAttributes,
      edgeAttributes: options.edgeAttributes,
    };

    let graph: NormalizedGraph;
    const warnings: RenderError[] = [];
    if (typeof input === 'string') {
      const graphList = parseDot(input, overrideAttributes);
      if (graphList.length === 0) {
        return failureResult(
          new RenderingBackendError(
            "Missing graph definition. Start your file with 'graph {}' or 'digraph {}'.",
          ),
        );
      }

      for (const { diagnostics } of graphList) {
        warnings.push(...diagnostics);
      }

      if (graphList.length > 1) {
        warnings.push(
          new RenderingBackendWarning(
            'Multiple graphs found. Using the first one.',
          ),
        );
      }

      if (graphList[0].graph === undefined) {
        return failureResult();
      }
      graph = graphList[0].graph;
    } else {
      graph = normalizeGraph(input, overrideAttributes);
    }

    const layout = graph.graphAttributes.layout;
    if (layout != undefined && !isLayoutEngine(layout)) {
      return failureResult(
        new RenderingBackendError(
          `Layout type: ${JSON.stringify(layout)} not recognized. Use one of: ${layoutEngines.join(' ')}`,
        ),
      );
    }

    if (
      layout != undefined &&
      options.engine != undefined &&
      layout !== options.engine
    ) {
      return failureResult(
        new RenderingBackendError(
          `Engine mismatch: layout attribute in graph ("${layout}") conflicts with engine option ("${options.engine}"). Remove one or make them match.`,
        ),
      );
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
      const errors = [
        ...warnings,
        ...response.errors.map((error) =>
          error.level === 'warning'
            ? new RenderingBackendWarning(error.message)
            : new RenderingBackendError(error.message),
        ),
      ];

      if (response.status === 'failure') {
        return { status: 'failure', output: undefined, errors };
      }

      const output = {
        dot: response.output.dot ?? undefined,
        svg: response.output.svg ?? undefined,
      };
      return { status: 'success', output, errors };
    } finally {
      this._wasm.wasm_free(outputJSONBuf.byteOffset, outputJSONBuf.length);
    }

    function failureResult(error?: RenderError): FailureResult {
      return {
        status: 'failure',
        output: undefined,
        errors: error ? [...warnings, error] : warnings,
      };
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

class RenderingBackendError implements RenderError {
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

class RenderingBackendWarning implements RenderError {
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
