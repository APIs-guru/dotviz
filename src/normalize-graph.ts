import type { Attributes, Edge, Graph, Node, Subgraph } from './graph.d.ts';

export class NormalizedGraph {
  name: string | null;
  strict: boolean;
  directed: boolean;
  graphAttributes: Attributes = {};
  nodeAttributes: Attributes = {};
  edgeAttributes: Attributes = {};
  allNodes = new Map<string, NormalizedNode>();
  allEdges: NormalizedEdge[] = [];
  subgraphs = new Map<string | number, NormalizedSubgraph>();

  constructor(config: Graph) {
    this.name = config.name ?? null;
    this.strict = config.strict ?? false;
    this.directed = config.directed ?? true;
    this.graphAttributes = config.graphAttributes ?? {};
    this.nodeAttributes = config.nodeAttributes ?? {};
    this.edgeAttributes = config.edgeAttributes ?? {};

    const { nodes = [], edges = [], subgraphs = [] } = config;
    for (const node of nodes) {
      this.upsertNode(node);
    }

    for (const edge of edges) {
      this.upsertEdge(edge);
    }

    for (const subgraph of subgraphs) {
      this.upsertSubgraph(subgraph);
    }
  }

  mergeGraphAttributes(newAttributes: Attributes | undefined) {
    if (newAttributes === undefined) {
      return;
    }

    const defaultAttributes: Attributes = {};
    for (const key of Object.keys(newAttributes)) {
      defaultAttributes[key] = this.graphAttributes[key] ?? '';
    }
    for (const subgraph of this.subgraphs.values()) {
      subgraph.applyDefaultGraphAttributes(defaultAttributes);
    }

    this.graphAttributes = { ...this.graphAttributes, ...newAttributes };
  }

  mergeNodeAttributes(newAttributes: Attributes | undefined) {
    if (newAttributes === undefined) {
      return;
    }

    const defaultAttributes: Attributes = {};
    for (const key of Object.keys(newAttributes)) {
      defaultAttributes[key] = this.nodeAttributes[key] ?? '';
    }
    for (const node of this.allNodes.values()) {
      node.applyDefaultAttributes(defaultAttributes);
    }

    this.nodeAttributes = { ...this.nodeAttributes, ...newAttributes };
  }

  mergeEdgeAttributes(newAttributes: Attributes | undefined) {
    if (newAttributes === undefined) {
      return;
    }

    const defaultAttributes: Attributes = {};
    for (const key of Object.keys(newAttributes)) {
      defaultAttributes[key] = this.edgeAttributes[key] ?? '';
    }
    for (const edge of this.allEdges) {
      edge.applyDefaultAttributes(defaultAttributes);
    }
    this.edgeAttributes = { ...this.edgeAttributes, ...newAttributes };
  }

  upsertNode(
    config: Node,
    defaultAttributes: Attributes = {},
  ): [NormalizedNode, boolean] {
    const { name } = config;
    const node = this.allNodes.get(name);
    if (node !== undefined) {
      return [node, false];
    }

    const newNode = new NormalizedNode(this.allNodes.size, config);
    newNode.applyDefaultAttributes(defaultAttributes);
    this.allNodes.set(name, newNode);
    return [newNode, true];
  }

  upsertEdge(
    config: Edge,
    defaultAttributes: Attributes = {},
  ): [NormalizedEdge, boolean] {
    // FIXME: handle special 'key' attribute
    // FIXME: handle strict graphs
    const edge = new NormalizedEdge(this.allEdges.length, config);
    edge.applyDefaultAttributes(defaultAttributes);
    this.allEdges.push(edge);
    return [edge, true];
  }

  upsertSubgraph(config: Subgraph): NormalizedSubgraph {
    const { name } = config;
    if (name !== undefined) {
      const subgraph = this.subgraphs.get(name);
      if (subgraph) {
        subgraph.mergeConfig(config);
        return subgraph;
      }
    }

    const key = name ?? this.subgraphs.size;
    const newSubgraph = new NormalizedSubgraph(this, config);
    this.subgraphs.set(key, newSubgraph);
    return newSubgraph;
  }

  toJSON() {
    return {
      name: this.name,
      strict: this.strict,
      directed: this.directed,
      graphAttributes: this.graphAttributes,
      nodeAttributes: this.nodeAttributes,
      edgeAttributes: this.edgeAttributes,
      allNodes: [...this.allNodes.values()],
      allEdges: this.allEdges,
      subgraphs: [...this.subgraphs.values()],
    };
  }
}

export class NormalizedNode {
  index: number;
  name: string;
  attributes: Attributes = {};

  constructor(index: number, config: Node) {
    this.index = index;
    this.name = config.name;
    this.mergeConfig(config);
  }

  mergeConfig(config: Node) {
    this.mergeAttributes(config.attributes);
  }

