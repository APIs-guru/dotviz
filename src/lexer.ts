import type { Attributes, Edge, Graph, Node, Subgraph } from './graph.js';
import type { FailureResult, RenderError } from './viz.ts';

const BOM = '\uFEFF' as const;
type LiteralToken = ',' | ':' | ';' | '=' | '[' | ']' | '{' | '}' | '--' | '->';

type KeywordToken =
  | 'node'
  | 'edge'
  | 'graph'
  | 'digraph'
  | 'subgraph'
  | 'strict';

const IDType = {
  Number: 0,
  Name: 1,
  String: 2,
  HTML: 3,
} as const;
type IDType = (typeof IDType)[keyof typeof IDType];

interface ID {
  kind: 'ID';
  value: string;
  idType: IDType;
}

type Token =
  | { kind: LiteralToken }
  | { kind: KeywordToken }
  | { kind: 'EOF' }
  | ID;

class Lexer {
  #dotStr: string;
  #line = 1;
  #lineStart = 0;
  #nextIndex = 0;
  warnings: { level: 'warning'; message: string }[] = [];

  constructor(dotStr: string) {
    this.#dotStr = dotStr;
    this.#skipUntilTokenStart();
  }

  nextToken(): Token {
    const token = this.#readNextToken();
    this.#skipUntilTokenStart();
    return token;
  }

  isEOF(): boolean {
    return this.#nextIndex >= this.#dotStr.length;
  }

  peekIsLiteral(literal: LiteralToken): boolean {
    return this.#dotStr.startsWith(literal, this.#nextIndex);
  }

