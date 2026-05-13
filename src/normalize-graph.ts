/* eslint-disable unicorn/no-null */
import type { Attributes, Graph, Subgraph } from './graph.d.ts';
import type { OverrideAttributes } from './viz.ts';

interface NormalizedGraphConfig {
  readonly name: string | undefined;
  readonly strict: boolean;
  readonly directed: boolean;
  readonly graphAttributes: Readonly<NormalizedAttributes>;
  readonly nodeAttributes: Readonly<NormalizedAttributes>;
  readonly edgeAttributes: Readonly<NormalizedAttributes>;
}

export class NormalizedGraph {
  readonly #overrideGraphAttributes: Readonly<NormalizedAttributes>;
  readonly #overrideNodeAttributes: Readonly<NormalizedAttributes>;
  readonly #overrideEdgeAttributes: Readonly<NormalizedAttributes>;
  readonly owner = undefined;
  readonly root = this;

  readonly name: string | undefined;
  readonly strict: boolean;
  readonly directed: boolean;
  graphAttributes: Readonly<NormalizedAttributes>;
  nodeAttributes: Readonly<NormalizedAttributes>;
  edgeAttributes: Readonly<NormalizedAttributes>;
  readonly #allNodes = new Map<string, NormalizedNode>();
  readonly #allEdges = new Map<string | number, NormalizedEdge>();
  readonly #subgraphs = new Map<string | number, NormalizedSubgraph>();

