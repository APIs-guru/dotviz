import type { Attributes, Edge, Graph, Node, Subgraph } from './graph.js';

const BOM = '\uFEFF' as const;
type LiteralToken = ',' | ';' | '=' | '[' | ']' | '{' | '}' | '--' | '->';

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
    if (this.#dotStr.startsWith(keyword, this.#nextIndex)) {
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
    const tokenStartChar = this.#peekNextChar();
    if (tokenStartChar === '"') {
      return { kind: 'ID', idType: IDType.String, value: this.#readString() };
    } else if (isNumberStart(tokenStartChar)) {
      return { kind: 'ID', idType: IDType.Number, value: this.#readNumber() };
    } else if (isNameStart(tokenStartChar)) {
      const value = this.#readName();
      const maybeKeyword = value.toLowerCase();
      switch (maybeKeyword) {
        case 'node':
        case 'edge':
        case 'graph':
        case 'digraph':
        case 'subgraph':
        case 'strict':
          // this.#nextIndex -= value.length; // roll back reading the token
          return { kind: maybeKeyword };
        default:
          return { kind: 'ID', idType: IDType.Name, value };
      }
    }
    return undefined;
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
          continue;

        // Comment
        // case 0x00_23: // #
        //   skipComment(lexer, position);
        //   continue;
      }
      return;
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

    // FIXME
    // if (this.#currentChar === Char['.']) {
    //   // syntax ambiguity - badly delimited number '5.5.' in line 2 of input splits into two tokens
    // }
    // syntax ambiguity - badly delimited number '5a' in line 2 of input splits into two tokens

    return this.#dotStr.slice(valueStart, this.#nextIndex);
  }

  #readDigits(): void {
    while (isDigit(this.#peekNextChar())) {
      this.#readNextChar();
    }
  }

  #readString(): string {
    const line = this.#line.toString();
    const column = (this.#nextIndex - this.#lineStart + 1).toString();

    this.#readNextChar(); // skip opening `"`
    const valueStart = this.#nextIndex;
    while (!this.#skipChar('"')) {
      switch (this.#readNextChar()) {
        case undefined: {
          const value = this.#dotStr.slice(valueStart, this.#nextIndex);
          throw new Error(
            `(${line}:${column})Unterminated string, missing closing '"' in: '"${ellipsize(value)}'`,
          );
        }
      }
    }

    return this.#dotStr.slice(valueStart, this.#nextIndex - 1);
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

function isDigit(ch: string | undefined): boolean {
  return ch !== undefined && ch >= '0' && ch <= '9';
}

function isNumberStart(ch: string | undefined): boolean {
  return isNumberContinue(ch) || ch === '-';
}

function isNumberContinue(ch: string | undefined): boolean {
  return isDigit(ch) || ch === '.';
}

function isLetter(ch: string): boolean {
  return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

function isASCII(ch: string): boolean {
  const code = ch.codePointAt(0);
  return code != undefined && code <= 127;
}

function isNameStart(ch: string | undefined): boolean {
  if (ch === undefined) return false;
  return isLetter(ch) || ch === '_' || !isASCII(ch);
}

function isNameContinue(code: string | undefined): boolean {
  return isNameStart(code) || isDigit(code);
}

export function parseDot(dotStr: string): Graph {
  const lexer = new Lexer(dotStr);
  if (lexer.isEOF()) {
    throw new Error('Missing graph definition!');
  }

  // graph:	[ strict ] (graph | digraph) [ ID ] '{' stmt_list '}'
  const isStrict = lexer.optionalKeyword('strict');
  const isDirected = parseIsDirectedGraph();
  const graphName = lexer.optionalID()?.value;
  lexer.expectedLiteral('{');

  const graphAttributes: Attributes = {};
  const nodeAttributes: Attributes = {};
  const edgeAttributes: Attributes = {};
  const nodes: Required<Node>[] = [];
  const edges: Required<Edge>[] = [];
  const subgraphs: Required<Subgraph>[] = [];

  while (!lexer.optionalLiteral('}')) {
    // stmt_list	:	[ stmt [ ';' ] stmt_list ]
    parseStatement();
    lexer.optionalLiteral(';');
  }

  if (!lexer.isEOF()) {
    throw new Error(
      `Unexpected ${tokenStr(lexer.nextToken())}, after closing '}' of the graph!`,
    );
  }
  return {
    strict: isStrict,
    directed: isDirected,
    name: graphName,
    graphAttributes,
    nodeAttributes,
    edgeAttributes,
    nodes,
    edges,
    subgraphs,
  };

  function parseStatement(): void {
    const token = lexer.nextToken();
    // stmt: node_stmt |	edge_stmt |	attr_stmt |	ID '=' ID |	subgraph
    switch (token.kind) {
      case 'ID': {
        if (lexer.peekIsLiteral('=')) {
          // ID '=' ID
          parseAttr(token, graphAttributes);
          break;
        }

        // node_id:	ID [ port ]
        // FIXME: handle string escape characters
        const nodeID = token.value;
        // port: ':' ID [ ':' compass_pt ]
        // FIXME

        if (optionalEdgeOp()) {
          const edge: Required<Edge> = {
            head: lexer.expectID('node name').value,
            tail: nodeID,
            attributes: {},
          };
          if (lexer.peekIsLiteral('[')) {
            parseAttrList(edge.attributes);
          }
          edges.push(edge);
          // FIXME
        } else {
          // node_stmt: node_id [ attr_list ]
          const node: Required<Node> = { name: nodeID, attributes: {} };
          if (lexer.peekIsLiteral('[')) {
            parseAttrList(node.attributes);
          }
          nodes.push(node);
          // FIXME
        }

        break;
      }
      // case 'subgraph':
      //   break;

      // attr_stmt:	(graph | node | edge) attr_list
      case 'graph':
        parseAttrList(graphAttributes);
        break;
      case 'node':
        parseAttrList(nodeAttributes);
        break;
      case 'edge':
        parseAttrList(edgeAttributes);
        break;
      case 'EOF':
        throw new Error(
          `Unexpected end of file, expected ${kindStr('}')} before the end of the graph!`,
        );
    }
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

  function parseAttrList(attributes: Attributes) {
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
    attributes[name.value] = value.value;
  }
}
