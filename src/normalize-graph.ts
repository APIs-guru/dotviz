import type { Attributes, Graph, Subgraph } from './graph.d.ts';

export interface OverrideAttributes {
  graphAttributes: Attributes | undefined;
  nodeAttributes: Attributes | undefined;
  edgeAttributes: Attributes | undefined;
}

interface NormalizedGraphConfig {
  name: string | null;
  strict: boolean;
  directed: boolean;
}

export class NormalizedGraph {
  overrideGraphAttributes: Attributes;
  overrideNodeAttributes: Attributes;
  overrideEdgeAttributes: Attributes;

  name: string | null;
  strict: boolean;
  directed: boolean;
  graphAttributes: Attributes;
  nodeAttributes: Attributes;
  edgeAttributes: Attributes;
  #allNodes = new Map<string, NormalizedNode>();
  #allEdges = new Map<string | number, NormalizedEdge>();
  #subgraphs = new Map<string | number, NormalizedSubgraph>();

  constructor(
    config: NormalizedGraphConfig,
    overrideAttributes: OverrideAttributes,
  ) {
    this.overrideGraphAttributes = overrideAttributes.graphAttributes ?? {};
    this.overrideNodeAttributes = overrideAttributes.nodeAttributes ?? {};
    this.overrideEdgeAttributes = overrideAttributes.edgeAttributes ?? {};
    this.graphAttributes = { ...this.overrideGraphAttributes };
    this.nodeAttributes = { ...this.overrideNodeAttributes };
    this.edgeAttributes = { ...this.overrideEdgeAttributes };

    this.name = config.name;
    this.strict = config.strict;
    this.directed = config.directed;
  }

  mergeGraphAttributes(newAttributes: Attributes) {
    const defaultAttributes: Attributes = {};
    for (const key of Object.keys(newAttributes)) {
      if (this.overrideGraphAttributes[key] === undefined) {
        defaultAttributes[key] = this.graphAttributes[key] ?? '';
      }
    }
    for (const subgraph of this.#subgraphs.values()) {
      subgraph.applyDefaultGraphAttributes(defaultAttributes);
    }

    this.graphAttributes = {
      ...this.graphAttributes,
      ...newAttributes,
      ...this.overrideGraphAttributes,
    };
  }

  mergeNodeAttributes(newAttributes: Attributes) {
    const defaultAttributes: Attributes = {};
    for (const key of Object.keys(newAttributes)) {
      if (this.overrideNodeAttributes[key] === undefined) {
        defaultAttributes[key] = '';
      }
    }
    for (const node of this.#allNodes.values()) {
      node.applyDefaultAttributes(defaultAttributes);
    }

    this.nodeAttributes = {
      ...this.nodeAttributes,
      ...newAttributes,
      ...this.overrideNodeAttributes,
    };
  }

  mergeEdgeAttributes(newAttributes: Attributes) {
    const defaultAttributes: Attributes = {};
    for (const key of Object.keys(newAttributes)) {
      if (this.overrideEdgeAttributes[key] === undefined) {
        defaultAttributes[key] = '';
      }
    }
    for (const edge of this.#allEdges.values()) {
      edge.applyDefaultAttributes(defaultAttributes);
    }
    this.edgeAttributes = {
      ...this.edgeAttributes,
      ...newAttributes,
      ...this.overrideEdgeAttributes,
    };
  }