  constructor(
    config: NormalizedGraphConfig,
    overrideAttributes: OverrideAttributes,
  ) {
    this.#overrideGraphAttributes = normalizeAttributes(
      overrideAttributes.graphAttributes,
    );
    this.#overrideNodeAttributes = normalizeAttributes(
      overrideAttributes.nodeAttributes,
    );
    this.#overrideEdgeAttributes = normalizeAttributes(
      overrideAttributes.edgeAttributes,
    );
    this.graphAttributes = new NormalizedAttributes([
      ...config.graphAttributes,
      ...this.#overrideGraphAttributes,
    ]);
    this.nodeAttributes = new NormalizedAttributes([
      ...config.nodeAttributes,
      ...this.#overrideNodeAttributes,
    ]);
    this.edgeAttributes = new NormalizedAttributes([
      ...config.edgeAttributes,
      ...this.#overrideEdgeAttributes,
    ]);

    this.name = config.name;
    this.strict = config.strict;
    this.directed = config.directed;
  }

  mergeGraphAttributes(newAttributes: NormalizedAttributes) {
    const defaultAttributes = new NormalizedAttributes();
    for (const key of newAttributes.keys()) {
      if (!this.#overrideGraphAttributes.has(key)) {
        // Seed with the current value (or `undefined`) so existing subgraphs retain whatever was in effect before this change.
        defaultAttributes.set(key, this.graphAttributes.get(key));
      }
    }
    for (const subgraph of this.#subgraphs.values()) {
      subgraph.applyDefaultGraphAttributes(defaultAttributes);
    }

    this.graphAttributes = new NormalizedAttributes([
      ...this.graphAttributes,
      ...newAttributes,
      ...this.#overrideGraphAttributes,
    ]);
  }

  mergeNodeAttributes(newAttributes: NormalizedAttributes) {
    const defaultAttributes = new NormalizedAttributes();
    for (const key of newAttributes.keys()) {
      if (!this.nodeAttributes.has(key)) {
        // Seed existing nodes with `undefined` so a later-declared default doesn't retroactively win
        defaultAttributes.set(key, undefined);
      }
    }

    for (const node of this.#allNodes.values()) {
      node.applyDefaultAttributes(defaultAttributes);
    }

    this.nodeAttributes = new NormalizedAttributes([
      ...this.nodeAttributes,
      ...newAttributes,
      ...this.#overrideNodeAttributes,
    ]);
  }

  mergeEdgeAttributes(newAttributes: NormalizedAttributes) {
    const defaultAttributes = new NormalizedAttributes();
    for (const key of newAttributes.keys()) {
      if (!this.edgeAttributes.has(key)) {
        // Seed existing edges with `undefined` so a later-declared default doesn't retroactively win
        defaultAttributes.set(key, undefined);
      }
    }

    for (const edge of this.#allEdges.values()) {
      edge.applyDefaultAttributes(defaultAttributes);
    }
    this.edgeAttributes = new NormalizedAttributes([
      ...this.edgeAttributes,
      ...newAttributes,
      ...this.#overrideEdgeAttributes,
    ]);
  }

  upsertNode(
    owner: NormalizedGraph | NormalizedSubgraph,
    config: NormalizedNodeConfig,
  ): NormalizedNode {
    const { name } = config;
    const node = this.#allNodes.get(name);
    if (node !== undefined) {
      owner.addNode(node);
      node.mergeAttributes(config.attributes);
      return node;
    }

    const newNode = new NormalizedNode(this.#allNodes.size, {
      name,
      attributes: new NormalizedAttributes([
        ...owner.resolvedNodeDefaults,
        ...config.attributes,
      ]),
    });
    owner.addNode(newNode);
    return newNode;
  }

  upsertEdge(
    owner: NormalizedGraph | NormalizedSubgraph,
    config: NormalizedEdgeConfig,
  ): NormalizedEdge {
    const newEdge = new NormalizedEdge(
      this.#allEdges.size,
      applyAttributesToEdgeConfig({
        tail: config.tail,
        head: config.head,
        key: config.key,
        attributes: new NormalizedAttributes([
          ...owner.resolvedEdgeDefaults,
          ...config.attributes,
        ]),
      }),
    );

    const deduplicateKey = this.#edgeDeduplicateKey(newEdge);
    if (deduplicateKey !== undefined) {
      const edge = this.#allEdges.get(deduplicateKey);
      if (edge !== undefined) {
        owner.addEdge(edge);
        edge.mergeAttributes(applyAttributesToEdgeConfig(config).attributes);
        return edge;
      }
    }

    owner.addEdge(newEdge);
    return newEdge;
  }

  addNode(node: NormalizedNode): void {
    this.#allNodes.set(node.name, node);
  }

  addEdge(edge: NormalizedEdge): void {
    const deduplicateKey = this.#edgeDeduplicateKey(edge);
    this.#allEdges.set(deduplicateKey ?? edge.index, edge);
  }

  #edgeDeduplicateKey(edge: NormalizedEdge): string | undefined {
    const { key } = edge;
    if (key === undefined && !this.strict) {
      return undefined;
    }

    let { tail, head } = edge;
    if (!this.directed) {
      const shouldSwap =
        head.port.node.index > tail.port.node.index ||
        (head.port.node.index === tail.port.node.index &&
          head.port.index > tail.port.index);
      if (shouldSwap) {
        [tail, head] = [head, tail];
      }
    }

    return [
      tail.port.node.index.toString(),
      tail.port.index.toString(),
      head.port.node.index.toString(),
      head.port.index.toString(),
      key ?? '',
    ].join(':');
  }

  upsertSubgraph(config: NormalizedSubgraphConfig): NormalizedSubgraph {
    const { name } = config;
    if (name !== undefined) {
      const subgraph = this.#subgraphs.get(name);
      if (subgraph) {
        return subgraph;
      }
    }

    const key = name ?? this.#subgraphs.size;
    const newSubgraph = new NormalizedSubgraph(this, config);
    this.#subgraphs.set(key, newSubgraph);
    return newSubgraph;
  }

  get resolvedNodeDefaults(): NormalizedAttributes {
    return this.nodeAttributes;
  }

  get resolvedEdgeDefaults(): NormalizedAttributes {
    return this.edgeAttributes;
  }

  getAllNodes(): NormalizedNode[] {
    return [...this.#allNodes.values()];
  }

  getAllEdges(): NormalizedEdge[] {
    return [...this.#allEdges.values()];
  }

  getSubgraphs(): NormalizedSubgraph[] {
    return [...this.#subgraphs.values()];
  }

  toJSON() {
    return {
      name: this.name,
      strict: this.strict,
      directed: this.directed,
      graphAttributes: this.graphAttributes,
      nodeAttributes: this.nodeAttributes,
      edgeAttributes: this.edgeAttributes,
      allNodes: this.getAllNodes(),
      allEdges: this.getAllEdges(),
      subgraphs: this.getSubgraphs(),
    };
  }
}

export interface NormalizedPortConfig {
  readonly node: NormalizedNode;
  readonly name: string | undefined;
}

export class NormalizedPort {
  readonly index: number;
  readonly node: NormalizedNode;
  readonly name: string | undefined;

  constructor(index: number, config: NormalizedPortConfig) {
    this.index = index;
    this.node = config.node;
    this.name = config.name;
  }

  toJSON() {
    return { node: this.node.index, name: this.name };
  }
}

export interface NormalizedNodeConfig {
  readonly name: string;
  readonly attributes: NormalizedAttributes;
}

export class NormalizedNode {
  readonly index: number;
  readonly name: string;
  readonly defaultPort = new NormalizedPort(0, { node: this, name: undefined });
  readonly defaultEndpoint: NormalizedEdgeEndpoint = {
    port: this.defaultPort,
    compass: undefined,
  };
  readonly ports = new Map<string | undefined, NormalizedPort>([
    [undefined, this.defaultPort],
  ]);
  attributes: NormalizedAttributes;

  constructor(index: number, config: NormalizedNodeConfig) {
    this.index = index;
    this.name = config.name;
    this.attributes = config.attributes;
  }

  mergeAttributes(newAttributes: NormalizedAttributes): void {
    this.attributes = new NormalizedAttributes([
      ...this.attributes,
      ...newAttributes,
    ]);
  }

  applyDefaultAttributes(defaults: NormalizedAttributes): void {
    this.attributes = new NormalizedAttributes([
      ...defaults,
      ...this.attributes,
    ]);
  }

  upsertPort(name: string): NormalizedPort {
    const port = this.ports.get(name);
    if (port) return port;

    const newPort = new NormalizedPort(this.ports.size, { node: this, name });
    this.ports.set(name, newPort);
    return newPort;
  }

  toJSON() {
    return {
      name: this.name,
      attributes: this.attributes,
    };
  }
}

export interface NormalizedEdgeEndpoint {
  readonly port: NormalizedPort;
  readonly compass: string | undefined;
}

interface NormalizedEdgeConfig {
  readonly tail: NormalizedEdgeEndpoint;
  readonly head: NormalizedEdgeEndpoint;
  readonly key: string | undefined;
  readonly attributes: NormalizedAttributes;
}

export class NormalizedEdge {
  readonly index: number;
  readonly tail: NormalizedEdgeEndpoint;
  readonly head: NormalizedEdgeEndpoint;
  readonly key: string | undefined;
  attributes: NormalizedAttributes;

  constructor(index: number, config: NormalizedEdgeConfig) {
    this.index = index;
    this.tail = config.tail;
    this.head = config.head;
    this.key = config.key;
    this.attributes = config.attributes;
  }

  mergeAttributes(newAttributes: NormalizedAttributes) {
    this.attributes = new NormalizedAttributes([
      ...this.attributes,
      ...newAttributes,
    ]);
  }

  applyDefaultAttributes(defaults: NormalizedAttributes) {
    this.attributes = new NormalizedAttributes([
      ...defaults,
      ...this.attributes,
    ]);
  }

  toJSON() {
    return {
      tail: this.tail,
      head: this.head,
      key: this.key,
      attributes: this.attributes,
    };
  }
}

interface NormalizedSubgraphConfig {
  readonly name: string | undefined;
  readonly graphAttributes: NormalizedAttributes;
  readonly nodeAttributes: NormalizedAttributes;
  readonly edgeAttributes: NormalizedAttributes;
}

export class NormalizedSubgraph {
  readonly root: NormalizedGraph;
  readonly owner: NormalizedGraph | NormalizedSubgraph;

  readonly name: string | undefined;
  graphAttributes: NormalizedAttributes;
  nodeAttributes: NormalizedAttributes;
  edgeAttributes: NormalizedAttributes;
  readonly #memberNodes = new Set<NormalizedNode>();
  readonly #memberEdges = new Set<NormalizedEdge>();
  readonly #subgraphs = new Map<string | number, NormalizedSubgraph>();

  constructor(
    owner: NormalizedGraph | NormalizedSubgraph,
    config: NormalizedSubgraphConfig,
  ) {
    this.root = owner.root;
    this.owner = owner;
    this.name = config.name;
    this.graphAttributes = config.graphAttributes;
    this.nodeAttributes = config.nodeAttributes;
    this.edgeAttributes = config.edgeAttributes;
  }

  mergeGraphAttributes(newAttributes: NormalizedAttributes): void {
    const defaultAttributes = new NormalizedAttributes();
    for (const key of newAttributes.keys()) {
      defaultAttributes.set(key, this.graphAttributes.get(key));
    }
    for (const subgraph of this.#subgraphs.values()) {
      subgraph.applyDefaultGraphAttributes(defaultAttributes);
    }

    this.graphAttributes = new NormalizedAttributes([
      ...this.graphAttributes,
      ...newAttributes,
    ]);
  }

  mergeNodeAttributes(newAttributes: NormalizedAttributes): void {
    this.nodeAttributes = new NormalizedAttributes([
      ...this.nodeAttributes,
      ...newAttributes,
    ]);
  }

  mergeEdgeAttributes(newAttributes: NormalizedAttributes): void {
    this.edgeAttributes = new NormalizedAttributes([
      ...this.edgeAttributes,
      ...newAttributes,
    ]);
  }

  applyDefaultGraphAttributes(defaults: NormalizedAttributes): void {
    this.graphAttributes = new NormalizedAttributes([
      ...defaults,
      ...this.graphAttributes,
    ]);
  }

  addNode(node: NormalizedNode): void {
    this.owner.addNode(node);
    this.#memberNodes.add(node);
  }

  addEdge(edge: NormalizedEdge): void {
    this.owner.addEdge(edge);
    this.#memberEdges.add(edge);
  }

  upsertSubgraph(
    config: Readonly<NormalizedSubgraphConfig>,
  ): NormalizedSubgraph {
    const { name } = config;
    if (name !== undefined) {
      const subgraph = this.#subgraphs.get(name);
      if (subgraph) {
        return subgraph;
      }
    }

    const key = name ?? this.#subgraphs.size;
    const newSubgraph = new NormalizedSubgraph(this, config);
    this.#subgraphs.set(key, newSubgraph);
    return newSubgraph;
  }

  get resolvedNodeDefaults(): NormalizedAttributes {
    return new NormalizedAttributes([
      ...this.owner.resolvedNodeDefaults,
      ...this.nodeAttributes,
    ]);
  }

  get resolvedEdgeDefaults(): NormalizedAttributes {
    return new NormalizedAttributes([
      ...this.owner.resolvedEdgeDefaults,
      ...this.edgeAttributes,
    ]);
  }

  sortedMemberNodes(): NormalizedNode[] {
    return [...this.#memberNodes].toSorted((a, b) => a.index - b.index);
  }

  sortedMemberEdges(): NormalizedEdge[] {
    return [...this.#memberEdges].toSorted((a, b) => a.index - b.index);
  }

  getSubgraphs(): NormalizedSubgraph[] {
    return [...this.#subgraphs.values()];
  }

  toJSON() {
    return {
      name: this.name,
      graphAttributes: this.graphAttributes,
      nodeAttributes: this.nodeAttributes,
      edgeAttributes: this.edgeAttributes,
      memberNodes: this.sortedMemberNodes().map((node) => node.index),
      memberEdges: this.sortedMemberEdges().map((edge) => edge.index),
      subgraphs: this.getSubgraphs(),
    };
  }
}

export type NormalizedAttributeValue = { html: string } | { text: string };

export class NormalizedAttributes extends Map<
  string,
  { html: string } | { text: string } | undefined
> {
  toJSON(): Record<string, NormalizedAttributeValue | null> {
    return Object.fromEntries(
      this.entries().map(([name, value]) => [name, value ?? null]),
    );
  }

  static isHTML(value: NormalizedAttributeValue): value is { html: string } {
    return 'html' in value;
  }

  static isText(value: NormalizedAttributeValue): value is { text: string } {
    return 'text' in value;
  }

  static valueToString(value: NormalizedAttributeValue): string {
    return this.isHTML(value) ? `<${value.html}>` : `"${value.text}"`;
  }
}

function normalizeAttributes(
  attributes: Attributes | undefined,
): NormalizedAttributes {
  if (attributes === undefined) return new NormalizedAttributes();

  return new NormalizedAttributes(
    Object.entries(attributes).map(([name, value]) => {
      switch (typeof value) {
        case 'undefined':
          return [name, undefined];
        case 'string':
          // In graphviz, empty strings are treated as default values
          return [name, value === '' ? undefined : { text: value }];
        case 'object':
          return [name, { html: value.html }];
        default:
          return [name, { text: value.toString() }];
      }
    }),
  );
}

export function normalizeGraph(
  config: Graph,
  overrideAttributes: OverrideAttributes,
): NormalizedGraph {
  const graph = new NormalizedGraph(
    {
      name: config.name,
      strict: config.strict ?? false,
      directed: config.directed ?? true,
      graphAttributes: normalizeAttributes(config.graphAttributes),
      nodeAttributes: normalizeAttributes(config.nodeAttributes),
      edgeAttributes: normalizeAttributes(config.edgeAttributes),
    },
    overrideAttributes,
  );
  applyDefinitions(graph, config);
  return graph;
}

function applyDefinitions(
  owner: NormalizedGraph | NormalizedSubgraph,
  config: Graph | Subgraph,
) {
  const root = owner.root;
  const { nodes, edges, subgraphs } = config;
  if (nodes) {
    for (const { name, attributes } of nodes) {
      root.upsertNode(owner, {
        name,
        attributes: normalizeAttributes(attributes),
      });
    }
  }

  if (edges) {
    for (const edgeConfig of edges) {
      const tail = root.upsertNode(owner, {
        name: edgeConfig.tail,
        attributes: new NormalizedAttributes(),
      });
      const head = root.upsertNode(owner, {
        name: edgeConfig.head,
        attributes: new NormalizedAttributes(),
      });
      root.upsertEdge(owner, {
        tail: tail.defaultEndpoint,
        head: head.defaultEndpoint,
        key: undefined,
        attributes: normalizeAttributes(edgeConfig.attributes),
      });
    }
  }

  if (subgraphs) {
    for (const subgraphConfig of subgraphs) {
      const subgraph = owner.upsertSubgraph({
        name: subgraphConfig.name,
        graphAttributes: normalizeAttributes(subgraphConfig.graphAttributes),
        nodeAttributes: normalizeAttributes(subgraphConfig.nodeAttributes),
        edgeAttributes: normalizeAttributes(subgraphConfig.edgeAttributes),
      });
      applyDefinitions(subgraph, subgraphConfig);
    }
  }
}

function applyAttributesToEdgeConfig(
  config: NormalizedEdgeConfig,
): NormalizedEdgeConfig {
  let key: string | undefined;
  let tailport: string | undefined;
  let headport: string | undefined;
  const attributes = new NormalizedAttributes();

  for (const [name, value] of config.attributes.entries()) {
    switch (name) {
      case 'key':
        /* v8 ignore start */
        if (value !== undefined && NormalizedAttributes.isHTML(value)) {
          throw new TypeError(`HTML as edge 'key' is not supported`);
        }
        /* v8 ignore stop */
        key = value?.text;
        break;
      case 'tailport':
        /* v8 ignore start */
        if (value !== undefined && NormalizedAttributes.isHTML(value)) {
          throw new TypeError(`HTML as 'tailport' is not supported`);
        }
        /* v8 ignore stop */
        tailport = value?.text;
        break;
      case 'headport':
        /* v8 ignore start */
        if (value !== undefined && NormalizedAttributes.isHTML(value)) {
          throw new TypeError(`HTML as 'headport' is not supported`);
        }
        /* v8 ignore stop */
        headport = value?.text;
        break;
      default:
        attributes.set(name, value);
    }
  }

  return {
    key: config.key ?? key?.toString() ?? undefined,
    tail: applyPortString(config.tail, tailport?.toString()),
    head: applyPortString(config.head, headport?.toString()),
    attributes,
  };
}

function applyPortString(
  endpoint: NormalizedEdgeEndpoint,
  str: string | undefined,
): NormalizedEdgeEndpoint {
  if (str === undefined) {
    return endpoint;
  }
  const [port, compass] = str.split(':') as [string, string | undefined];
  // FIXME: missing validation of compass
  return { port: endpoint.port.node.upsertPort(port), compass };
}
