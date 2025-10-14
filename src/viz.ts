import { parseAgerrMessages, parseStderrMessages } from './errors.ts';
import type { Attributes, Graph } from './graph.d.ts';

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
 * Image sizes to use when rendering nodes with <code>image</code> attributes.
 *
 * For example, to indicate to Graphviz that the image <code>test.png</code> has size 300x200:
 *
 * ```js
 * viz.render("graph { a[image=\"test.png\"] }", {
 *   images: [
 *     { name: "test.png", width: 300, height: 200 }
 *   ]
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
  images?: ImageSize[];
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
  output: undefined;
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
  level: 'error' | 'warning' | undefined;
  message: string;
}

/**
 * Specifies the size of an image used as a node's `image` attribute. See {@link RenderOptions.images}.
 *
 * `width` and `height` may be specified as numbers or strings with units: in, px, pc, pt, cm, or mm. If no units are given or measurements are given as numbers, points (pt) are used.
 *
 * @property name
 * The name of the image. In addition to filenames, names that look like absolute filesystem paths or URLs can be used. For example:
 *
 * - `"example.png"`
 * - `"/images/example.png"`
 * - `"http://example.com/image.png"`
 *
 * Names that look like relative filesystem paths, such as `"../example.png"`, are not supported.
 *
 * @property width
 * The width of the image.
 *
 * @property height
 * The height of the image.
 */
export interface ImageSize {
  name: string;
  width: string | number;
  height: string | number;
}

/**
 * The {@link Viz} class isn't exported, but it can be instantiated using the {@link instance} function.
 */
class Viz {
  _stdoutBuf = '';
  _stderrBuf = '';
  _stderrMessages: string[] = [];
  _agerrMessages: string[] = [];
  _utf8Encoder: TextEncoder = new TextEncoder();
  _utf8Decoder: TextDecoder = new TextDecoder('utf8');
  _wasm: {
    memory: Uint8Array;
    viz_json_to_graph(jsonPtr: number, jsonLength: number): number;
    viz_set_default_graph_attribute(
      graphPtr: number,
      namePtr: number,
      valuePtr: number,
      isHTML: boolean,
    ): void;
    viz_set_default_node_attribute(
      graphPtr: number,
      namePtr: number,
      valuePtr: number,
      isHTML: boolean,
    ): void;
    viz_set_default_edge_attribute(
      graphPtr: number,
      namePtr: number,
      valuePtr: number,
      isHTML: boolean,
    ): void;

    viz_read_one_graph_from_dot(inputPtr: number): number;
    viz_set_y_invert(value: boolean): void;
    viz_set_reduce(value: boolean): void;
    viz_create_context(): number;
    viz_reset_errors(): void;
    viz_layout(ctxPtr: number, graphPtr: number): number;
    viz_render(ctxPtr: number, graphPtr: number, formatPtr: number): number;
    viz_free_svg(resultPtr: number): void;
    viz_free_layout(graphPtr: number): void;
    viz_free_graph(graphPtr: number): void;
    viz_free_context(ctxPtr: number): void;
    wasm_alloc(length: number): number;
    wasm_free(ptr: number, length: number): void;
  };

  /**
   * @internal
   */
  constructor(module: WebAssembly.WebAssemblyInstantiatedSource) {
    // @ts-expect-error not sure how to properly type it
    this._wasm = module.instance.exports;
  }

  /**
   * Renders the graph described by the input for each format in `formats` and returns the result as an object. For a successful result, `output` is an object keyed by format.
   */
  renderFormats(
    input: string | Graph,
    formats: readonly string[],
    options: RenderOptions = {},
  ): MultipleRenderResult {
    return this._renderInput(input, formats, {
      engine: 'dot',
      ...options,
    });
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

    const result = this._renderInput(input, [format], {
      engine: 'dot',
      ...options,
    });

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
      throw new Error(
        result.errors.find((e) => e.level == 'error')?.message ??
          'render failed',
      );
    }

    return result.output;
  }

  _parseErrorMessages(): RenderError[] {
    return [
      ...parseAgerrMessages(this._agerrMessages),
      ...parseStderrMessages(this._stderrMessages),
    ];
  }

  _renderInput(
    input: string | Graph,
    formats: readonly string[],
    options: RenderOptions,
  ): MultipleRenderResult {
    let graphPointer = 0;
    let contextPointer = 0;

    this._agerrMessages = [];
    this._stderrMessages = [];

    try {
      if (typeof input === 'string') {
        graphPointer = this._withCString(input, (cInput) =>
          this._wasm.viz_read_one_graph_from_dot(cInput),
        );
      } else {
        graphPointer = this._readObjectInput(input);
      }

      if (graphPointer === 0) {
        return {
          status: 'failure',
          output: undefined,
          errors: this._parseErrorMessages(),
        };
      }

      this._setDefaultAttributes(graphPointer, options);
      this._wasm.viz_set_y_invert(options.yInvert ?? false); //FIXME: test
      this._wasm.viz_set_reduce(options.reduce ?? false); //FIXME: test

      contextPointer = this._wasm.viz_create_context();

      this._wasm.viz_reset_errors();

      const layoutError = this._wasm.viz_layout(contextPointer, graphPointer);

      if (layoutError !== 0) {
        return {
          status: 'failure',
          output: undefined,
          errors: this._parseErrorMessages(),
        };
      }

      const output: Record<string, string> = {};

      for (const format of formats) {
        const resultPointer = this._withCString(format, (cFormat) =>
          this._wasm.viz_render(contextPointer, graphPointer, cFormat),
        );
        if (resultPointer === 0) {
          return {
            status: 'failure',
            output: undefined,
            errors: this._parseErrorMessages(),
          };
        }
        output[format] = this._readCString(resultPointer);
        this._wasm.viz_free_svg(resultPointer);
      }

      return {
        status: 'success',
        output: output,
        errors: this._parseErrorMessages(),
      };
    } catch (error) {
      // @ts-expect-error check if this code needed
      if (/^exit\(\d+\)/.test(error)) {
        return {
          status: 'failure',
          output: undefined,
          errors: this._parseErrorMessages(),
        };
      } else {
        throw error;
      }
    } finally {
      if (contextPointer != 0 && graphPointer != 0) {
        this._wasm.viz_free_layout(graphPointer);
      }

      if (graphPointer != 0) {
        this._wasm.viz_free_graph(graphPointer);
      }

      if (contextPointer != 0) {
        this._wasm.viz_free_context(contextPointer);
      }
    }
  }

  _readObjectInput(object: Graph): number {
    const json = JSON.stringify(object);
    let jsonBuf;
    try {
      const cJson = this._utf8Encoder.encode(json);
      const jsonPtr = this._wasm.wasm_alloc(cJson.length);
      jsonBuf = new Uint8Array(this._wasm.memory.buffer, jsonPtr, cJson.length);
      jsonBuf.set(cJson);
      return this._wasm.viz_json_to_graph(jsonBuf.byteOffset, jsonBuf.length);
    } finally {
      if (jsonBuf) {
        this._wasm.wasm_free(jsonBuf.byteOffset, jsonBuf.length);
      }
    }
  }

  _jsHandleGraphvizError(ptr: number): void {
    this._agerrMessages.push(this._readCString(ptr));
  }

  // FIXME: handle HTML strings separately
  _withCString<T>(src: string, fn: (ptr: number) => T): T {
    let inputBuf;
    try {
      const cString = this._utf8Encoder.encode(src + '\0');
      const inputPtr = this._wasm.wasm_alloc(cString.length);
      inputBuf = new Uint8Array(
        this._wasm.memory.buffer,
        inputPtr,
        cString.length,
      );
      inputBuf.set(cString);
      return fn(inputBuf.byteOffset);
    } finally {
      if (inputBuf) {
        this._wasm.wasm_free(inputBuf.byteOffset, inputBuf.length);
      }
    }
  }

  _readCString(ptr: number): string {
    const buf = new Uint8Array(this._wasm.memory.buffer);
    let end = ptr;
    while (buf[end] !== 0) {
      end++;
    }
    return this._utf8Decoder.decode(buf.subarray(ptr, end));
  }

  _setDefaultAttributes(graphPointer: number, data: Graph): void {
    if (data.graphAttributes) {
      for (const [name, value] of Object.entries(data.graphAttributes)) {
        this._withCString(name, (cName) => {
          const isHTML = typeof value === 'object' && 'html' in value;
          this._withCString(
            isHTML ? value.html : value.toString(),
            (cValue) => {
              this._wasm.viz_set_default_graph_attribute(
                graphPointer,
                cName,
                cValue,
                isHTML,
              );
            },
          );
        });
      }
    }

    if (data.nodeAttributes) {
      for (const [name, value] of Object.entries(data.nodeAttributes)) {
        this._withCString(name, (cName) => {
          const isHTML = typeof value === 'object' && 'html' in value;
          this._withCString(
            isHTML ? value.html : value.toString(),
            (cValue) => {
              this._wasm.viz_set_default_node_attribute(
                graphPointer,
                cName,
                cValue,
                isHTML,
              );
            },
          );
        });
      }
    }

    if (data.edgeAttributes) {
      for (const [name, value] of Object.entries(data.edgeAttributes)) {
        this._withCString(name, (cName) => {
          const isHTML = typeof value === 'object' && 'html' in value;
          this._withCString(
            isHTML ? value.html : value.toString(),
            (cValue) => {
              this._wasm.viz_set_default_edge_attribute(
                graphPointer,
                cName,
                cValue,
                isHTML,
              );
            },
          );
        });
      }
    }
  }

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
      default: {
        console.trace(`fd_write: unknown fd ${fd.toString()}`);
        return 52; // WASI_ERRNO_NOTSUP
      }
    }

    view.setUint32(nwritten_ptr, totalWritten, true);
    return 0;
  }
}

export default Viz;
