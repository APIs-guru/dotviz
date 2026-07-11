import type { Attributes, Graph, Subgraph } from './graph.d.ts';
import { cmpNumbersAsc } from './utils.ts';
import { type OverrideAttributes } from './viz.ts';

export declare const enum AllNodesIndex {}
export interface AllNodesArray<T> extends IndexedArray<AllNodesIndex, T> {
  [n: AllNodesIndex & number]: T;
}

export declare const enum PortsIndex {}
export const defaultPortIndex: PortsIndex = 0;
export interface PortsArray<T> extends IndexedArray<PortsIndex, T> {
  [n: PortsIndex & number]: T;
}

export declare const enum AllEdgesIndex {}
export interface AllEdgesArray<T> extends IndexedArray<AllEdgesIndex, T> {
  [n: AllEdgesIndex & number]: T;
}

export declare const enum AllSubgraphsIndex {}
export interface AllSubgraphsArray<T> extends IndexedArray<
  AllSubgraphsIndex,
  T
> {
  [n: AllSubgraphsIndex & number]: T;
}

interface IndexedArray<Index, Item> {
  length: Index;
  map<T>(fn: (node: Item) => T): T[];
  push(node: Item): void;
  values(): ArrayIterator<Item>;
  [Symbol.iterator](): ArrayIterator<Item>;
}

interface NormalizedGraphConfig {
  readonly name: string | undefined;
  readonly strict: boolean;
  readonly directed: boolean;
  readonly graphAttributes: Iterable<AttributePair>;
  readonly nodeAttributes: Iterable<AttributePair>;
  readonly edgeAttributes: Iterable<AttributePair>;
}

export class NormalizedGraph {
  readonly #overrideGraphAttributeNames = new Set<string>();
  readonly #overrideNodeAttributeNames = new Set<string>();
  readonly #overrideEdgeAttributeNames = new Set<string>();
  readonly owner = undefined;
  readonly root = this;

  readonly name: string | undefined;
  readonly strict: boolean;
  readonly directed: boolean;
  readonly graphAttributes: NormalizedAttributes;
  readonly nodeAttributes: NormalizedAttributes;
  readonly edgeAttributes: NormalizedAttributes;
  readonly allNodes: AllNodesArray<NormalizedNode> = [];
  readonly allNamedNodes = new Map<string, AllNodesIndex>();
  readonly allEdges: AllEdgesArray<NormalizedEdge> = [];
  readonly allSubgraphs: AllSubgraphsArray<NormalizedSubgraph> = [];
  readonly subgraphs: AllSubgraphsIndex[] = [];
  readonly namedSubgraphs = new Map<string, AllSubgraphsIndex>();
  readonly #deduplicatedEdgesMap = new Map<string, AllEdgesIndex>();

  constructor(
    config: NormalizedGraphConfig,
    overrideAttributes: OverrideAttributes,
  ) {
    this.graphAttributes = new NormalizedAttributes(config.graphAttributes);
    if (overrideAttributes.graphAttributes !== undefined) {
      for (const [key, value] of normalizeAttributes(
        overrideAttributes.graphAttributes,
      )) {
        this.#overrideGraphAttributeNames.add(key);
        this.graphAttributes.set(key, value);
      }
    }

    this.nodeAttributes = new NormalizedAttributes(config.nodeAttributes);
    if (overrideAttributes.nodeAttributes !== undefined) {
      for (const [key, value] of normalizeAttributes(
        overrideAttributes.nodeAttributes,
      )) {
        this.#overrideNodeAttributeNames.add(key);
        this.nodeAttributes.set(key, value);
      }
    }

    this.edgeAttributes = new NormalizedAttributes(config.edgeAttributes);
    if (overrideAttributes.edgeAttributes !== undefined) {
      for (const [key, value] of normalizeAttributes(
        overrideAttributes.edgeAttributes,
      )) {
        this.#overrideEdgeAttributeNames.add(key);
        this.edgeAttributes.set(key, value);
      }
    }

    this.name = config.name;
    this.strict = config.strict;
    this.directed = config.directed;
  }

