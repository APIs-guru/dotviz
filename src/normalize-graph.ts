import type { Attributes, Graph, Subgraph } from './graph.d.ts';
import { type OverrideAttributes } from './viz.ts';

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
  readonly allNodes: NormalizedNode[] = [];
  readonly allEdges: NormalizedEdge[] = [];
  readonly subgraphs: NormalizedSubgraph[] = [];
  readonly namedNodes = new Map<string, NormalizedNode>();
  readonly namedSubgraphs = new Map<string, NormalizedSubgraph>();
  readonly deduplicatedEdgesMap = new Map<string, NormalizedEdge>();

  constructor(
    config: NormalizedGraphConfig,
    overrideAttributes: OverrideAttributes,
  ) {
    this.#overrideGraphAttributes = overrideAttributes.graphAttributes
      ? normalizeAttributes(overrideAttributes.graphAttributes)
      : new NormalizedAttributes();
    this.#overrideNodeAttributes = overrideAttributes.nodeAttributes
      ? normalizeAttributes(overrideAttributes.nodeAttributes)
      : new NormalizedAttributes();
    this.#overrideEdgeAttributes = overrideAttributes.edgeAttributes
      ? normalizeAttributes(overrideAttributes.edgeAttributes)
      : new NormalizedAttributes();

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
    for (const subgraph of this.subgraphs) {
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

    for (const node of this.allNodes) {
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

    for (const edge of this.allEdges) {
      edge.applyDefaultAttributes(defaultAttributes);
    }
    this.edgeAttributes = new NormalizedAttributes([
      ...this.edgeAttributes,
      ...newAttributes,
      ...this.#overrideEdgeAttributes,
    ]);
  }

  upsertNode(name: string, nodeDefaults: NormalizedAttributes): NormalizedNode {
    const node = this.namedNodes.get(name);
    if (node !== undefined) {
      return node;
    }

    const newNode = new NormalizedNode(this.allNodes.length, {
      name,
      attributes: nodeDefaults,
    });
    this.namedNodes.set(name, newNode);
    this.allNodes.push(newNode);
    return newNode;
  }

  upsertEdge(
    config: NormalizedEdgeConfig,
    edgeDefaults: NormalizedAttributes,
  ): NormalizedEdge {
    const deduplicateKey = this.edgeDeduplicateKey(config);
    if (deduplicateKey === undefined) {
      const newEdge = new NormalizedEdge(
        this.allEdges.length,
        config,
        edgeDefaults,
      );
      this.allEdges.push(newEdge);
      return newEdge;
    }

    const edge = this.deduplicatedEdgesMap.get(deduplicateKey);
    if (edge !== undefined) {
      return edge;
    }

    const newEdge = new NormalizedEdge(
      this.allEdges.length,
      config,
      edgeDefaults,
    );
    this.deduplicatedEdgesMap.set(deduplicateKey, newEdge);
    this.allEdges.push(newEdge);
    return newEdge;
  }

  edgeDeduplicateKey(config: NormalizedEdgeConfig): string | undefined {
    const { key } = config;

    if (key === undefined && !this.strict) {
      return undefined;
    }

    let { tail, head } = config;
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
    if (name === undefined) {
      const newSubgraph = new NormalizedSubgraph(this, config);
      this.subgraphs.push(newSubgraph);
      return newSubgraph;
    }

    const subgraph = this.namedSubgraphs.get(name);
    if (subgraph) {
      return subgraph;
    }
    const newSubgraph = new NormalizedSubgraph(this, config);
    this.namedSubgraphs.set(name, newSubgraph);
    this.subgraphs.push(newSubgraph);
    return newSubgraph;
  }

  resolvedNodeDefaults(): NormalizedAttributes {
    return this.nodeAttributes;
  }

  resolvedEdgeDefaults(): NormalizedAttributes {
    return this.edgeAttributes;
  }
}

export interface NormalizedPortConfig {
  readonly node: NormalizedNode;
  readonly name: string | undefined;
}

export interface NormalizedPort {
  readonly index: number;
  readonly node: NormalizedNode;
  readonly name: string | undefined;
}

export interface NormalizedNodeConfig {
  readonly name: string;
  readonly attributes: NormalizedAttributes;
}

export class NormalizedNode {
  readonly index: number;
  readonly name: string;
  readonly defaultPort = { index: 0, node: this, name: undefined };
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

  upsertPort(portName: string): NormalizedPort {
    const port = this.ports.get(portName);
    if (port) {
      return port;
    }

    const newPort = { index: this.ports.size, node: this, name: portName };
    this.ports.set(portName, newPort);
    return newPort;
  }

  upsertEdgeEndpoint(
    portName: string | undefined,
    compass: string | undefined,
  ): NormalizedEdgeEndpoint {
    if (portName === undefined) {
      if (compass === undefined) {
        return this.defaultEndpoint;
      }
      return { port: this.defaultPort, compass };
    }
    return { port: this.upsertPort(portName), compass };
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
}

export class NormalizedEdge {
  readonly index: number;
  readonly tail: NormalizedEdgeEndpoint;
  readonly head: NormalizedEdgeEndpoint;
  readonly key: string | undefined;
  attributes: NormalizedAttributes;

  constructor(
    index: number,
    config: NormalizedEdgeConfig,
    attributes: NormalizedAttributes,
  ) {
    this.index = index;
    this.tail = config.tail;
    this.head = config.head;
    this.key = config.key;
    this.attributes = attributes;
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
  readonly memberNodes = new Set<NormalizedNode>();
  readonly memberEdges = new Set<NormalizedEdge>();
  readonly subgraphs: NormalizedSubgraph[] = [];
  readonly namedSubgraphs = new Map<string, NormalizedSubgraph>();

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
    for (const subgraph of this.subgraphs) {
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

  upsertNode(
    name: string,
    defaultAttributes: NormalizedAttributes,
  ): NormalizedNode {
    const node = this.owner.upsertNode(name, defaultAttributes);
    this.memberNodes.add(node);
    return node;
  }

  upsertEdge(
    config: NormalizedEdgeConfig,
    defaultAttributes: NormalizedAttributes,
  ): NormalizedEdge {
    const edge = this.owner.upsertEdge(config, defaultAttributes);
    this.memberEdges.add(edge);
    return edge;
  }

  upsertSubgraph(
    config: Readonly<NormalizedSubgraphConfig>,
  ): NormalizedSubgraph {
    const { name } = config;
    if (name === undefined) {
      const newSubgraph = new NormalizedSubgraph(this, config);
      this.subgraphs.push(newSubgraph);
      return newSubgraph;
    }

    const subgraph = this.namedSubgraphs.get(name);
    if (subgraph) {
      return subgraph;
    }

    const newSubgraph = new NormalizedSubgraph(this, config);
    this.namedSubgraphs.set(name, newSubgraph);
    this.subgraphs.push(newSubgraph);
    return newSubgraph;
  }

  resolvedNodeDefaults(): NormalizedAttributes {
    return new NormalizedAttributes([
      ...this.owner.resolvedNodeDefaults(),
      ...this.nodeAttributes,
    ]);
  }

  resolvedEdgeDefaults(): NormalizedAttributes {
    return new NormalizedAttributes([
      ...this.owner.resolvedEdgeDefaults(),
      ...this.edgeAttributes,
    ]);
  }

  sortedMemberNodes(): NormalizedNode[] {
    return [...this.memberNodes].sort((a, b) => a.index - b.index);
  }

  sortedMemberEdges(): NormalizedEdge[] {
    return [...this.memberEdges].sort((a, b) => a.index - b.index);
  }
}

export type NormalizedAttributeValue =
  | { text: undefined; html: string }
  | { text: string; html: undefined };

export class NormalizedAttributes extends Map<
  string,
  NormalizedAttributeValue | undefined
> {
  static valueToString(value: NormalizedAttributeValue): string {
    return value.text === undefined ? `<${value.html}>` : `"${value.text}"`;
  }
}

function normalizeAttributes(attributes: Attributes): NormalizedAttributes {
  return new NormalizedAttributes(
    Object.entries(attributes).map(([name, value]) => {
      switch (typeof value) {
        case 'undefined':
          return [name, undefined];
        case 'string':
          // In graphviz, empty strings are treated as default values
          return [
            name,
            value === '' ? undefined : { text: value, html: undefined },
          ];
        case 'object':
          return [name, { text: undefined, html: value.html }];
        default:
          return [name, { text: value.toString(), html: undefined }];
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
      graphAttributes: config.graphAttributes
        ? normalizeAttributes(config.graphAttributes)
        : new NormalizedAttributes(),
      nodeAttributes: config.nodeAttributes
        ? normalizeAttributes(config.nodeAttributes)
        : new NormalizedAttributes(),
      edgeAttributes: config.edgeAttributes
        ? normalizeAttributes(config.edgeAttributes)
        : new NormalizedAttributes(),
    },
    overrideAttributes,
  );
  graph.mergeNodeAttributes(
    // FIXME: check if it's viz.js hack or it also present in graphviz
    new NormalizedAttributes([
      ['label', { text: String.raw`\N`, html: undefined }],
    ]),
  );
  applyDefinitions(graph, config);
  return graph;
}

function applyDefinitions(
  owner: NormalizedGraph | NormalizedSubgraph,
  config: Subgraph,
): void {
  const nodeDefaults = owner.resolvedNodeDefaults();
  if (config.nodes) {
    for (const { name, attributes } of config.nodes) {
      const node = owner.upsertNode(name, nodeDefaults);
      if (attributes) {
        node.mergeAttributes(normalizeAttributes(attributes));
      }
    }
  }

  if (config.edges) {
    const edgeDefaults = owner.resolvedEdgeDefaults();
    const edgeDefaultConfigAttributes =
      extractEdgeConfigAttributes(edgeDefaults);

    for (const edgeConfig of config.edges) {
      let attributes;
      let configAttributes = edgeDefaultConfigAttributes;
      if (edgeConfig.attributes) {
        attributes = normalizeAttributes(edgeConfig.attributes);
        configAttributes = {
          ...configAttributes,
          ...extractEdgeConfigAttributes(attributes),
        };
      }

      const { key, tailport, headport } = configAttributes;
      const edge = owner.upsertEdge(
        {
          tail: owner
            .upsertNode(edgeConfig.tail, nodeDefaults)
            .upsertEdgeEndpoint(tailport?.[0], tailport?.[1]),
          head: owner
            .upsertNode(edgeConfig.head, nodeDefaults)
            .upsertEdgeEndpoint(headport?.[0], headport?.[1]),
          key,
        },
        edgeDefaults,
      );
      if (attributes) {
        edge.mergeAttributes(attributes);
      }
    }
  }

  if (config.subgraphs) {
    for (const subgraphConfig of config.subgraphs) {
      const subgraph = owner.upsertSubgraph({
        name: subgraphConfig.name,
        graphAttributes: subgraphConfig.graphAttributes
          ? normalizeAttributes(subgraphConfig.graphAttributes)
          : new NormalizedAttributes(),
        nodeAttributes: subgraphConfig.nodeAttributes
          ? normalizeAttributes(subgraphConfig.nodeAttributes)
          : new NormalizedAttributes(),
        edgeAttributes: subgraphConfig.edgeAttributes
          ? normalizeAttributes(subgraphConfig.edgeAttributes)
          : new NormalizedAttributes(),
      });
      applyDefinitions(subgraph, subgraphConfig);
    }
  }
}

interface EdgeConfigAttributes {
  key?: string | undefined;
  tailport?: [string | undefined, string | undefined];
  headport?: [string | undefined, string | undefined];
}

export function extractEdgeConfigAttributes(
  attributes: NormalizedAttributes,
): EdgeConfigAttributes {
  const result: EdgeConfigAttributes = {};

  if (attributes.has('key')) {
    const value = attributes.get('key');
    /* v8 ignore start */
    if (value?.html !== undefined) {
      throw new TypeError(`HTML as edge 'key' is not supported`);
    }
    /* v8 ignore stop */
    result.key = value?.text;
    attributes.delete('key');
  }

  if (attributes.has('tailport')) {
    const value = attributes.get('tailport');
    /* v8 ignore start */
    if (value?.html !== undefined) {
      throw new TypeError(`HTML as 'tailport' is not supported`);
    }
    /* v8 ignore stop */
    result.tailport = splitPortString(value?.text);
    attributes.delete('tailport');
  }

  if (attributes.has('headport')) {
    const value = attributes.get('headport');
    /* v8 ignore start */
    if (value?.html !== undefined) {
      throw new TypeError(`HTML as 'headport' is not supported`);
    }
    /* v8 ignore stop */
    result.headport = splitPortString(value?.text);
    attributes.delete('headport');
  }

  return result;
}

function splitPortString(
  str: string | undefined,
): [string | undefined, string | undefined] {
  if (str === undefined) {
    return [undefined, undefined];
  }
  // FIXME: missing validation of compass
  const [port, compass] = str.split(':') as [string, string | undefined];
  return [port === '' ? undefined : port, compass === '' ? undefined : compass];
}
