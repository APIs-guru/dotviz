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
    rawConfig: NormalizedEdgeConfig,
  ): NormalizedEdge {
    const resolvedConfig = normalizeEdgeConfig({
      tail: rawConfig.tail,
      head: rawConfig.head,
      key: rawConfig.key,
      attributes: { ...scope.resolvedEdgeDefaults, ...rawConfig.attributes },
    });

    const hashKey = this.#edgeHashKey(resolvedConfig);
    if (hashKey !== null) {
      const edge = this.#allEdges.get(hashKey);
      if (edge !== undefined) {
        scope.addEdge(edge);
        edge.mergeAttributes(normalizeEdgeConfig(rawConfig).attributes);
        return edge;
      }
    }

    const newEdge = new NormalizedEdge(this.#allEdges.size, resolvedConfig);
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

  #edgeHashKey(config: NormalizedEdgeConfig): string | null {
    let tail = config.tail.node.index;
    let head = config.head.node.index;

    if (!this.directed && head > tail) {
      [tail, head] = [head, tail];
    }

    if (this.strict) {
      return tail.toString() + ':' + head.toString();
    }

    if (config.key == null) {
      return null;
    }
    return tail.toString() + ':' + head.toString() + ':' + config.key;
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

export interface NormalizedNodeConfig {
  readonly name: string;
  readonly attributes: Attributes;
}

export class NormalizedNode {
  readonly index: number;
  readonly name: string;
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

  toJSON() {
    return {
      name: this.name,
      attributes: this.attributes,
    };
  }
}

export interface EdgePort {
  readonly name: string;
  readonly compass: string | null;
}

export interface EdgeEndpoint {
  readonly node: NormalizedNode;
  readonly port: EdgePort | null;
}

interface NormalizedEdgeConfig {
  readonly tail: EdgeEndpoint;
  readonly head: EdgeEndpoint;
  readonly key: string | null;
  readonly attributes: Readonly<Attributes>;
}

export class NormalizedEdge {
  readonly index: number;
  readonly tail: EdgeEndpoint;
  readonly head: EdgeEndpoint;
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
      tail: { node: this.tail.node.index, port: this.tail.port },
      head: { node: this.head.node.index, port: this.head.port },
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
        tail: { node: tail, port: null },
        head: { node: head, port: null },
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
    tail: {
      node: config.tail.node,
      port: config.tail.port ?? splitPortString(tailport?.toString()),
    },
    head: {
      node: config.head.node,
      port: config.head.port ?? splitPortString(headport?.toString()),
    },
    attributes,
  };
}

function splitPortString(str: string | undefined): EdgePort | null {
  if (str === undefined) return null;
  const [name, compass] = str.split(':') as [string, string | undefined];
  return { name, compass: compass ?? null };
}