  mergeAttributes(newAttributes: Attributes | undefined) {
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

export class NormalizedEdge {
  index: number;
  tail: string;
  head: string;
  attributes: Attributes;

  constructor(index: number, config: Edge) {
    this.index = index;
    this.tail = config.tail;
    this.head = config.head;
    this.attributes = config.attributes ?? {};
  }

  mergeConfig(config: Node) {
    this.mergeAttributes(config.attributes);
  }

  mergeAttributes(newAttributes: Attributes | undefined) {
    this.attributes = { ...this.attributes, ...newAttributes };
  }

  applyDefaultAttributes(defaults: Attributes) {
    this.attributes = { ...defaults, ...this.attributes };
  }

  toJSON() {
    return {
      tail: this.tail,
      head: this.head,
      attributes: this.attributes,
    };
  }
}

export class NormalizedSubgraph {
  parent: NormalizedGraph | NormalizedSubgraph;
  ownedNodes = new Set<NormalizedNode>();
  ownedEdges = new Set<NormalizedEdge>();

  name: string | null = null;
  graphAttributes: Attributes = {};
  nodeAttributes: Attributes = {};
  edgeAttributes: Attributes = {};
  nodeIndexes = new Set<NormalizedNode>();
  edgeIndexes = new Set<NormalizedEdge>();
  subgraphs = new Map<string | number, NormalizedSubgraph>();

  constructor(parent: NormalizedGraph | NormalizedSubgraph, config: Subgraph) {
    this.parent = parent;
    this.mergeConfig(config);
  }

  mergeConfig(config: Subgraph) {
    this.name = config.name ?? null;

    this.mergeGraphAttributes(config.graphAttributes);
    this.mergeNodeAttributes(config.nodeAttributes);
    this.mergeEdgeAttributes(config.edgeAttributes);

    const { nodes = [], edges = [], subgraphs = [] } = config;
    for (const node of nodes) {
      this.upsertNode(node);
    }

    for (const edge of edges) {
      this.upsertEdge(edge);
    }

    for (const subgraph of subgraphs) {
      this.upsertSubgraph(subgraph);
    }
  }

  mergeGraphAttributes(newAttributes: Attributes | undefined) {
    if (newAttributes === undefined) {
      return;
    }

    const defaultAttributes: Attributes = {};
    for (const key of Object.keys(newAttributes)) {
      defaultAttributes[key] = this.graphAttributes[key] ?? '';
    }
    for (const subgraph of this.subgraphs.values()) {
      subgraph.applyDefaultGraphAttributes(defaultAttributes);
    }

    this.graphAttributes = { ...this.graphAttributes, ...newAttributes };
  }

  mergeNodeAttributes(newAttributes: Attributes | undefined) {
    if (newAttributes === undefined) {
      return;
    }

    const defaultAttributes: Attributes = {};
    for (const key of Object.keys(newAttributes)) {
      defaultAttributes[key] = this.nodeAttributes[key] ?? '';
    }
    for (const node of this.ownedNodes) {
      node.applyDefaultAttributes(defaultAttributes);
    }

    this.nodeAttributes = { ...this.nodeAttributes, ...newAttributes };
  }

  mergeEdgeAttributes(newAttributes: Attributes | undefined) {
    if (newAttributes === undefined) {
      return;
    }

    const defaultAttributes: Attributes = {};
    for (const key of Object.keys(newAttributes)) {
      defaultAttributes[key] = this.edgeAttributes[key] ?? '';
    }
    for (const edge of this.ownedEdges) {
      edge.applyDefaultAttributes(defaultAttributes);
    }
    this.edgeAttributes = { ...this.edgeAttributes, ...newAttributes };
  }

  applyDefaultGraphAttributes(defaults: Attributes) {
    this.graphAttributes = { ...defaults, ...this.graphAttributes };
  }

  upsertNode(
    config: Node,
    defaultAttributes: Attributes = {},
  ): [NormalizedNode, boolean] {
    const [node, isCreated] = this.parent.upsertNode(config, {
      ...this.nodeAttributes,
      ...defaultAttributes,
    });

    this.nodeIndexes.add(node);
    if (isCreated) {
      this.ownedNodes.add(node);
    }
    return [node, isCreated];
  }

  upsertEdge(
    config: Edge,
    defaultAttributes: Attributes = {},
  ): [NormalizedEdge, boolean] {
    // FIXME: handle special 'key' attribute
    // FIXME: handle strict graphs
    const [edge, isCreated] = this.parent.upsertEdge(config, {
      ...this.edgeAttributes,
      ...defaultAttributes,
    });

    this.edgeIndexes.add(edge);
    if (isCreated) {
      this.ownedEdges.add(edge);
    }
    return [edge, isCreated];
  }

  upsertSubgraph(config: Subgraph): NormalizedSubgraph {
    const { name } = config;
    if (name !== undefined) {
      const subgraph = this.subgraphs.get(name);
      if (subgraph) {
        subgraph.mergeConfig(config);
        return subgraph;
      }
    }

    const key = name ?? this.subgraphs.size;
    const newSubgraph = new NormalizedSubgraph(this, config);
    this.subgraphs.set(key, newSubgraph);
    return newSubgraph;
  }

  sortedNodes(): NormalizedNode[] {
    return [...this.nodeIndexes].toSorted((a, b) => a.index - b.index);
  }

  sortedEdges(): NormalizedEdge[] {
    return [...this.edgeIndexes].toSorted((a, b) => a.index - b.index);
  }

  toJSON() {
    return {
      name: this.name,
      graphAttributes: this.graphAttributes,
      nodeAttributes: this.nodeAttributes,
      edgeAttributes: this.edgeAttributes,
      nodeIndexes: this.sortedNodes().map((node) => node.index),
      edgeIndexes: this.sortedEdges().map((edge) => edge.index),
      subgraphs: [...this.subgraphs.values()],
    };
  }
}