  peekIsKeyword(keyword: KeywordToken): boolean {
    const maybeKeyword = this.#dotStr.slice(
      this.#nextIndex,
      this.#nextIndex + keyword.length,
    );
    if (maybeKeyword.toLowerCase() === keyword) {
      const nextChar = this.#dotStr[this.#nextIndex + keyword.length];
      return !isNameContinue(nextChar);
    }
    return false;
  }

  optionalID(): ID | undefined {
    const token = this.#readKeywordOrID();
    if (token?.kind == 'ID') {
      this.#skipUntilTokenStart();
      return token;
    }
    if (token !== undefined) {
      this.#nextIndex -= token.kind.length;
    }
    return undefined;
  }

  expectID(description: string): ID {
    const id = this.optionalID();
    if (id !== undefined) {
      return id;
    }
    throw new Error(
      `Unexpected ${tokenStr(this.#readNextToken())}, expected ${description}!`,
    );
  }

  expectedLiteral(literal: LiteralToken): void {
    if (!this.optionalLiteral(literal)) {
      throw new Error(
        `Unexpected ${tokenStr(this.#readNextToken())}, expected ${kindStr(literal)}!`,
      );
    }
  }

  optionalLiteral(kind: LiteralToken): boolean {
    if (this.peekIsLiteral(kind)) {
      this.#nextIndex += kind.length;
      this.#skipUntilTokenStart();
      return true;
    }
    return false;
  }

  optionalKeyword(keyword: KeywordToken): boolean {
    if (this.peekIsKeyword(keyword)) {
      this.#nextIndex += keyword.length;
      this.#skipUntilTokenStart();
      return true;
    }
    return false;
  }

  #readNextToken(): Token {
    const token = this.#readLiteral() ?? this.#readKeywordOrID();
    if (token !== undefined) {
      return token;
    }

    const char = this.#peekNextChar();
    if (char === undefined) {
      return { kind: 'EOF' };
    }

    const line = this.#line.toString();
    const column = (this.#nextIndex - this.#lineStart + 1).toString();
    throw new Error(`(${line}:${column})Unexpected character: '${char}'`);
  }

  #readLiteral(): { kind: LiteralToken } | undefined {
    const tokenStartChar = this.#peekNextChar();
    switch (tokenStartChar) {
      case ',':
      case ':':
      case ';':
      case '=':
      case '[':
      case ']':
      case '{':
      case '}':
        this.#nextIndex += 1;
        return { kind: tokenStartChar };

      case '-': {
        const nextChar = this.#peekNextChar(1);
        if (nextChar === '-') {
          this.#nextIndex += 2;
          return { kind: '--' };
        } else if (nextChar === '->') {
          this.#nextIndex += 2;
          return { kind: '->' };
        }
        break;
      }
    }

    return undefined;
  }

  #readKeywordOrID(): { kind: KeywordToken } | ID | undefined {
    const tokenStartIndex = this.#nextIndex;
    const tokenStartChar = this.#peekNextChar();
    if (tokenStartChar === '<') {
      const value = this.#readHTML();
      return { kind: 'ID', idType: IDType.HTML, value };
    } else if (tokenStartChar === '"') {
      const value = this.#readString();
      return { kind: 'ID', idType: IDType.String, value };
    } else if (isNumberStart(tokenStartChar)) {
      const value = this.#readNumber();
      const token = { kind: 'ID' as const, idType: IDType.Number, value };
      this.#warnIfAmbiguous(tokenStartIndex, token);
      return token;
    } else if (isNameStart(tokenStartChar)) {
      const value = this.#readName();
      const maybeKeyword = value.toLowerCase();
      let token: { kind: KeywordToken } | ID;
      switch (maybeKeyword) {
        case 'node':
        case 'edge':
        case 'graph':
        case 'digraph':
        case 'subgraph':
        case 'strict':
          token = { kind: maybeKeyword };
          break;
        default:
          token = { kind: 'ID', idType: IDType.Name, value };
      }
      this.#warnIfAmbiguous(tokenStartIndex, token);
      return token;
    }
    return undefined;
  }

  #warnIfAmbiguous(tokenStartIndex: number, lastToken: Token) {
    const nextChar = this.#peekNextChar();
    if (isNumberContinue(nextChar) || isNameContinue(nextChar)) {
      const ambiguousText =
        ellipsize(this.#dotStr.slice(tokenStartIndex, this.#nextIndex)) +
        nextChar;
      this.warnings.push({
        level: 'warning',
        message: `Ambiguous token sequence: '${ambiguousText}' will be split into ${tokenStr(lastToken)} and a following token. If you want it interpreted as a single value, use quotes: "${ambiguousText}". Otherwise, use whitespace or other delimiters to separate tokens.`,
      });
    }
  }

  #skipUntilTokenStart() {
    while (true) {
      switch (this.#peekNextChar()) {
        // Ignored:
        case BOM:
        case '\n':
        case '\r':
        case '\t':
        case ' ':
          this.#readNextChar();
          break;
        case '#': {
          const nextLine = this.#line + 1;
          while (this.#line !== nextLine && this.#readNextChar() !== undefined);
          break;
        }
        case '/':
          if (this.#skipStr('/*')) {
            while (!this.#skipStr('*/') && this.#readNextChar() !== undefined);
            break;
          } else if (this.#skipStr('//')) {
            const nextLine = this.#line + 1;
            while (
              this.#line !== nextLine &&
              this.#readNextChar() !== undefined
            );
            break;
          }
          return;
        default:
          return;
      }
    }
  }

  #readNumber(): string {
    const valueStart = this.#nextIndex;
    this.#skipChar('-');
    if (this.#peekNextChar() !== '.') {
      this.#readDigits();
    }
    if (this.#skipChar('.')) {
      this.#readDigits();
    }

    return this.#dotStr.slice(valueStart, this.#nextIndex);
  }

  #readDigits(): void {
    while (isDigit(this.#peekNextChar())) {
      this.#readNextChar();
    }
  }

  #readHTML(): string {
    const line = this.#line.toString();
    const column = (this.#nextIndex - this.#lineStart + 1).toString();

    const valueStart = this.#nextIndex + 1;
    let unclosedAngleBrackets = 0;
    do {
      switch (this.#readNextChar()) {
        case undefined: {
          const value = this.#dotStr.slice(valueStart, this.#nextIndex);
          throw new Error(
            `(${line}:${column})Unterminated HTML string, missing closing '>' in: '<${ellipsize(value)}'`,
          );
        }
        case '<':
          ++unclosedAngleBrackets;
          break;
        case '>':
          --unclosedAngleBrackets;
          break;
      }
    } while (unclosedAngleBrackets !== 0);

    return this.#dotStr.slice(valueStart, this.#nextIndex - 1);
  }

  #readString(): string {
    const line = this.#line.toString();
    const column = (this.#nextIndex - this.#lineStart + 1).toString();

    this.#readNextChar(); // skip opening `"`
    const valueStart = this.#nextIndex;
    while (this.#peekNextChar(-1) === '\\' || !this.#skipChar('"')) {
      switch (this.#readNextChar()) {
        case undefined: {
          const value = this.#dotStr.slice(valueStart, this.#nextIndex);
          throw new Error(
            `(${line}:${column})Unterminated string, missing closing '"' in: '"${ellipsize(value)}'`,
          );
        }
      }
    }

    return this.#dotStr
      .slice(valueStart, this.#nextIndex - 1)
      .replaceAll('\\\r\n', '')
      .replaceAll('\\\r', '')
      .replaceAll('\\\n', '');
  }

  #readName(): string {
    const valueStart = this.#nextIndex;
    this.#readNextChar();
    while (isNameContinue(this.#peekNextChar())) {
      this.#readNextChar();
    }

    return this.#dotStr.slice(valueStart, this.#nextIndex);
  }

  #skipChar(char: string): boolean {
    if (this.#peekNextChar() === char) {
      this.#readNextChar();
      return true;
    }
    return false;
  }

  #skipStr(str: string): boolean {
    if (this.#dotStr.startsWith(str, this.#nextIndex)) {
      this.#nextIndex += str.length;
      return true;
    }
    return false;
  }

  #peekNextChar(offset = 0): string | undefined {
    return this.#dotStr[this.#nextIndex + offset];
  }

  #readNextChar(): string | undefined {
    const charIndex = this.#nextIndex++;
    const char = this.#dotStr[charIndex];
    if (char === '\r') {
      if (this.#peekNextChar() !== '\n') {
        // "Carriage Return (U+000D)" [lookahead != "New Line (U+000A)"]
        ++this.#line;
        this.#lineStart = charIndex;
      }
    } else if (char === '\n') {
      // "New Line (U+000A)"
      ++this.#line;
      this.#lineStart = charIndex;
    }
    return char;
  }
}

