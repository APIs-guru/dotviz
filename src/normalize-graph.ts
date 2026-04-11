import type { Attributes, Graph, Subgraph } from './graph.d.ts';

export interface FixedAttributes {
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
  name: string | null;
  strict: boolean;
  directed: boolean;
  fixedGraphAttributes: Attributes = {};
  fixedNodeAttributes: Attributes = {};
  fixedEdgeAttributes: Attributes = {};
  graphAttributes: Attributes = {};
  nodeAttributes: Attributes = {};
  edgeAttributes: Attributes = {};
  allNodes = new Map<string, NormalizedNode>();
  allEdges = new Map<string | number, NormalizedEdge>();
  subgraphs = new Map<string | number, NormalizedSubgraph>();

  constructor(config: NormalizedGraphConfig, fixedAttributes: FixedAttributes) {
    this.name = config.name;
    this.strict = config.strict;
    this.directed = config.directed;
    this.fixedGraphAttributes = fixedAttributes.graphAttributes ?? {};
    this.fixedNodeAttributes = fixedAttributes.nodeAttributes ?? {};
    this.fixedEdgeAttributes = fixedAttributes.edgeAttributes ?? {};
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

    this.graphAttributes = {
      ...this.graphAttributes,
      ...newAttributes,
      ...this.fixedGraphAttributes,
    };
  }

  mergeNodeAttributes(newAttributes: Attributes | undefined) {
    if (newAttributes === undefined) {
      return;
    }

    const defaultAttributes: Attributes = {};
    for (const key of Object.keys(newAttributes)) {
      defaultAttributes[key] = '';
    }
    for (const node of this.allNodes.values()) {
      node.applyDefaultAttributes(defaultAttributes);
    }

    this.nodeAttributes = {
      ...this.nodeAttributes,
      ...newAttributes,
      ...this.fixedNodeAttributes,
    };
  }

  mergeEdgeAttributes(newAttributes: Attributes | undefined) {
    if (newAttributes === undefined) {
      return;
    }

    const defaultAttributes: Attributes = {};
    for (const key of Object.keys(newAttributes)) {
      defaultAttributes[key] = '';
    }
    for (const edge of this.allEdges.values()) {
      edge.applyDefaultAttributes(defaultAttributes);
    }
    this.edgeAttributes = {
      ...this.edgeAttributes,
      ...newAttributes,
      ...this.fixedEdgeAttributes,
    };
  }

  upsertNode(name: string): [NormalizedNode, boolean] {
    const node = this.allNodes.get(name);
    if (node !== undefined) {
      return [node, false];
    }

    const newNode = new NormalizedNode(this.allNodes.size, name);
    newNode.mergeAttributes(this.nodeAttributes);
    this.allNodes.set(name, newNode);
    return [newNode, true];
  }

  upsertEdge(config: NormalizedEdgeConfig): [NormalizedEdge, boolean] {
    // FIXME: handle special 'key' attribute
    const key = this.edgeKey(config);
    if (key) {
      const edge = this.allEdges.get(key);
      if (edge !== undefined) {
        return [edge, false];
      }
    }

    const newEdge = new NormalizedEdge(this.allEdges.size, config);
    newEdge.mergeAttributes(this.edgeAttributes);
    this.allEdges.set(key ?? newEdge.index, newEdge);
    return [newEdge, true];
  }

  edgeKey(config: NormalizedEdgeConfig): string | undefined {
    let tail = config.tail.index;
    let head = config.head.index;

    if (!this.directed && head > tail) {
      [tail, head] = [head, tail];
    }

    if (this.strict) {
      return tail.toString() + ':' + head.toString();
    }

    if (config.key == null) {
      return undefined;
    }
    return tail.toString() + ':' + head.toString() + ':' + config.key;
  }

  upsertSubgraph(name: string | null): NormalizedSubgraph {
    if (name !== null) {
      const subgraph = this.subgraphs.get(name);
      if (subgraph) {
        return subgraph;
      }
    }

    const key = name ?? this.subgraphs.size;
    const newSubgraph = new NormalizedSubgraph(this, name);
    this.subgraphs.set(key, newSubgraph);
    return newSubgraph;
  }

