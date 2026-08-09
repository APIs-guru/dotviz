import type { Attributes, Graph } from './graph.d.ts';
import { type Location } from './location.ts';
import {
  NormalizedAttributes,
  type NormalizedGraph,
  normalizeGraph,
} from './normalize-graph.ts';
import { parseDot, parseDotNumber, parseDotNumberWithUnits } from './parser.ts';
import { formatValueForDiagnostics } from './utils.ts';

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
export function isLayoutEngine(value: string): value is LayoutEngine {
  return layoutEngines.includes(value as LayoutEngine);
}

const MIN_dotOutputMaxLineLength = 60;
const MAX_dotOutputMaxLineLength = 128;
function isMaxLineLength(value: number): boolean {
  if (value === 0) {
    return true;
  }

  return (
    MIN_dotOutputMaxLineLength <= value && value <= MAX_dotOutputMaxLineLength
  );
}

/** The {@link Viz} class isn't exported, but it can be instantiated using the {@link instance} function. */
export class Viz {
  #stdoutBuf = '';
  #stderrBuf = '';
  #utf8Encoder: TextEncoder = new TextEncoder();
  #utf8Decoder: TextDecoder = new TextDecoder('utf-8');
  #wasm: {
    memory: Uint8Array;
    wasm_alloc(length: number): number;
    wasm_free(ptr: number, length: number): void;
    render(jsonPtr: number, jsonLength: number): bigint;
  };
  /**
   * @internal
   */
  constructor(moduleInstance: WebAssembly.Instance) {
    // @ts-expect-error not sure how to properly type it
    this.#wasm = moduleInstance.exports;
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
    const graphParseResults = parseDot(input, options.overrideAttributes ?? {});
    if (graphParseResults.length === 0) {
      return failureResult([
        new RenderingBackendError(
          "Missing graph definition. Start your file with 'graph {}' or 'digraph {}'.",
        ),
      ]);
    }

    const diagnostics: Diagnostic[] = graphParseResults.flatMap(
      (result) => result.diagnostics,
    );
    if (graphParseResults.length > 1) {
      diagnostics.push(
        new RenderingBackendWarning(
          'Multiple graphs found. Using the first one.',
        ),
      );
    }

    const { graph } = graphParseResults[0];
    if (graph === undefined) {
      return failureResult(diagnostics);
    }
    const result = this.#renderNormalizedGraph(graph, options);
    return {
      ...result,
      diagnostics: [...diagnostics, ...result.diagnostics],
    };
  }

  // oxlint-disable-next-line complexity
  #renderNormalizedGraph(
    graph: NormalizedGraph,
    options: RenderOptions,
  ): RenderResult {
    let { engine } = options;

    const background = graph.graphAttributes.get('_background');
    if (background !== undefined) {
      return failureResult([
        new RenderingBackendError(
          `'_background' is not supported. If you need it, open an issue: https://github.com/APIs-guru/dotviz/issues`,
        ),
      ]);
    }

    const layout = graph.graphAttributes.get('layout');
    if (layout !== undefined) {
      if (layout.html !== undefined || !isLayoutEngine(layout.text)) {
        const value = NormalizedAttributes.valueToString(layout);
        return failureResult([
          new RenderingBackendError(
            `Layout type: ${value} not recognized. Use one of: ${layoutEngines.join(' ')}`,
          ),
        ]);
      }

      if (engine !== undefined && engine !== layout.text) {
        const layoutValue = formatValueForDiagnostics(layout.text);
        const engineValue = formatValueForDiagnostics(engine);
        return failureResult([
          new RenderingBackendError(
            `Engine mismatch: layout attribute in graph ("${layoutValue}") conflicts with engine option ("${engineValue}"). Remove one or make them match.`,
          ),
        ]);
      }
      engine = layout.text;
    }

    const charset = graph.graphAttributes.get('charset');
    if (
      charset !== undefined &&
      (charset.text === undefined ||
        !['utf8', 'utf-8'].includes(charset.text.toLowerCase()))
    ) {
      const value = NormalizedAttributes.valueToString(charset);
      return failureResult([
        new RenderingBackendError(
          `Unsupported charset: ${value}. Only 'utf-8' and 'utf8' are supported.`,
        ),
      ]);
    }

    const formats = options.formats ?? ['dot'];
    const renderDot = formats.includes('dot');
    const renderSvg = formats.includes('svg');

    const diagnostics: Diagnostic[] = [];
    if (renderDot && graph.graphAttributes.get('layers') !== undefined) {
      diagnostics.push(
        new RenderingBackendWarning('layers not supported in dot output'),
      );
    }

    let dotOutputMaxLineLength = MAX_dotOutputMaxLineLength;
    const linelengthValue = graph.graphAttributes.get('linelength');
    if (linelengthValue !== undefined) {
      const number = parseDotNumber(linelengthValue);
      if (!Number.isInteger(number) || !isMaxLineLength(number)) {
        return failureResult([
          new RenderingBackendError(
            `linelength must be '0' or an integer in the [${MIN_dotOutputMaxLineLength.toString()}, ${MAX_dotOutputMaxLineLength.toString()}] range`,
          ),
        ]);
      }
      dotOutputMaxLineLength = number;
    }

    const images: Record<string, { heightPt: number; widthPt: number }> = {};
    if (options.images !== undefined) {
      for (const [name, { height, width }] of Object.entries(options.images)) {
        const heightPt = convertImageSizeToPoints(height);
        if (Number.isNaN(heightPt)) {
          const value = formatValueForDiagnostics(height.toString());
          return failureResult([
            new RenderingBackendError(
              `Invalid height for image "${name}": "${value}". Use a number or a string with one of these units: in, px, pc, pt, cm, or mm.`,
            ),
          ]);
        }

        const widthPt = convertImageSizeToPoints(width);
        if (Number.isNaN(widthPt)) {
          const value = formatValueForDiagnostics(width.toString());
          return failureResult([
            new RenderingBackendError(
              `Invalid width for image "${name}": "${value}". Use a number or a string with one of these units: in, px, pc, pt, cm, or mm.`,
            ),
          ]);
        }
        images[name] = { heightPt, widthPt };
      }
    }

    const request = {
      graph: serializeGraph(graph),
      engine: engine ?? 'dot',
      yInvert: options.yInvert ?? false,
      reduce: options.reduce ?? false,
      images,
      renderSvg,
      renderDot,
      dotOutputMaxLineLength,
    };
    const requestJSON = JSON.stringify(request);
    const cJson = this.#utf8Encoder.encode(requestJSON);
    const jsonPtr = this.#wasm.wasm_alloc(cJson.length);
    const inputJSONBuf = new Uint8Array(
      this.#wasm.memory.buffer,
      jsonPtr,
      cJson.length,
    );
    inputJSONBuf.set(cJson);
    const sliceU64 = this.#wasm.render(
      inputJSONBuf.byteOffset,
      inputJSONBuf.length,
    );
    const ptr = Number(BigInt.asUintN(32, sliceU64));
    const len = Number(BigInt.asUintN(32, sliceU64 >> 32n));
    const outputJSONBuf = new Uint8Array(this.#wasm.memory.buffer, ptr, len);
    try {
      const str: string = this.#utf8Decoder.decode(outputJSONBuf);
      const response = JSON.parse(str) as RenderResult;

      for (const error of response.diagnostics) {
        diagnostics.push(
          error.level === 'warning'
            ? new RenderingBackendWarning(error.message)
            : new RenderingBackendError(error.message),
        );
      }

      if (response.status === 'failure') {
        return { status: 'failure', output: undefined, diagnostics };
      }

      const svg = response.output.svg ?? undefined;
      let dot = response.output.dot ?? undefined;
      if (dot !== undefined && graph.strict) {
        dot = 'strict ' + dot;
      }
      return { status: 'success', diagnostics, output: { dot, svg } };
    } finally {
      this.#wasm.wasm_free(outputJSONBuf.byteOffset, outputJSONBuf.length);
    }
  }

  /* v8 ignore next -- used only for debugging */
  _wasi_fd_write(
    fd: number,
    iovs_ptr: number,
    iovs_len: number,
    nwritten_ptr: number,
  ): number {
    const mem = new Uint8Array(this.#wasm.memory.buffer);
    const view = new DataView(this.#wasm.memory.buffer);

    let totalWritten = 0;

    let bufferStr = '';
    for (let i = 0; i < iovs_len; i++) {
      const base = view.getUint32(iovs_ptr + i * 8, true);
      const len = view.getUint32(iovs_ptr + i * 8 + 4, true);
      const chunk = mem.subarray(base, base + len);
      bufferStr += this.#utf8Decoder.decode(chunk);

      totalWritten += len;
    }

    switch (fd) {
      case 1: {
        const lines = (this.#stdoutBuf + bufferStr).split('\n');
        this.#stdoutBuf = lines.pop() ?? '';
        for (const line of lines) {
          console.info(line);
        }
        break;
      }
      case 2: {
        const lines = (this.#stderrBuf + bufferStr).split('\n');
        this.#stderrBuf = lines.pop() ?? '';
        for (const line of lines) {
          console.error(line);
        }
        break;
      }
      default:
        console.error(`fd_write: unknown fd ${fd.toString()}`);
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

// NOTE: could return NaN if parseDotNumberWithUnits fails
function convertImageSizeToPoints(value: string | number): number {
  if (typeof value === 'number') {
    return value;
  }

  const [n, units] = parseDotNumberWithUnits(value);
  if (Number.isNaN(n)) {
    return n;
  }

  const POINTS_PER_PICAS = 12;
  const POINTS_PER_INCH = 72;
  const CSS_POINTS_PER_INCH = 96;
  const MM_PER_INCH = 0.0393700787;
  switch (units) {
    case '':
    case 'pt':
      return Math.round(n);
    case 'px':
      return Math.round((n * POINTS_PER_INCH) / CSS_POINTS_PER_INCH);
    case 'pc':
      return Math.round(n * POINTS_PER_PICAS);
    case 'in':
      return Math.round(n * POINTS_PER_INCH);
    case 'cm':
      return Math.round(n * 10 * MM_PER_INCH * POINTS_PER_INCH);
    case 'mm':
      return Math.round(n * MM_PER_INCH * POINTS_PER_INCH);
  }
  return NaN;
}

function serializeGraph(graph: NormalizedGraph): unknown {
  return {
    name: graph.name,
    directed: graph.directed,
    graphAttributes: serializeAttributes(graph.graphAttributes),
    nodeAttributes: serializeAttributes(graph.nodeAttributes),
    edgeAttributes: serializeAttributes(graph.edgeAttributes),
    allNodes: graph.allNodes.map((node) => ({
      name: node.name,
      ports: node.ports.map((port) => port.name),
      attributes: serializeAttributes(node.attributes),
    })),
    allEdges: graph.allEdges.map((edge) => ({
      tail: edge.tail,
      head: edge.head,
      key: edge.key,
      attributes: serializeAttributes(edge.attributes),
    })),
    allSubgraphs: graph.allSubgraphs.map((subgraph) => ({
      name: subgraph.name,
      graphAttributes: serializeAttributes(subgraph.graphAttributes),
      nodeAttributes: serializeAttributes(subgraph.nodeAttributes),
      edgeAttributes: serializeAttributes(subgraph.edgeAttributes),
      memberNodes: subgraph.sortedMemberNodeIndexes(),
      memberEdges: subgraph.sortedMemberEdgeIndexes(),
      subgraphs: subgraph.subgraphs,
    })),
    subgraphs: graph.subgraphs,
  };
}

function serializeAttributes(attributes: NormalizedAttributes): unknown {
  return Object.fromEntries(
    attributes.entries().map(([name, value]) => [name, value ?? null]),
  );
}