function idStr({ idType, value }: ID): string {
  switch (idType) {
    case IDType.Number:
      return `number '${value}'`;
    case IDType.Name:
      return `identifier '${value}'`;
    case IDType.String:
      return `string "${ellipsize(value)}"`;
    case IDType.HTML:
      return `html "${ellipsize(value)}"`;
    default:
      return `unknown token '${value}'`;
  }
}

function kindStr(kind: LiteralToken | KeywordToken | 'EOF'): string {
  switch (kind) {
    case ',':
    case ':':
    case ';':
    case '=':
    case '[':
    case ']':
    case '{':
    case '}':
    case '--':
    case '->':
      return `'${kind}'`;
    case 'node':
    case 'edge':
    case 'graph':
    case 'digraph':
    case 'subgraph':
    case 'strict':
      return `keyword '${kind}'`;
    case 'EOF':
      return 'end of file';
  }
}
function tokenStr(token: Token): string {
  return token.kind === 'ID' ? idStr(token) : kindStr(token.kind);
}

function ellipsize(str: string): string {
  return str.length > 20 ? str.slice(0, 17) + '...' : str;
}

type DigitChars = '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9';
function isDigit(ch: string | undefined): ch is DigitChars {
  return ch !== undefined && ch >= '0' && ch <= '9';
}