  mergeGraphAttributes(newAttributes: Iterable<AttributePair>) {
    const defaultAttributes: AttributePair[] = [];
    for (const [key, value] of newAttributes) {
      if (!this.#overrideGraphAttributeNames.has(key)) {
        const oldValue = this.graphAttributes.get(key);
        this.graphAttributes.set(key, value);
        // Seed with the current value (or `undefined`) so existing subgraphs retain whatever was in effect before this change.
        defaultAttributes.push([key, oldValue]);
      }
    }

    const { allSubgraphs } = this;
    for (const subgraphIndex of this.subgraphs) {
      allSubgraphs[subgraphIndex].applyDefaultGraphAttributes(
        defaultAttributes,
      );
    }
  }

  mergeNodeAttributes(newAttributes: Iterable<AttributePair>) {
    const defaultAttributes: AttributePair[] = [];
    for (const [key, value] of newAttributes) {
      if (!this.#overrideNodeAttributeNames.has(key)) {
        this.nodeAttributes.set(key, value);
        // Seed existing nodes with `undefined` so a later-declared default doesn't retroactively win
        defaultAttributes.push([key, undefined]);
      }
    }

    for (const node of this.allNodes) {
      node.applyDefaultAttributes(defaultAttributes);
    }
  }

  mergeEdgeAttributes(newAttributes: Iterable<AttributePair>) {
    const defaultAttributes: AttributePair[] = [];
    for (const [key, value] of newAttributes) {
      if (!this.#overrideEdgeAttributeNames.has(key)) {
        this.edgeAttributes.set(key, value);
        // Seed existing edges with `undefined` so a later-declared default doesn't retroactively win
        defaultAttributes.push([key, undefined]);
      }
    }

    for (const edge of this.allEdges) {
      edge.applyDefaultAttributes(defaultAttributes);
    }
  }

  upsertNode(
    name: string,
    nodeDefaults: Iterable<AttributePair>,
  ): NormalizedNode {
    const { allNodes, allNamedNodes } = this;
    const newIndex = allNodes.length;
    const nodeIndex = allNamedNodes.getOrInsert(name, newIndex);
    if (nodeIndex !== newIndex) {
      return allNodes[nodeIndex];
    }

    allNamedNodes.set(name, newIndex);
    const newNode = new NormalizedNode(newIndex, {
      name,
      attributes: nodeDefaults,
    });
    allNodes.push(newNode);
    return newNode;
  }

  upsertEdgeEndpoint(
    config: NormalizedEdgeEndpointConfig,
  ): NormalizedEdgeEndpoint {
    const { node, portName, compass } = config;

    if (portName === undefined) {
      if (compass === undefined) {
        return this.allNodes[node].defaultEndpoint;
      }
      return { node, port: defaultPortIndex, compass };
    }

    const port = this.allNodes[node].upsertPort(portName);
    return { node, port, compass };
  }

  upsertEdge(
    config: NormalizedEdgeConfig,
    edgeDefaults: Iterable<AttributePair>,
  ): NormalizedEdge {
    const { allEdges } = this;
    const newIndex = allEdges.length;
    const deduplicateKey = this.edgeDeduplicateKey(config);
    if (deduplicateKey !== undefined) {
      const edgeIndex = this.#deduplicatedEdgesMap.getOrInsert(
        deduplicateKey,
        newIndex,
      );
      if (edgeIndex !== newIndex) {
        return allEdges[edgeIndex];
      }
    }

