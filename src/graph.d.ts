/**
 * In addition to strings in {@link https://www.graphviz.org/doc/info/lang.html | DOT syntax}, {@link Viz.render | rendering methods} accept <i>graph objects</i>.
 *
 * Graph objects are plain JavaScript objects, similar to {@link https://jsongraphformat.info | JSON Graph} or {@link https://github.com/dagrejs/graphlib/wiki/API-Reference#json-write | the Dagre JSON serialization}, but are specifically designed for working with Graphviz. Because of that, they use terminology from the Graphviz API (edges have a "head" and "tail", and nodes are identified with "name") and support features such as subgraphs, HTML labels, and default attributes.
 *
 * Some example graph objects and the corresponding graph in DOT:
 *
 * ## Empty directed graph
 *
 * ```json
 * {}
 * ```
 *
 * ```
 * digraph { }
 * ```
 *
 * ## Simple Undirected Graph
 *
 * ```json
 * {
 *   directed: false,
 *   edges: [
 *     { tail: "a", head: "b" },
 *     { tail: "b", head: "c" },
 *     { tail: "c", head: "a" }
 *   ]
 * }
 * ```
 *
 * ```
 * graph {
 *   a -- b
 *   b -- c
 *   c -- a
 * }
 * ```
 *
 * ## Attributes, Subgraphs, HTML Labels
 *
 * ```json
 * {
 *   graphAttributes: {
 *     rankdir: "LR"
 *   },
 *   nodeAttributes: {
 *     shape: "circle"
 *   },
 *   nodes: [
 *     { name: "a", attributes: { label: { html: "&lt;i&gt;A&lt;/i&gt;" }, color: "red" } },
 *     { name: "b", attributes: { label: { html: "&lt;b&gt;A&lt;/b&gt;" }, color: "green" } }
 *   ],
 *   edges: [
 *     { tail: "a", head: "b", attributes: { label: "1" } },
 *     { tail: "b", head: "c", attributes: { label: "2", headport: "name" } }
 *   ],
 *   subgraphs: [
 *     {
 *       name: "cluster_1",
 *       nodes: [
 *         {
 *           name: "c",
 *           attributes: {
 *             label: {
 *               html: "&lt;table&gt;&lt;tr&gt;&lt;td&gt;test&lt;/td&gt;&lt;td port=\"name\"&gt;C&lt;/td&gt;&lt;/tr&gt;&lt;/table&gt;"
 *             }
 *           }
 *         }
 *       ]
 *     }
 *   ]
 * }
 * ```
 *
 * ```
 * digraph {
 *   graph [rankdir="LR"]
 *   node [shape="circle"]
 *   a [label=&lt;&lt;i&gt;A&lt;/i&gt;&gt;, color="red"]
 *   b [label=&lt;&lt;b&gt;B&lt;/b&gt;&gt;, color="green"]
 *   a -> b [label="1"]
 *   b -> c:name [label="2"]
 *   subgraph cluster_1 {
 *     c [label=&lt;&lt;table&gt;&lt;tr&gt;&lt;td port="name"&gt;C&lt;/td&gt;&lt;/tr&gt;&lt;/table&gt;&gt;]
 *   }
 * }
 * ```
 */
export interface Graph {
  name?: string | undefined;
  strict?: boolean;
  directed?: boolean;
  graphAttributes?: Attributes | undefined;
  nodeAttributes?: Attributes | undefined;
  edgeAttributes?: Attributes | undefined;
  nodes?: Node[] | undefined;
  edges?: Edge[] | undefined;
  subgraphs?: Subgraph[] | undefined;
}

export type Attributes = Record<
  string,
  string | number | boolean | HTMLString | undefined
>;

export interface HTMLString {
  html: string;
}

export interface Node {
  name: string;
  attributes?: Attributes;
}

export interface Edge {
  tail: string;
  head: string;
  attributes?: Attributes;
}

export interface Subgraph {
  name?: string | undefined;
  graphAttributes?: Attributes | undefined;
  nodeAttributes?: Attributes | undefined;
  edgeAttributes?: Attributes | undefined;
  nodes?: Node[] | undefined;
  edges?: Edge[] | undefined;
  subgraphs?: Subgraph[] | undefined;
}