type NumberStartChars = NumberContinueChars | '-';
function isNumberStart(ch: string | undefined): ch is NumberStartChars {
  return isNumberContinue(ch) || ch === '-';
}

type NumberContinueChars = DigitChars | '.';
function isNumberContinue(ch: string | undefined): ch is NumberContinueChars {
  return isDigit(ch) || ch === '.';
}

// prettier-ignore
type LetterChars = 'A' | 'B' | 'C' | 'D' | 'E' | 'F' | 'G' | 'H' | 'I' | 'J' | 'K' | 'L' | 'M' | 'N' | 'O' | 'P' | 'Q' | 'R' | 'S' | 'T' | 'U' | 'V' | 'W' | 'X' | 'Y' | 'Z' | 'a' | 'b' | 'c' | 'd' | 'e' | 'f' | 'g' | 'h' | 'i' | 'j' | 'k' | 'l' | 'm' | 'n' | 'o' | 'p' | 'q' | 'r' | 's' | 't' | 'u' | 'v' | 'w' | 'x' | 'y' | 'z';
function isLetter(ch: string): ch is LetterChars {
  return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

function isASCII(ch: string): ch is string {
  const code = ch.codePointAt(0);
  return code != undefined && code <= 127;
}

function isNameStart(ch: string | undefined): ch is string {
  if (ch === undefined) return false;
  return isLetter(ch) || ch === '_' || !isASCII(ch);
}

function isNameContinue(ch: string | undefined): ch is string {
  return isNameStart(ch) || isDigit(ch);
}

export interface ParseSuccessResult {
  status: 'success';
  output: Graph;
  errors: RenderError[];
}

export type ParseResult = ParseSuccessResult | FailureResult;
export function parseDot(dotStr: string): ParseResult {
  try {
    const lexer = new Lexer(dotStr);
    if (lexer.isEOF()) {
      throw new Error('Missing graph definition!');
    }

    const graph = parseGraph(lexer);
    // console.log(JSON.stringify(graph, null, 2));
    return {
      status: 'success',
      output: graph,
      errors: lexer.warnings,
    };
  } catch (error: unknown) {
    return {
      status: 'failure',
      output: null,
      errors: [
        {
          level: 'error',
          message: error instanceof Error ? error.message : String(error),
        },
      ],
    };
  }
}

class StatementList {
  inheritGraphAttributes: Attributes = {};
  inheritNodeAttributes: Attributes = {};
  inheritEdgeAttributes: Attributes = {};
  graphAttributes: Attributes = {};
  nodeAttributes: Attributes = {};
  edgeAttributes: Attributes = {};

  referencedNodes = new Set<number>();
  ownedNodes = new Set<number>();

  referencedEdges = new Set<number>();
  ownedEdges = new Set<number>();

  subgraphs = new Map<number | string, StatementList>();

  constructor(parent: StatementList | undefined) {
    if (parent) {
      this.inheritGraphAttributes = {
        ...parent.inheritGraphAttributes,
        ...parent.graphAttributes,
      };
      this.inheritNodeAttributes = {
        ...parent.inheritNodeAttributes,
        ...parent.nodeAttributes,
      };
      this.inheritEdgeAttributes = {
        ...parent.inheritEdgeAttributes,
        ...parent.edgeAttributes,
      };
    }
  }

  sortedReferencedNodes(): number[] {
    return [...this.referencedNodes].toSorted((a, b) => a - b);
  }

  sortedReferenceEdges(): number[] {
    return [...this.referencedEdges].toSorted((a, b) => a - b);
  }
}

type NodeID = [string, string | undefined];

function parseGraph(lexer: Lexer): Graph {
  // graph:	[ strict ] (graph | digraph) [ ID ] '{' stmt_list '}'
  const isStrict = lexer.optionalKeyword('strict');
  const isDirected = parseIsDirectedGraph();
  const graphName = lexer.optionalID()?.value;

  const globalNodes: Required<Node>[] = [];
  const nodeNameToIndex = new Map<string, number>();
  const globalEdges: Required<Edge>[] = [];

  const graphStatementList = new StatementList(undefined);
  // FIXME: check if it's viz.js hack or it also present in graphviz
  graphStatementList.nodeAttributes.label = String.raw`\N`;

  parseStatementList(graphStatementList);
  const graph = normalizeGraph(graphStatementList);
  return {
    strict: isStrict,
    directed: isDirected,
    name: graphName,
    graphAttributes: graph.graphAttributes,
    nodeAttributes: graph.nodeAttributes,
    edgeAttributes: graph.edgeAttributes,

    nodes: globalNodes,
    edges: isStrict ? globalEdges : graph.edges,
    subgraphs: graph.subgraphs,
  };

  function normalizeGraph(
    statementList: StatementList,
    parentGraphAttributeNames: Set<string> = new Set<string>(),
    parentNodeAttributeNames: Set<string> = new Set<string>(),
    parentEdgeAttributeNames: Set<string> = new Set<string>(),
  ): {
    graphAttributes: Attributes | undefined;
    nodeAttributes: Attributes | undefined;
    edgeAttributes: Attributes | undefined;
    nodes: Node[] | undefined;
    edges: Edge[] | undefined;
    subgraphs: Subgraph[] | undefined;
  } {
    const { graphAttributes, nodeAttributes, edgeAttributes } = statementList;

    const nodeAttributeNames = new Set<string>([
      ...parentNodeAttributeNames,
      ...Object.keys(nodeAttributes),
    ]);
    for (const nodeIndex of statementList.ownedNodes) {
      const node = globalNodes[nodeIndex];
      for (const attributeName of nodeAttributeNames) {
        node.attributes[attributeName] ??= '';
      }
    }
    const nodes: Node[] = statementList
      .sortedReferencedNodes()
      .map((nodeIndex) => {
        const node = globalNodes[nodeIndex];
        return { name: node.name };
      });

    const edgeAttributeNames = new Set<string>([
      ...parentEdgeAttributeNames,
      ...Object.keys(edgeAttributes),
    ]);
    for (const edgeIndex of statementList.ownedEdges) {
      const edge = globalEdges[edgeIndex];
      for (const attributeName of edgeAttributeNames) {
        edge.attributes[attributeName] ??= '';
      }
    }

    const edges: Edge[] = [];
    for (const edgeIndex of statementList.sortedReferenceEdges()) {
      const edge = globalEdges[edgeIndex];
      if (isStrict) {
        edges.push({ tail: edge.tail, head: edge.head });
      } else if (statementList.ownedEdges.has(edgeIndex)) {
        edges.push(edge);
      }
    }

    for (const attributeName of parentGraphAttributeNames) {
      if (statementList.inheritGraphAttributes[attributeName] === undefined) {
        graphAttributes[attributeName] ??= '';
      }
    }
    const graphAttributeNames = new Set<string>([
      ...parentGraphAttributeNames,
      ...Object.keys(graphAttributes),
    ]);
    const subgraphs: Subgraph[] = [...statementList.subgraphs.entries()].map(
      ([id, subgraphStatementList]) => {
        const name = typeof id === 'string' ? id : undefined;
        return {
          ...normalizeGraph(
            subgraphStatementList,
            graphAttributeNames,
            nodeAttributeNames,
            edgeAttributeNames,
          ),
          name,
        };
      },
    );

    return {
      graphAttributes,
      nodeAttributes,
      edgeAttributes,
      nodes,
      edges,
      subgraphs,
    };
  }

  function parseStatementList(statementList: StatementList): void {
    // '{' stmt_list '}'
    lexer.expectedLiteral('{');

    while (!lexer.optionalLiteral('}')) {
      if (lexer.isEOF()) {
        throw new Error(
          `Unexpected end of file, expected ${kindStr('}')} before the end of the graph!`,
        );
      }

      // stmt_list	:	[ stmt [ ';' ] stmt_list ]
      parseStatement(statementList);
      lexer.optionalLiteral(';');
    }

    // for (const edge of graph.edges) {
    //   if (edge.attributes) {
    //     edge.attributes = { ...defaultEdgeAttributes, ...edge.attributes };
    //   }
    // }
  }

  function parseStatement(statementList: StatementList): void {
    // stmt: node_stmt |	edge_stmt |	attr_stmt |	ID '=' ID |	subgraph
    if (lexer.peekIsLiteral('{')) {
      const subgraph = parseSubgraph(undefined, statementList);
      if (optionalEdgeOp()) {
        const tailNodes: NodeID[] = subgraph
          .sortedReferencedNodes()
          .map((nodeIndex) => [globalNodes[nodeIndex].name, undefined]);
        parseEdges(tailNodes, statementList);
      }
      return;
    }

    const token = lexer.nextToken();
    switch (token.kind) {
      case 'ID': {
        if (lexer.peekIsLiteral('=')) {
          // ID '=' ID
          parseAttr(token, statementList.graphAttributes);
          break;
        }

        const nodeID: NodeID = [token.value, optionalNodePort()];
        const nodeIndex = makeNode(nodeID, statementList);

        if (optionalEdgeOp()) {
          parseEdges([nodeID], statementList);
        } else {
          // node_stmt: node_id [ attr_list ]
          if (lexer.peekIsLiteral('[')) {
            parseAttrList(globalNodes[nodeIndex].attributes);
          }
        }
        break;
      }
      // case 'subgraph':
      //   break;

      // attr_stmt:	(graph | node | edge) attr_list
      case 'graph':
        parseAttrList(statementList.graphAttributes);
        break;
      case 'node':
        parseAttrList(statementList.nodeAttributes);
        break;
      case 'edge':
        parseAttrList(statementList.edgeAttributes);
        break;
      default:
        throw new Error(
          `Unexpected ${tokenStr(token)}, expected node, edge, subgraph or attribute statement!`,
        );
    }
  }

  function parseEdges(tailNodes: NodeID[], statementList: StatementList) {
    const newEdges = new Set<number>();
    do {
      let headNodes: NodeID[];
      if (lexer.peekIsLiteral('{')) {
        const subgraph = parseSubgraph(undefined, statementList);
        headNodes = subgraph
          .sortedReferencedNodes()
          .map((nodeIndex) => [globalNodes[nodeIndex].name, undefined]);
      } else {
        const head = parseNodeID();
        makeNode(head, statementList);
        headNodes = [head];
      }

      for (const tail of tailNodes) {
        for (const head of headNodes) {
          newEdges.add(makeEdge(tail, head, statementList));
        }
      }
      tailNodes = headNodes;
    } while (optionalEdgeOp());

    if (lexer.peekIsLiteral('[')) {
      const attributes = {};
      parseAttrList(attributes);
      for (const edgeIndex of newEdges) {
        Object.assign(globalEdges[edgeIndex].attributes, attributes);
      }
    }
  }

  function parseSubgraph(
    name: string | undefined,
    statementList: StatementList,
  ): StatementList {
    const subgraph = new StatementList(statementList);
    statementList.subgraphs.set(statementList.subgraphs.size, subgraph);
    parseStatementList(subgraph);
    statementList.referencedNodes = statementList.referencedNodes.union(
      subgraph.referencedNodes,
    );
    statementList.referencedEdges = statementList.referencedEdges.union(
      subgraph.referencedEdges,
    );
    return subgraph;
  }

  function makeNode(nodeID: NodeID, statementList: StatementList): number {
    // FIXME: check that it's safe to ignore port
    const [name] = nodeID;
    let nodeIndex = nodeNameToIndex.get(name);
    if (nodeIndex === undefined) {
      nodeIndex = globalNodes.length;
      globalNodes.push({
        name,
        attributes: {
          ...statementList.inheritNodeAttributes,
          ...statementList.nodeAttributes,
        },
      });
      nodeNameToIndex.set(name, nodeIndex);
      statementList.ownedNodes.add(nodeIndex);
    }
    statementList.referencedNodes.add(nodeIndex);
    return nodeIndex;
  }

  function makeEdge(
    tail: NodeID,
    head: NodeID,
    statementList: StatementList,
  ): number {
    // FIXME: handle isStrict and ports should be merged
    const edgeIndex = globalEdges.length;
    globalEdges.push({
      head: head[0],
      tail: tail[0],
      attributes: {
        headport: head[1],
        tailport: tail[1],
        ...statementList.inheritEdgeAttributes,
        ...statementList.edgeAttributes,
      },
    });
    statementList.ownedEdges.add(edgeIndex);
    statementList.referencedEdges.add(edgeIndex);
    return edgeIndex;
  }

  function parseNodeID(): NodeID {
    // node_id:	ID [ port ]
    const id = lexer.expectID('node name').value;

    // port: ':' ID [ ':' compass_pt ]
    let port: string | undefined;
    if (lexer.optionalLiteral(':')) {
      port = lexer.expectID('port name').value;

      // compass_pt: n | ne | e | se | s | sw | w | nw | c | _
      if (lexer.optionalLiteral(':')) {
        port += ':' + lexer.expectID('compass point values').value;
      }
    }
    return [id, port];
  }

  function optionalNodePort(): string | undefined {
    // port: ':' ID [ ':' compass_pt ]
    let port: string | undefined;
    if (lexer.optionalLiteral(':')) {
      port = lexer.expectID('port name').value;

      // compass_pt: n | ne | e | se | s | sw | w | nw | c | _
      if (lexer.optionalLiteral(':')) {
        port += ':' + lexer.expectID('compass point values').value;
      }
    }
    return port;
  }

  function optionalEdgeOp(): boolean {
    if (lexer.optionalLiteral('--')) {
      if (isDirected) {
        throw new Error(
          `Unexpected keyword '--' in directed graph, expected keyword '->'!`,
        );
      }
      return true;
    }
    if (lexer.optionalLiteral('->')) {
      if (!isDirected) {
        throw new Error(
          `Unexpected keyword '->' in undirected graph, expected keyword '--'!`,
        );
      }
      return true;
    }
    return false;
  }

  function parseIsDirectedGraph(): boolean {
    if (lexer.optionalKeyword('graph')) {
      return false;
    } else if (lexer.optionalKeyword('digraph')) {
      return true;
    }
    throw new Error(
      `Unexpected ${tokenStr(lexer.nextToken())}, expected keyword 'graph' or 'digraph'!`,
    );
  }

  function parseAttrList(attributes: Attributes): void {
    // attr_list:	'[' [ a_list ] ']' [ attr_list ]
    do {
      lexer.expectedLiteral('[');
      // a_list: ID '=' ID [ (';' | ',') ] [ a_list ]
      while (!lexer.optionalLiteral(']')) {
        parseAttr(lexer.expectID('attribute name'), attributes);
        // Either ';' or ',' but doesn't allow both
        if (!lexer.optionalLiteral(';')) {
          lexer.optionalLiteral(',');
        }
      }
    } while (lexer.peekIsLiteral('['));
  }

  function parseAttr(name: ID, attributes: Attributes) {
    lexer.expectedLiteral('=');
    const value = lexer.expectID('attribute value');
    // FIXME: handle string escape characters
    attributes[name.value] =
      value.idType === IDType.HTML ? { html: value.value } : value.value;
  }
}
