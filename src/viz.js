import {
  readStringInput,
  readObjectInput,
  setDefaultAttributes,
} from './wrapper.js';
import { parseAgerrMessages, parseStderrMessages } from './errors.js';

class Viz {
  constructor(module, stderrMessages) {
    this.module = module;

    this._stderrMessages = stderrMessages;
    this._agerrMessages = [];
  }

  renderFormats(input, formats, options = {}) {
    return this._renderInput(input, formats, {
      engine: 'dot',
      ...options,
    });
  }

  render(input, options = {}) {
    const format = options.format ?? 'dot';

    let result = this._renderInput(input, [format], {
      engine: 'dot',
      ...options,
    });

    if (result.status === 'success') {
      result.output = result.output[format];
    }

    return result;
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
    let graphPointer, contextPointer, resultPointer;

    try {
      // this.stderrMessages = [];

      if (typeof input === 'string') {
        graphPointer = readStringInput(this.module, input);
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

      setDefaultAttributes(this.module, graphPointer, options);
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

      let resultPointer = wasm.viz_render(contextPointer, graphPointer);
      if (resultPointer === 0) {
        return {
          status: 'failure',
          output: undefined,
          errors: this._parseErrorMessages(),
        };
      }

      const output = readCString(wasm.memory, resultPointer);
      wasm.viz_free_svg(resultPointer);
      resultPointer = 0;
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
    const { exports: wasm } = this.module.instance;
    this._agerrMessages.push(readCString(wasm.memory, ptr));
  }
}

export default Viz;

function readCString(memory, ptr) {
  const buf = new Uint8Array(memory.buffer);
  let end = ptr;
  while (buf[end] !== 0) {
    end++;
  }
  return new TextDecoder('utf-8').decode(buf.subarray(ptr, end));
}
