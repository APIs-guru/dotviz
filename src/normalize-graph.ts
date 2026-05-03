import type { Attributes, Graph, Subgraph } from './graph.d.ts';

export interface OverrideAttributes {
  readonly graphAttributes: Readonly<Attributes> | undefined;
  readonly nodeAttributes: Readonly<Attributes> | undefined;
  readonly edgeAttributes: Readonly<Attributes> | undefined;
}

interface NormalizedGraphConfig {
  readonly name: string | null;
  readonly strict: boolean;
  readonly directed: boolean;
  readonly graphAttributes: Readonly<Attributes>;
  readonly nodeAttributes: Readonly<Attributes>;
  readonly edgeAttributes: Readonly<Attributes>;
}

export class NormalizedGraph {
  readonly #overrideGraphAttributes: Readonly<Attributes>;
  readonly #overrideNodeAttributes: Readonly<Attributes>;
  readonly #overrideEdgeAttributes: Readonly<Attributes>;
  readonly parent = null;
  readonly root = this;

  readonly name: string | null;
  readonly strict: boolean;
  readonly directed: boolean;
  graphAttributes: Readonly<Attributes>;
  nodeAttributes: Readonly<Attributes>;
  edgeAttributes: Readonly<Attributes>;
  readonly #allNodes = new Map<string, NormalizedNode>();
  readonly #allEdges = new Map<string | number, NormalizedEdge>();
  readonly #subgraphs = new Map<string | number, NormalizedSubgraph>();

  constructor(
    config: NormalizedGraphConfig,
    overrideAttributes: OverrideAttributes,
  ) {
    this.#overrideGraphAttributes = overrideAttributes.graphAttributes ?? {};
    this.#overrideNodeAttributes = overrideAttributes.nodeAttributes ?? {};
    this.#overrideEdgeAttributes = overrideAttributes.edgeAttributes ?? {};
    this.graphAttributes = {
      ...config.graphAttributes,
      ...this.#overrideGraphAttributes,
    };
    this.nodeAttributes = {
      ...config.nodeAttributes,
      ...this.#overrideNodeAttributes,
    };
    this.edgeAttributes = {
      ...config.edgeAttributes,
      ...this.#overrideEdgeAttributes,
    };

    this.name = config.name;
    this.strict = config.strict;
    this.directed = config.directed;
  }

  mergeGraphAttributes(newAttributes: Readonly<Attributes>) {
    const defaultAttributes: Attributes = {};
    for (const key of Object.keys(newAttributes)) {
      if (this.#overrideGraphAttributes[key] === undefined) {
        defaultAttributes[key] = this.graphAttributes[key] ?? '';
      }
    }
    for (const subgraph of this.#subgraphs.values()) {
      subgraph.applyDefaultGraphAttributes(defaultAttributes);
    }

    this.graphAttributes = {
      ...this.graphAttributes,
      ...newAttributes,
      ...this.#overrideGraphAttributes,
    };
  }

  mergeNodeAttributes(newAttributes: Readonly<Attributes>) {
    const defaultAttributes: Attributes = {};
    for (const key of Object.keys(newAttributes)) {
      if (this.#overrideNodeAttributes[key] === undefined) {
        defaultAttributes[key] = '';
      }
    }
    for (const node of this.#allNodes.values()) {
      node.applyDefaultAttributes(defaultAttributes);
    }

    this.nodeAttributes = {
      ...this.nodeAttributes,
      ...newAttributes,
      ...this.#overrideNodeAttributes,
    };
  }

  mergeEdgeAttributes(newAttributes: Readonly<Attributes>) {
    const defaultAttributes: Attributes = {};
    for (const key of Object.keys(newAttributes)) {
      if (this.#overrideEdgeAttributes[key] === undefined) {
        defaultAttributes[key] = '';
      }
    }
    for (const edge of this.#allEdges.values()) {
      edge.applyDefaultAttributes(defaultAttributes);
    }
    this.edgeAttributes = {
      ...this.edgeAttributes,
      ...newAttributes,
      ...this.#overrideEdgeAttributes,
    };
  }