    const newEdge = new NormalizedEdge(newIndex, config, edgeDefaults);
    allEdges.push(newEdge);
    return newEdge;
  }

  edgeDeduplicateKey(config: NormalizedEdgeConfig): string | undefined {
    const { key } = config;

    if (key === undefined && !this.strict) {
      return undefined;
    }

    let { tail, head } = config;
    if (!this.directed) {
      if (
        head.node > tail.node ||
        (head.node === tail.node && head.port > tail.port)
      ) {
        [tail, head] = [head, tail];
      }
    }

    return [
      tail.node.toString(),
      tail.port.toString(),
      head.node.toString(),
      head.port.toString(),
      key ?? '',
    ].join(':');
  }

  upsertSubgraph(config: NormalizedSubgraphConfig): NormalizedSubgraph {
    const { allSubgraphs } = this;
    const newIndex = allSubgraphs.length;
    const { name } = config;

    if (name !== undefined) {
      const subgraphIndex = this.namedSubgraphs.getOrInsert(name, newIndex);
      if (subgraphIndex !== newIndex) {
        return allSubgraphs[subgraphIndex];
      }
    }

    this.subgraphs.push(newIndex);
    const newSubgraph = new NormalizedSubgraph(newIndex, this, config);
    allSubgraphs.push(newSubgraph);
    return newSubgraph;
  }

  resolvedNodeDefaults(): NormalizedAttributes {
    return this.nodeAttributes;
  }

  resolvedEdgeDefaults(): NormalizedAttributes {
    return this.edgeAttributes;
  }
}

export interface NormalizedPort {
  readonly index: PortsIndex;
  readonly name: string | undefined;
}

export interface NormalizedNodeConfig {
  readonly name: string;
  readonly attributes: Iterable<AttributePair>;
}

export class NormalizedNode {
  static readonly defaultPort: PortsIndex = 0;
  readonly index: AllNodesIndex;
  readonly name: string;
  readonly defaultEndpoint: NormalizedEdgeEndpoint;
  readonly ports: PortsArray<NormalizedPort> = [{ index: 0, name: undefined }];
  readonly namedPorts = new Map<string, PortsIndex>();
  readonly attributes: NormalizedAttributes;

  constructor(index: AllNodesIndex, config: NormalizedNodeConfig) {
    this.index = index;
    this.name = config.name;
    this.defaultEndpoint = { node: index, port: 0, compass: undefined };
    this.attributes = new NormalizedAttributes(config.attributes);
  }

  mergeAttributes(newAttributes: Iterable<AttributePair>): void {
    for (const [key, value] of newAttributes) {
      this.attributes.set(key, value);
    }
  }

  applyDefaultAttributes(defaults: Iterable<AttributePair>): void {
    for (const [key, value] of defaults) {
      this.attributes.getOrInsert(key, value);
    }
  }

  upsertPort(portName: string): PortsIndex {
    let portIndex = this.namedPorts.get(portName);
    if (portIndex !== undefined) {
      return portIndex;
    }

    portIndex = this.ports.length;
    this.ports.push({ index: portIndex, name: portName });
    this.namedPorts.set(portName, portIndex);
    return portIndex;
  }
}

export interface NormalizedEdgeEndpoint {
  readonly node: AllNodesIndex;
  readonly port: PortsIndex;
  readonly compass: string | undefined;
}

export interface NormalizedEdgeEndpointConfig {
  readonly node: AllNodesIndex;
  readonly portName: string | undefined;
  readonly compass: string | undefined;
}

interface NormalizedEdgeConfig {
  readonly tail: NormalizedEdgeEndpoint;
  readonly head: NormalizedEdgeEndpoint;
  readonly key: string | undefined;
}

export class NormalizedEdge {
  readonly index: AllEdgesIndex;
  readonly tail: NormalizedEdgeEndpoint;
  readonly head: NormalizedEdgeEndpoint;
  readonly key: string | undefined;
  readonly attributes: NormalizedAttributes;

  constructor(
    index: AllEdgesIndex,
    config: NormalizedEdgeConfig,
    attributes: Iterable<AttributePair>,
  ) {
    this.index = index;
    this.tail = config.tail;
    this.head = config.head;
    this.key = config.key;
    this.attributes = new NormalizedAttributes(attributes);
  }

  mergeAttributes(newAttributes: Iterable<AttributePair>) {
    for (const [key, value] of newAttributes) {
      this.attributes.set(key, value);
    }
  }

  applyDefaultAttributes(defaults: Iterable<AttributePair>) {
    for (const [key, value] of defaults) {
      this.attributes.getOrInsert(key, value);
    }
  }
}

