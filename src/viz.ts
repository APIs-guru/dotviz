import type { Attributes, Graph } from './graph.d.ts';
import type { Location } from './location.ts';
import {
  type FixedAttributes,
  NormalizedGraph,
  normalizeGraph,
} from './normalize-graph.ts';
import { parseDot } from './parser.ts';

/**
 * @property format
 * The {@link https://www.graphviz.org/docs/outputs/ | Graphviz output format} to render. For example, `"dot"` or `"svg"`.
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
  format?: string;
  engine?: string;
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
 * Returned by {@link Viz.render} if rendering was successful. `errors` may contain warning messages even if the graph rendered successfully.
 */
export interface SuccessResult {
  status: 'success';
  output: string;
  errors: RenderError[];
}

/**
 * Returned by {@link Viz.render} or {@link Viz.renderFormats} if rendering failed.
 */
export interface FailureResult {
  status: 'failure';
  output: null;
  errors: RenderError[];
}

/**
 * The result object returned by {@link Viz.renderFormats}.
 */
export type MultipleRenderResult = MultipleSuccessResult | FailureResult;

/**
 * Returned by {@link Viz.renderFormats} if rendering was successful. `errors` may contain warning messages even if the graph rendered successfully.
 */
export interface MultipleSuccessResult {
  status: 'success';
  output: Record<string, string>;
  errors: RenderError[];
}

export interface RenderError {
  level: 'error' | 'warning';
  message: string;
  location: Location | null;

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
   * Renders the graph described by the input for each format in `formats` and returns the result as an object. For a successful result, `output` is an object keyed by format.
   */
  renderFormats(
    input: string | Graph,
    formats: readonly string[],
    options: RenderOptions = {},
  ): MultipleRenderResult {
    const fixedAttributes: FixedAttributes = {
      graphAttributes: options.graphAttributes,
      nodeAttributes: options.nodeAttributes,
      edgeAttributes: options.edgeAttributes,
    };
    return this._renderInput(input, formats, fixedAttributes, options);
  }

  /**
   * Renders the graph described by the input and returns the result as an object.
   *
   * `input` may be a string in {@link https://www.graphviz.org/doc/info/lang.html | DOT syntax} or a {@link Graph | graph object}.
   *
   * This method does not throw an error if rendering failed, including for invalid DOT syntax, but it will throw for invalid types in input or unexpected runtime errors.
   */
  render(input: string | Graph, options: RenderOptions = {}): RenderResult {
    const format = options.format ?? 'dot';

    const result = this.renderFormats(input, [format], options);

    return result.status === 'success'
      ? {
          ...result,
          output: result.output[format],
        }
      : result;
  }

  /**
   * Renders the input and returns the result as a string. Throws an error if rendering failed.
   */
  renderString(input: string | Graph, options: RenderOptions = {}): string {
    const result = this.render(input, options);

    if (result.status !== 'success') {
      let message = result.errors.find((e) => e.level == 'error')?.message;
      message ??= 'Unknown error';
      throw new Error(message);
    }

    return result.output;
  }

  _renderInput(
    input: string | Graph,
    formats: readonly string[],
    fixedAttributes: FixedAttributes,
    options: RenderOptions,
  ): MultipleRenderResult {
    let graph: NormalizedGraph;
    const warnings: RenderError[] = [];
    if (typeof input === 'string') {
      const result = parseDot(input, fixedAttributes);
      if (result.status === 'failure') {
        return result;
      }
      graph = result.output;
      warnings.push(...result.errors);
    } else {
      graph = normalizeGraph(input, fixedAttributes);
    }

    let renderGv = false;
    let renderDot = false;
    let renderSvg = false;
    for (const name of formats) {
      switch (name) {
        case 'gv':
          renderGv = true;
          break;
        case 'dot':
          renderDot = true;
          break;
        case 'svg':
          renderSvg = true;
          break;
        default:
          return {
            status: 'failure',
            output: null,
            errors: [
              new RenderingBackendError(
                'error',
                `Format: "${name}" not recognized. Use one of: dot gv svg`,
              ),
            ],
          };
      }
    }

    const request = {
      graph,
      renderDot: renderDot || renderGv,
      renderSvg,
      engine: options.engine ?? 'dot',
      yInvert: options.yInvert ?? false,
      reduce: options.reduce ?? false,
      images: this._normalizeImages(options.images),
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
      const response = JSON.parse(str) as MultipleRenderResult;
      response.errors = response.errors.map(
        (error) => new RenderingBackendError(error.level, error.message),
      );

      let output: Record<string, string> | null = null;
      if (response.output) {
        output = {};
        if (renderGv) {
          output.gv = response.output.dot;
        }
        if (renderDot) {
          output.dot = response.output.dot;
        }
        if (renderSvg) {
          output.svg = response.output.svg;
        }
      }
      response.output = output;
      return { ...response, errors: [...warnings, ...response.errors] };
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

class RenderingBackendError implements RenderError {
  level: 'warning' | 'error';
  message: string;
  location = null;

  constructor(level: 'warning' | 'error', message: string) {
    this.level = level;
    this.message = message;
  }

  toString() {
    return 'RenderingBackendError: ' + this.message;
  }
}
