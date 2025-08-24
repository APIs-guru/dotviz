import { renderInput } from './wrapper.js';

class Viz {
  constructor(module, stderrMessages) {
    this.module = module;
    this._stderrMessages = stderrMessages;
    this._agerrMessages = []; // FIXME: put graphviz errors here
  }

  renderFormats(input, formats, options = {}) {
    return renderInput(this.module, input, formats, {
      engine: 'dot',
      ...options,
    });
  }

  render(input, options = {}) {
    const format = options.format ?? 'dot';

    let result = renderInput(this.module, input, [format], {
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
}

export default Viz;