  upsertNode(name: string): [NormalizedNode, boolean] {
    const node = this.#allNodes.get(name);
    if (node !== undefined) {
      return [node, false];
    }

    const newNode = new NormalizedNode(this.#allNodes.size, name);
    newNode.mergeAttributes(this.nodeAttributes);
    this.#allNodes.set(name, newNode);
    return [newNode, true];
  }

  upsertEdge(config: NormalizedEdgeConfig): [NormalizedEdge, boolean] {
    const hashKey = this.#edgeHashKey(config);
    if (hashKey !== null) {
      const edge = this.#allEdges.get(hashKey);
      if (edge !== undefined) {
        return [edge, false];
      }
    }

    const newEdge = new NormalizedEdge(this.#allEdges.size, config);

    newEdge.mergeAttributes(this.edgeAttributes);
    this.#allEdges.set(hashKey ?? newEdge.index, newEdge);
    return [newEdge, true];
  }

  #edgeHashKey(config: NormalizedEdgeConfig): string | null {
    let tail = config.tail.index;
    let head = config.head.index;

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

  upsertSubgraph(name: string | null): NormalizedSubgraph {
    if (name !== null) {
      const subgraph = this.#subgraphs.get(name);
      if (subgraph) {
        return subgraph;
      }
    }

    const key = name ?? this.#subgraphs.size;
    const newSubgraph = new NormalizedSubgraph(this, name);
    this.#subgraphs.set(key, newSubgraph);
    return newSubgraph;
  }

  getRoot(): this {
    return this;
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

export class NormalizedNode {
  index: number;
  name: string;
  attributes: Attributes = {};

  constructor(index: number, name: string) {
    this.index = index;
    this.name = name;
  }

  mergeAttributes(newAttributes: Attributes) {
    this.attributes = { ...this.attributes, ...newAttributes };
  }

  applyDefaultAttributes(defaults: Attributes) {
    this.attributes = { ...defaults, ...this.attributes };
  }

  toJSON() {
    return {
      name: this.name,
      attributes: this.attributes,
    };
  }
}

interface NormalizedEdgeConfig {
  tail: NormalizedNode;
  head: NormalizedNode;
  key: string | null;
}

export class NormalizedEdge {
  index: number;
  tail: NormalizedNode;
  head: NormalizedNode;
  key: string | null;
  attributes: Attributes = {};

  constructor(index: number, config: NormalizedEdgeConfig) {
    this.index = index;
    this.tail = config.tail;
    this.head = config.head;
    this.key = config.key;
  }

  mergeAttributes(newAttributes: Attributes) {
    this.attributes = { ...this.attributes, ...newAttributes };
  }

  applyDefaultAttributes(defaults: Attributes) {
    this.attributes = { ...defaults, ...this.attributes };
  }

  toJSON() {
    return {
      tail: this.tail.index,
      head: this.head.index,
      key: this.key,
      attributes: this.attributes,
    };
  }
}

export class NormalizedSubgraph {
  #root: NormalizedGraph;
  #parent: NormalizedGraph | NormalizedSubgraph;

  // Contains all nodes/edges created within this subgraph AND its nested subgraphs.
  #nodesCreatedInScope = new Set<NormalizedNode>();
  #edgesCreatedInScope = new Set<NormalizedEdge>();

  name: string | null = null;
  graphAttributes: Attributes = {};
  nodeAttributes: Attributes = {};
  edgeAttributes: Attributes = {};
  #memberNodes = new Set<NormalizedNode>();
  #memberEdges = new Set<NormalizedEdge>();
  #subgraphs = new Map<string | number, NormalizedSubgraph>();

  constructor(
    parent: NormalizedGraph | NormalizedSubgraph,
    name: string | null,
  ) {
    this.#root = parent.getRoot();
    this.#parent = parent;
    this.name = name;
  }

  mergeGraphAttributes(newAttributes: Attributes) {
    const defaultAttributes: Attributes = {};
    for (const key of Object.keys(newAttributes)) {
      defaultAttributes[key] = this.graphAttributes[key] ?? '';
    }
    for (const subgraph of this.#subgraphs.values()) {
      subgraph.applyDefaultGraphAttributes(defaultAttributes);
    }

    this.graphAttributes = { ...this.graphAttributes, ...newAttributes };
  }

  mergeNodeAttributes(newAttributes: Attributes) {
    const defaultAttributes: Attributes = {};
    for (const key of Object.keys(newAttributes)) {
      defaultAttributes[key] = '';
    }
    for (const node of this.#nodesCreatedInScope) {
      node.applyDefaultAttributes(defaultAttributes);
    }

    this.nodeAttributes = { ...this.nodeAttributes, ...newAttributes };
  }

  mergeEdgeAttributes(newAttributes: Attributes) {
    const defaultAttributes: Attributes = {};
    for (const key of Object.keys(newAttributes)) {
      defaultAttributes[key] = '';
    }
    for (const edge of this.#edgesCreatedInScope) {
      edge.applyDefaultAttributes(defaultAttributes);
    }
    this.edgeAttributes = { ...this.edgeAttributes, ...newAttributes };
  }

  applyDefaultGraphAttributes(defaults: Attributes) {
    this.graphAttributes = { ...defaults, ...this.graphAttributes };
  }

  upsertNode(name: string): [NormalizedNode, boolean] {
    const [node, isCreated] = this.#parent.upsertNode(name);

    this.#memberNodes.add(node);
    if (isCreated) {
      this.#nodesCreatedInScope.add(node);
      node.mergeAttributes(this.nodeAttributes);
    }
    return [node, isCreated];
  }

  upsertEdge(config: NormalizedEdgeConfig): [NormalizedEdge, boolean] {
    const [edge, isCreated] = this.#parent.upsertEdge(config);

    this.#memberEdges.add(edge);
    if (isCreated) {
      this.#edgesCreatedInScope.add(edge);
      edge.mergeAttributes(this.edgeAttributes);
    }
    return [edge, isCreated];
  }

  upsertSubgraph(name: string | null): NormalizedSubgraph {
    if (name !== null) {
      const subgraph = this.#subgraphs.get(name);
      if (subgraph) {
        return subgraph;
      }
    }

    const key = name ?? this.#subgraphs.size;
    const newSubgraph = new NormalizedSubgraph(this, name);
    this.#subgraphs.set(key, newSubgraph);
    return newSubgraph;
  }

  getRoot(): NormalizedGraph {
    return this.#root;
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
  if (config.graphAttributes) {
    owner.mergeGraphAttributes(config.graphAttributes);
  }
  if (config.nodeAttributes) {
    owner.mergeNodeAttributes(config.nodeAttributes);
  }
  if (config.edgeAttributes) {
    owner.mergeEdgeAttributes(config.edgeAttributes);
  }

  const { nodes, edges, subgraphs } = config;
  if (nodes) {
    for (const nodeConfig of nodes) {
      const [node] = owner.upsertNode(nodeConfig.name);
      if (nodeConfig.attributes) {
        node.mergeAttributes(nodeConfig.attributes);
      }
    }
  }

  if (edges) {
    for (const edgeConfig of edges) {
      const [tail] = owner.upsertNode(edgeConfig.tail);
      const [head] = owner.upsertNode(edgeConfig.head);
      const [key, attributes] = edgeConfig.attributes
        ? splitEdgeKey(edgeConfig.attributes)
        : [null, undefined];
      const [edge] = owner.upsertEdge({ tail, head, key });
      if (attributes) {
        edge.mergeAttributes(attributes);
      }
    }
  }

  if (subgraphs) {
    for (const subgraphConfig of subgraphs) {
      const subgraph = owner.upsertSubgraph(subgraphConfig.name ?? null);
      applyDefinitions(subgraph, subgraphConfig);
    }
  }
}

export function splitEdgeKey(
  attributes: Attributes,
): [string | null, Attributes] {
  const { key } = attributes;
  if (key == null) {
    return [null, attributes];
  }

  const attributesCopy = { ...attributes };
  delete attributesCopy.key;

  /* v8 ignore start -- FIXME: it's weird edge case, so in future we should forbid using HTML as keys */
  if (typeof key === 'object') {
    throw new TypeError('HTML as edge key is not supported');
  }
  /* v8 ignore end */
  return [key.toString(), attributesCopy];
}
