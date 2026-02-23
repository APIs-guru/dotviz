import type { Attributes, Edge, Graph, Node, Subgraph } from './graph.js';

const BOM = '\uFEFF' as const;
type LiteralToken =
  | ','
  | ';'
  | '='
  | '['
  | ']'
  | '{'
  | '}'
  | '--'
  | '->'
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

type Token = { kind: LiteralToken } | ID | { kind: 'EOF' };

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
    if (this.isEOF()) {
      return { kind: 'EOF' };
    }

    const token = this.#readNextToken();
    this.#skipUntilTokenStart();
    return token;
  }

  isEOF(): boolean {
    return this.#nextIndex >= this.#dotStr.length;
  }

  peekIs(kind: LiteralToken): boolean {
    return this.#dotStr.startsWith(kind, this.#nextIndex);
  }

  expectID(description: string): ID {
    const token = this.nextToken();
    if (token.kind !== 'ID') {
      throw new Error(
        `Unexpected ${tokenStr(token)}, expected ${description}!`,
      );
    }
    return token;
  }

  expectedToken(kind: LiteralToken): void {
    if (!this.optionalToken(kind)) {
      throw new Error(
        `Unexpected ${tokenStr(this.nextToken())}, expected ${kindStr(kind)}!`,
      );
    }
  }

  optionalToken(kind: LiteralToken): boolean {
    if (this.#dotStr.startsWith(kind, this.#nextIndex)) {
      this.#nextIndex += kind.length;
      this.#skipUntilTokenStart();
      return true;
    }
    return false;
  }

  #readNextToken(): Token {
    const tokenStartChar = this.#peekNextChar();
    switch (tokenStartChar) {
      case undefined:
        return { kind: 'EOF' };

      case ',':
      case ';':
      case '=':
      case '[':
      case ']':
      case '{':
      case '}':
        this.#readNextChar();
        return { kind: tokenStartChar };

      case '-': {
        const nextChar = this.#peekNextChar(1);
        if (this.optionalToken('--')) {
          return { kind: '--' };
        } else if (this.optionalToken('->')) {
          return { kind: '->' };
        } else if (isNumberContinue(nextChar)) {
          return this.#readNumberToken();
        }
        break;
      }

      case '"':
        return this.#readStringToken();

      // Comment
      // case 0x00_23: // #
      //   skipComment(lexer, position);
      //   continue;

      default:
        if (isNameStart(tokenStartChar)) {
          return this.#readNameToken();
        }
        if (isNumberStart(tokenStartChar)) {
          return this.#readNumberToken();
        }
    }
    const line = this.#line.toString();
    const column = (this.#nextIndex - this.#lineStart + 1).toString();
    throw new Error(
      `(${line}:${column})Unexpected character: '${tokenStartChar}'`,
    );
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
      }
      return;
    }
  }

  #readNumberToken(): Token {
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

    // FIXME: check for over and underflow
    return {
      kind: 'ID',
      idType: IDType.Number,
      value: this.#dotStr.slice(valueStart, this.#nextIndex),
    };
  }

  #readDigits(): void {
    while (isDigit(this.#peekNextChar())) {
      this.#readNextChar();
    }
  }

  #readStringToken(): ID {
    const line = this.#line.toString();
    const column = (this.#nextIndex - this.#lineStart + 1).toString();

    this.#readNextChar(); // skip opening `"`
    const valueStart = this.#nextIndex;
    while (this.#peekNextChar() !== '"') {
      switch (this.#readNextChar()) {
        case undefined: {
          const value = this.#dotStr.slice(valueStart, this.#nextIndex);
          throw new Error(
            `(${line}:${column})Unterminated string, missing closing '"' in: '"${ellipsize(value)}'`,
          );
        }
      }
    }

    const value = this.#dotStr.slice(valueStart, this.#nextIndex);
    this.#readNextChar(); // skip closing `"`
    return { kind: 'ID', idType: IDType.String, value };
  }

  #readNameToken(): Token | ID {
    const valueStart = this.#nextIndex;
    while (isNameContinue(this.#peekNextChar())) {
      this.#readNextChar();
    }

    const value = this.#dotStr.slice(valueStart, this.#nextIndex);
    const token = value.toLowerCase();
    switch (token) {
      case 'node':
      case 'edge':
      case 'graph':
      case 'digraph':
      case 'subgraph':
      case 'strict':
        return { kind: token };
      default:
        return { kind: 'ID', idType: IDType.Name, value };
    }
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

function kindStr(kind: LiteralToken | 'EOF'): string {
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

function isNumberStart(ch: string): boolean {
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
  const isStrict = lexer.optionalToken('strict');
  const isDirected = parseIsDirectedGraph();
  let graphName = undefined;
  if (!lexer.optionalToken('{')) {
    graphName = lexer.expectID('graph name').value;
    lexer.expectedToken('{');
  }

  const graphAttributes: Attributes = {};
  const nodeAttributes: Attributes = {};
  const edgeAttributes: Attributes = {};
  const nodes: Required<Node>[] = [];
  const edges: Required<Edge>[] = [];
  const subgraphs: Required<Subgraph>[] = [];

  while (!lexer.optionalToken('}')) {
    // stmt_list	:	[ stmt [ ';' ] stmt_list ]
    parseStatement();
    lexer.optionalToken(';');
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
        if (lexer.peekIs('=')) {
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
          if (lexer.peekIs('[')) {
            parseAttrList(edge.attributes);
          }
          edges.push(edge);
          // FIXME
        } else {
          // node_stmt: node_id [ attr_list ]
          const node: Required<Node> = { name: nodeID, attributes: {} };
          if (lexer.peekIs('[')) {
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
    if (lexer.optionalToken('--')) {
      if (isDirected) {
        throw new Error(
          `Unexpected keyword '--' in directed graph, expected keyword '->'!`,
        );
      }
      return true;
    }
    if (lexer.optionalToken('->')) {
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
    const token = lexer.nextToken();
    switch (token.kind) {
      case 'graph':
        return false;
      case 'digraph':
        return true;
      default:
        throw new Error(
          `Unexpected ${tokenStr(token)}, expected keyword 'graph' or 'digraph'!`,
        );
    }
  }

  function parseAttrList(attributes: Attributes) {
    // attr_list:	'[' [ a_list ] ']' [ attr_list ]
    do {
      lexer.expectedToken('[');
      // a_list: ID '=' ID [ (';' | ',') ] [ a_list ]
      while (!lexer.optionalToken(']')) {
        parseAttr(lexer.expectID('attribute name'), attributes);
        // Either ';' or ',' but doesn't allow both
        if (!lexer.optionalToken(';')) {
          lexer.optionalToken(',');
        }
      }
    } while (lexer.peekIs('['));
  }

  function parseAttr(name: ID, attributes: Attributes) {
    lexer.expectedToken('=');
    const value = lexer.expectID('attribute value');
    // FIXME: handle string escape characters
    attributes[name.value] = value.value;
  }
}