  toJSON() {
    return {
      name: this.name,
      strict: this.strict,
      directed: this.directed,
      graphAttributes: {
        ...this.graphAttributes,
        ...this.fixedGraphAttributes,
      },
      nodeAttributes: { ...this.nodeAttributes, ...this.fixedNodeAttributes },
      edgeAttributes: { ...this.edgeAttributes, ...this.fixedEdgeAttributes },
      allNodes: [...this.allNodes.values()],
      allEdges: [...this.allEdges.values()],
      subgraphs: [...this.subgraphs.values()],
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

  mergeAttributes(newAttributes: Attributes | undefined) {
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

  constructor(
    parent: NormalizedGraph | NormalizedSubgraph,
    name: string | null,
  ) {
    this.parent = parent;
    this.name = name;
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

  upsertNode(name: string): [NormalizedNode, boolean] {
    const [node, isCreated] = this.parent.upsertNode(name);

    this.nodeIndexes.add(node);
    if (isCreated) {
      this.ownedNodes.add(node);
      node.mergeAttributes(this.nodeAttributes);
    }
    return [node, isCreated];
  }

  upsertEdge(config: NormalizedEdgeConfig): [NormalizedEdge, boolean] {
    const [edge, isCreated] = this.parent.upsertEdge(config);

    this.edgeIndexes.add(edge);
    if (isCreated) {
      this.ownedEdges.add(edge);
      edge.mergeAttributes(this.edgeAttributes);
    }
    return [edge, isCreated];
  }

  upsertSubgraph(name: string | null): NormalizedSubgraph {
    if (name !== null) {
      const subgraph = this.subgraphs.get(name);
      if (subgraph) {
        return subgraph;
      }
    }

    const key = name ?? this.subgraphs.size;
    const newSubgraph = new NormalizedSubgraph(this, name);
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

export function normalizeGraph(
  config: Graph,
  fixedAttributes: FixedAttributes,
): NormalizedGraph {
  const graph = new NormalizedGraph(
    {
      name: config.name ?? null,
      strict: config.strict ?? false,
      directed: config.directed ?? true,
    },
    fixedAttributes,
  );
  applyDefinitions(graph, config);
  return graph;
}

function applyDefinitions(
  owner: NormalizedGraph | NormalizedSubgraph,
  config: Graph | Subgraph,
) {
  owner.mergeGraphAttributes(config.graphAttributes);
  owner.mergeNodeAttributes(config.nodeAttributes);
  owner.mergeEdgeAttributes(config.edgeAttributes);

  const { nodes, edges, subgraphs } = config;
  if (nodes) {
    for (const config of nodes) {
      const [node] = owner.upsertNode(config.name);
      node.mergeAttributes(config.attributes);
    }
  }

  if (edges) {
    for (const config of edges) {
      const [tail] = owner.upsertNode(config.tail);
      const [head] = owner.upsertNode(config.head);
      const [key, attributes] = extractKeyFromEdgeAttributes(config.attributes);
      const [edge] = owner.upsertEdge({ tail, head, key });
      edge.mergeAttributes(attributes);
    }
  }

  if (subgraphs) {
    for (const config of subgraphs) {
      const subgraph = owner.upsertSubgraph(config.name ?? null);
      applyDefinitions(subgraph, config);
    }
  }
}

export function extractKeyFromEdgeAttributes(
  attributes: Attributes | undefined,
): [string | null, Attributes] {
  if (attributes == null) {
    return [null, {}];
  }

  const { key } = attributes;
  if (key == null) {
    return [null, attributes];
  }

  const attributesCopy = { ...attributes };
  delete attributesCopy.key;

  /* v8 ignore start -- FIXME: it's wierd edge case, so in future we should forbid using HTML as keys */
  if (typeof key === 'object') {
    throw new TypeError('HTML as edge key is not supported');
  }
  /* v8 ignore end */
  return [key.toString(), attributesCopy];
}
