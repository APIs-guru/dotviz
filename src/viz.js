import { parseAgerrMessages, parseStderrMessages } from './errors.js';
import { readObjectInput } from './wrapper.js';

class Viz {
  constructor(module, stderrMessages) {
    this.module = module;

    this._stderrMessages = stderrMessages;
    this._agerrMessages = [];
    this._utf8Encoder = new TextEncoder('utf8');
    this._utf8Decoder = new TextDecoder('utf8');
  }

  renderFormats(input, formats, options = {}) {
    return this._renderInput(input, formats, {
      engine: 'dot',
      ...options,
    });
  }

  render(input, options = {}) {
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

  renderString(src, options = {}) {
    const result = this.render(src, options);

    if (result.status !== 'success') {
      throw new Error(
        result.errors.find((e) => e.level == 'error')?.message ||
          'render failed',
      );
    }

    return result.output;
  }

  renderSVGElement(src, options = {}) {
    const str = this.renderString(src, { ...options, format: 'svg' });
    const parser = new DOMParser();
    return parser.parseFromString(str, 'image/svg+xml').documentElement;
  }

  renderJSON(src, options = {}) {
    const str = this.renderString(src, { ...options, format: 'json' });
    return JSON.parse(str);
  }

  _parseErrorMessages() {
    return [
      ...parseAgerrMessages(this._agerrMessages),
      ...parseStderrMessages(this._stderrMessages),
    ];
  }

  _renderInput(input, formats, options) {
    const { exports: wasm } = this.module.instance;
    let graphPointer, contextPointer;

    try {
      // this.stderrMessages = [];

      if (typeof input === 'string') {
        graphPointer = this._withCString(input, (cInput) =>
          wasm.viz_read_one_graph_from_dot(cInput),
        );
      } else if (typeof input === 'object') {
        graphPointer = readObjectInput(this.module, input);
      } else {
        throw new TypeError('input must be a string or object');
      }

      if (graphPointer === 0) {
        return {
          status: 'failure',
          output: undefined,
          errors: this._parseErrorMessages(),
        };
      }

      this._setDefaultAttributes(graphPointer, options);
      wasm.viz_set_y_invert(options.yInvert); //FIXME: test
      wasm.viz_set_reduce(options.reduce); //FIXME: test

      contextPointer = wasm.viz_create_context();

      wasm.viz_reset_errors();

      let layoutError = wasm.viz_layout(contextPointer, graphPointer);

      if (layoutError !== 0) {
        return {
          status: 'failure',
          output: undefined,
          errors: this._parseErrorMessages(),
        };
      }

      const output = {};

      for (let format of formats) {
        let resultPointer = this._withCString(format, (cFormat) =>
          wasm.viz_render(contextPointer, graphPointer, cFormat),
        );
        if (resultPointer === 0) {
          return {
            status: 'failure',
            output: undefined,
            errors: this._parseErrorMessages(),
          };
        }
        output[format] = this._readCString(resultPointer);
        wasm.viz_free_svg(resultPointer);
      }

      return {
        status: 'success',
        output: output,
        errors: this._parseErrorMessages(),
      };
    } catch (error) {
      if (/^exit\(\d+\)/.test(error)) {
        // FIXME: check if needed
        return {
          status: 'failure',
          output: undefined,
          errors: this._parseErrorMessages(),
        };
      } else {
        throw error;
      }
    } finally {
      if (contextPointer && graphPointer) {
        wasm.viz_free_layout(contextPointer, graphPointer);
      }

      if (graphPointer) {
        wasm.viz_free_graph(graphPointer);
      }

      if (contextPointer) {
        wasm.viz_free_context(contextPointer);
      }
    }
  }

  _jsHandleGraphvizError(ptr) {
    this._agerrMessages.push(this._readCString(ptr));
  }

  // FIXME: handle HTML strings separately
  _withCString(src, fn) {
    const { exports: wasm } = this.module.instance;
    let inputBuf;

    try {
      const cString = this._utf8Encoder.encode(src + '\0');
      const inputPtr = wasm.wasm_alloc(cString.length);
      inputBuf = new Uint8Array(wasm.memory.buffer, inputPtr, cString.length);
      inputBuf.set(cString);
      return fn(inputBuf.byteOffset);
    } finally {
      if (inputBuf) {
        wasm.wasm_free(inputBuf.byteOffset, inputBuf.length);
      }
    }
  }

  _readCString(ptr) {
    const { exports: wasm } = this.module.instance;

    const buf = new Uint8Array(wasm.memory.buffer);
    let end = ptr;
    while (buf[end] !== 0) {
      end++;
    }
    return this._utf8Decoder.decode(buf.subarray(ptr, end));
  }

  _setDefaultAttributes(graphPointer, data) {
    const { exports: wasm } = this.module.instance;

    if (data.graphAttributes) {
      for (const [name, value] of Object.entries(data.graphAttributes)) {
        this._withCString(name, (cName) => {
          this._withCString(value, (cValue) => {
            wasm.viz_set_default_graph_attribute(graphPointer, cName, cValue);
          });
        });
      }
    }

    if (data.nodeAttributes) {
      for (const [name, value] of Object.entries(data.nodeAttributes)) {
        this._withCString(name, (cName) => {
          this._withCString(value, (cValue) => {
            wasm.viz_set_default_node_attribute(graphPointer, cName, cValue);
          });
        });
      }
    }

    if (data.edgeAttributes) {
      for (const [name, value] of Object.entries(data.edgeAttributes)) {
        this._withCString(name, (cName) => {
          this._withCString(value, (cValue) => {
            wasm.viz_set_default_edge_attribute(graphPointer, cName, cValue);
          });
        });
      }
    }
  }
}

export default Viz;