interface NormalizedSubgraphConfig {
  readonly name: string | undefined;
  readonly graphAttributes: Iterable<AttributePair>;
  readonly nodeAttributes: Iterable<AttributePair>;
  readonly edgeAttributes: Iterable<AttributePair>;
}

export class NormalizedSubgraph {
  readonly root: NormalizedGraph;
  readonly owner: NormalizedGraph | NormalizedSubgraph;

  readonly index: AllSubgraphsIndex;
  readonly name: string | undefined;
  readonly graphAttributes: NormalizedAttributes;
  readonly nodeAttributes: NormalizedAttributes;
  readonly edgeAttributes: NormalizedAttributes;
  readonly memberNodes = new Set<AllNodesIndex>();
  readonly memberEdges = new Set<AllEdgesIndex>();
  readonly subgraphs: AllSubgraphsIndex[] = [];
  readonly namedSubgraphs = new Map<string, AllSubgraphsIndex>();

  constructor(
    index: AllSubgraphsIndex,
    owner: NormalizedGraph | NormalizedSubgraph,
    config: NormalizedSubgraphConfig,
  ) {
    this.root = owner.root;
    this.owner = owner;
    this.index = index;
    this.name = config.name;
    this.graphAttributes = new NormalizedAttributes(config.graphAttributes);
    this.nodeAttributes = new NormalizedAttributes(config.nodeAttributes);
    this.edgeAttributes = new NormalizedAttributes(config.edgeAttributes);
  }

  mergeGraphAttributes(newAttributes: Iterable<AttributePair>): void {
    const defaultAttributes: AttributePair[] = [];
    for (const [key, value] of newAttributes) {
      const oldValue = this.graphAttributes.get(key);
      this.graphAttributes.set(key, value);
      // Seed with the current value (or `undefined`) so existing subgraphs retain whatever was in effect before this change.
      defaultAttributes.push([key, oldValue]);
    }

    const { allSubgraphs } = this.root;
    for (const subgraphIndex of this.subgraphs) {
      allSubgraphs[subgraphIndex].applyDefaultGraphAttributes(
        defaultAttributes,
      );
    }
  }

  mergeNodeAttributes(newAttributes: Iterable<AttributePair>): void {
    for (const [key, value] of newAttributes) {
      this.nodeAttributes.set(key, value);
    }
  }

  mergeEdgeAttributes(newAttributes: Iterable<AttributePair>): void {
    for (const [key, value] of newAttributes) {
      this.edgeAttributes.set(key, value);
    }
  }

  applyDefaultGraphAttributes(defaults: Iterable<AttributePair>): void {
    for (const [key, value] of defaults) {
      this.graphAttributes.set(key, value);
    }
  }

  upsertNode(
    name: string,
    defaultAttributes: Iterable<AttributePair>,
  ): NormalizedNode {
    const node = this.owner.upsertNode(name, defaultAttributes);
    this.memberNodes.add(node.index);
    return node;
  }

  upsertEdge(
    config: NormalizedEdgeConfig,
    defaultAttributes: Iterable<AttributePair>,
  ): NormalizedEdge {
    const edge = this.owner.upsertEdge(config, defaultAttributes);
    this.memberEdges.add(edge.index);
    return edge;
  }