  upsertNode(
    scope: NormalizedGraph | NormalizedSubgraph,
    config: NormalizedNodeConfig,
  ): NormalizedNode {
    const { name } = config;
    const node = this.#allNodes.get(name);
    if (node !== undefined) {
      scope.addNode(node);
      node.mergeAttributes(config.attributes);
      return node;
    }

    const newNode = new NormalizedNode(this.#allNodes.size, {
      name,
      attributes: { ...scope.resolvedNodeDefaults, ...config.attributes },
    });
    scope.addNode(newNode);
    return newNode;
  }

  upsertEdge(
    scope: NormalizedGraph | NormalizedSubgraph,
    config: NormalizedEdgeConfig,
  ): NormalizedEdge {
    const newEdge = new NormalizedEdge(
      this.#allEdges.size,
      normalizeEdgeConfig({
        tail: config.tail,
        head: config.head,
        key: config.key,
        attributes: { ...scope.resolvedEdgeDefaults, ...config.attributes },
      }),
    );

    const hashKey = this.#edgeHashKey(newEdge);
    if (hashKey !== null) {
      const edge = this.#allEdges.get(hashKey);
      if (edge !== undefined) {
        scope.addEdge(edge);
        edge.mergeAttributes(normalizeEdgeConfig(config).attributes);
        return edge;
      }
    }

    scope.addEdge(newEdge);
    return newEdge;
  }

  addNode(node: NormalizedNode): void {
    this.#allNodes.set(node.name, node);
  }

  addEdge(edge: NormalizedEdge): void {
    const hashKey = this.#edgeHashKey(edge);
    this.#allEdges.set(hashKey ?? edge.index, edge);
  }

  #edgeHashKey(edge: NormalizedEdge): string | null {
    const { key } = edge;
    if (key === null && !this.strict) {
      return null;
    }

    let { tail, head } = edge;
    if (!this.directed) {
      const shouldSwap =
        head.port.node.index > tail.port.node.index ||
        head.port.index > tail.port.index;
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
    if (name !== null) {
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

  get resolvedNodeDefaults(): Readonly<Attributes> {
    return this.nodeAttributes;
  }

  get resolvedEdgeDefaults(): Readonly<Attributes> {
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
  readonly name: string | null;
}

export class NormalizedPort {
  readonly index: number;
  readonly node: NormalizedNode;
  readonly name: string | null;

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
  readonly attributes: Attributes;
}

export class NormalizedNode {
  readonly index: number;
  readonly name: string;
  readonly defaultPort = new NormalizedPort(0, { node: this, name: null });
  readonly defaultEndpoint = { port: this.defaultPort, compass: null };
  readonly ports = new Map<string | null, NormalizedPort>([
    [null, this.defaultPort],
  ]);
  attributes: Readonly<Attributes>;

  constructor(index: number, config: NormalizedNodeConfig) {
    this.index = index;
    this.name = config.name;
    this.attributes = config.attributes;
  }

  mergeAttributes(newAttributes: Readonly<Attributes>): void {
    this.attributes = { ...this.attributes, ...newAttributes };
  }

  applyDefaultAttributes(defaults: Readonly<Attributes>): void {
    this.attributes = { ...defaults, ...this.attributes };
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
  readonly compass: string | null;
}

interface NormalizedEdgeConfig {
  readonly tail: NormalizedEdgeEndpoint;
  readonly head: NormalizedEdgeEndpoint;
  readonly key: string | null;
  readonly attributes: Readonly<Attributes>;
}

export class NormalizedEdge {
  readonly index: number;
  readonly tail: NormalizedEdgeEndpoint;
  readonly head: NormalizedEdgeEndpoint;
  readonly key: string | null;
  attributes: Readonly<Attributes>;

  constructor(index: number, config: NormalizedEdgeConfig) {
    this.index = index;
    this.tail = config.tail;
    this.head = config.head;
    this.key = config.key;
    this.attributes = config.attributes;
  }

  mergeAttributes(newAttributes: Readonly<Attributes>) {
    this.attributes = { ...this.attributes, ...newAttributes };
  }

  applyDefaultAttributes(defaults: Readonly<Attributes>) {
    this.attributes = { ...defaults, ...this.attributes };
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
  readonly name: string | null;
  readonly graphAttributes: Attributes;
  readonly nodeAttributes: Attributes;
  readonly edgeAttributes: Attributes;
}

export class NormalizedSubgraph {
  readonly root: NormalizedGraph;
  readonly parent: NormalizedGraph | NormalizedSubgraph;

  readonly name: string | null;
  graphAttributes: Readonly<Attributes>;
  nodeAttributes: Readonly<Attributes>;
  edgeAttributes: Readonly<Attributes>;
  readonly #memberNodes = new Set<NormalizedNode>();
  readonly #memberEdges = new Set<NormalizedEdge>();
  readonly #subgraphs = new Map<string | number, NormalizedSubgraph>();

  constructor(
    parent: NormalizedGraph | NormalizedSubgraph,
    config: NormalizedSubgraphConfig,
  ) {
    this.root = parent.root;
    this.parent = parent;
    this.name = config.name;
    this.graphAttributes = config.graphAttributes;
    this.nodeAttributes = config.nodeAttributes;
    this.edgeAttributes = config.edgeAttributes;
  }

  mergeGraphAttributes(newAttributes: Readonly<Attributes>): void {
    const defaultAttributes: Attributes = {};
    for (const key of Object.keys(newAttributes)) {
      defaultAttributes[key] = this.graphAttributes[key] ?? '';
    }
    for (const subgraph of this.#subgraphs.values()) {
      subgraph.applyDefaultGraphAttributes(defaultAttributes);
    }

    this.graphAttributes = { ...this.graphAttributes, ...newAttributes };
  }

  mergeNodeAttributes(newAttributes: Readonly<Attributes>): void {
    this.nodeAttributes = { ...this.nodeAttributes, ...newAttributes };
  }

  mergeEdgeAttributes(newAttributes: Readonly<Attributes>): void {
    this.edgeAttributes = { ...this.edgeAttributes, ...newAttributes };
  }

  applyDefaultGraphAttributes(defaults: Readonly<Attributes>): void {
    this.graphAttributes = { ...defaults, ...this.graphAttributes };
  }

  addNode(node: NormalizedNode): void {
    this.parent.addNode(node);
    this.#memberNodes.add(node);
  }

  addEdge(edge: NormalizedEdge): void {
    this.parent.addEdge(edge);
    this.#memberEdges.add(edge);
  }

  upsertSubgraph(
    config: Readonly<NormalizedSubgraphConfig>,
  ): NormalizedSubgraph {
    const { name } = config;
    if (name !== null) {
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

  get resolvedNodeDefaults(): Attributes {
    return { ...this.parent.resolvedNodeDefaults, ...this.nodeAttributes };
  }

  get resolvedEdgeDefaults(): Attributes {
    return { ...this.parent.resolvedEdgeDefaults, ...this.edgeAttributes };
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

export function normalizeGraph(
  config: Graph,
  overrideAttributes: OverrideAttributes,
): NormalizedGraph {
  const graph = new NormalizedGraph(
    {
      name: config.name ?? null,
      strict: config.strict ?? false,
      directed: config.directed ?? true,
      graphAttributes: config.graphAttributes ?? {},
      nodeAttributes: config.nodeAttributes ?? {},
      edgeAttributes: config.edgeAttributes ?? {},
    },
    overrideAttributes,
  );
  applyDefinitions(graph, config);
  return graph;
}

function applyDefinitions(
  scope: NormalizedGraph | NormalizedSubgraph,
  config: Graph | Subgraph,
) {
  const root = scope.root;
  const { nodes, edges, subgraphs } = config;
  if (nodes) {
    for (const { name, attributes } of nodes) {
      root.upsertNode(scope, { name, attributes: attributes ?? {} });
    }
  }

  if (edges) {
    for (const edgeConfig of edges) {
      const tail = root.upsertNode(scope, {
        name: edgeConfig.tail,
        attributes: {},
      });
      const head = root.upsertNode(scope, {
        name: edgeConfig.head,
        attributes: {},
      });
      root.upsertEdge(scope, {
        tail: tail.defaultEndpoint,
        head: head.defaultEndpoint,
        key: null,
        attributes: edgeConfig.attributes ?? {},
      });
    }
  }

  if (subgraphs) {
    for (const subgraphConfig of subgraphs) {
      const subgraph = scope.upsertSubgraph({
        name: subgraphConfig.name ?? null,
        graphAttributes: subgraphConfig.graphAttributes ?? {},
        nodeAttributes: subgraphConfig.nodeAttributes ?? {},
        edgeAttributes: subgraphConfig.edgeAttributes ?? {},
      });
      applyDefinitions(subgraph, subgraphConfig);
    }
  }
}

function normalizeEdgeConfig(
  config: NormalizedEdgeConfig,
): NormalizedEdgeConfig {
  const { key, tailport, headport, ...attributes } = config.attributes;

  /* v8 ignore start -- FIXME: it's weird edge case, so in future we should forbid using HTML as keys */
  if (typeof key === 'object') {
    throw new TypeError('HTML as edge key is not supported');
  }
  if (typeof tailport === 'object') {
    throw new TypeError('HTML as tailport is not supported');
  }
  if (typeof headport === 'object') {
    throw new TypeError('HTML as headport is not supported');
  }
  /* v8 ignore end */

  return {
    key: config.key ?? key?.toString() ?? null,
    tail: mergePortString(config.tail, tailport?.toString()),
    head: mergePortString(config.head, headport?.toString()),
    attributes,
  };
}

function mergePortString(
  endpoint: NormalizedEdgeEndpoint,
  str: string | undefined,
): NormalizedEdgeEndpoint {
  if (str === undefined || str === '') return endpoint;
  const [port, compass] = str.split(':') as [string, string | undefined];
  return {
    port: endpoint.port.node.upsertPort(port),
    compass: compass ?? null,
  };
}