  upsertSubgraph(
    config: Readonly<NormalizedSubgraphConfig>,
  ): NormalizedSubgraph {
    const { name } = config;
    const { allSubgraphs } = this.root;
    const newIndex = allSubgraphs.length;
    if (name !== undefined) {
      const subgraphIndex = this.namedSubgraphs.getOrInsert(name, newIndex);
      if (subgraphIndex !== newIndex) {
        return allSubgraphs[subgraphIndex];
      }
    }

    this.subgraphs.push(newIndex);
    const newSubgraph = new NormalizedSubgraph(newIndex, this, config);
    allSubgraphs.push(newSubgraph);
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

  sortedMemberNodeIndexes(): AllNodesIndex[] {
    return [...this.memberNodes].sort(cmpNumbersAsc);
  }

  sortedMemberEdgeIndexes(): AllEdgesIndex[] {
    return [...this.memberEdges].sort(cmpNumbersAsc);
  }
}

export type NormalizedAttributeValue =
  | { text: undefined; html: string }
  | { text: string; html: undefined };

export type AttributePair = [string, NormalizedAttributeValue | undefined];
export class NormalizedAttributes extends Map<
  string,
  NormalizedAttributeValue | undefined
> {
  static valueToString(value: NormalizedAttributeValue): string {
    return value.text === undefined ? `<${value.html}>` : `"${value.text}"`;
  }
}

function normalizeOptionalAttributes(
  attributes: Attributes | undefined,
): AttributePair[] {
  return attributes === undefined ? [] : normalizeAttributes(attributes);
}

function normalizeAttributes(attributes: Attributes): AttributePair[] {
  return Object.entries(attributes).map(([name, value]) => {
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
  });
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
      graphAttributes: normalizeOptionalAttributes(config.graphAttributes),
      nodeAttributes: normalizeOptionalAttributes(config.nodeAttributes),
      edgeAttributes: normalizeOptionalAttributes(config.edgeAttributes),
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
    const edgeDefaultConfigAttributes: EdgeConfigAttributes = {};
    const edgeDefaults = extractEdgeConfigAttributes(
      owner.resolvedEdgeDefaults(),
      edgeDefaultConfigAttributes,
    );

    for (const edgeConfig of config.edges) {
      const configAttributes = { ...edgeDefaultConfigAttributes };
      const attributes = extractEdgeConfigAttributes(
        normalizeOptionalAttributes(edgeConfig.attributes),
        configAttributes,
      );
      const { key, tailport, headport } = configAttributes;
      const edge = owner.upsertEdge(
        {
          tail: owner.root.upsertEdgeEndpoint({
            node: owner.upsertNode(edgeConfig.tail, nodeDefaults).index,
            portName: tailport?.portName,
            compass: tailport?.compass,
          }),
          head: owner.root.upsertEdgeEndpoint({
            node: owner.upsertNode(edgeConfig.head, nodeDefaults).index,
            portName: headport?.portName,
            compass: headport?.compass,
          }),
          key,
        },
        edgeDefaults,
      );
      edge.mergeAttributes(attributes);
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

export interface EdgeConfigAttributes {
  key?: string | undefined;
  tailport?: { portName: string | undefined; compass: string | undefined };
  headport?: { portName: string | undefined; compass: string | undefined };
}

export function extractEdgeConfigAttributes(
  attributes: Iterable<AttributePair>,
  configAttributes: EdgeConfigAttributes,
): AttributePair[] {
  const filteredPairs: AttributePair[] = [];

  for (const [key, value] of attributes) {
    switch (key) {
      case 'key': {
        /* v8 ignore start */
        if (value?.html !== undefined) {
          throw new TypeError(`HTML as edge 'key' is not supported`);
        }
        /* v8 ignore stop */
        configAttributes.key = value?.text;
        break;
      }
      case 'tailport': {
        /* v8 ignore start */
        if (value?.html !== undefined) {
          throw new TypeError(`HTML as 'tailport' is not supported`);
        }
        /* v8 ignore stop */
        configAttributes.tailport = splitPortString(value?.text);
        break;
      }
      case 'headport': {
        /* v8 ignore start */
        if (value?.html !== undefined) {
          throw new TypeError(`HTML as 'headport' is not supported`);
        }
        /* v8 ignore stop */
        configAttributes.headport = splitPortString(value?.text);
        break;
      }
      default:
        filteredPairs.push([key, value]);
    }
  }

  return filteredPairs;
}

function splitPortString(str: string | undefined): {
  portName: string | undefined;
  compass: string | undefined;
} {
  if (str === undefined) {
    return { portName: undefined, compass: undefined };
  }
  // FIXME: missing validation of compass
  let [portName, compass] = str.split(':') as [
    string | undefined,
    string | undefined,
  ];
  if (portName === '') {
    portName = undefined;
  }
  if (compass === '') {
    compass = undefined;
  }
  return { portName, compass };
}
