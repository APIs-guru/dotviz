import type { Attributes, Edge, Graph, Node } from './graph.js';

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

const Char = {
  BOM: 0xfe_ff,
  '\t': 0x09,
  '\n': 0x0a,
  '\r': 0x0d,
  ' ': 0x20,
  '"': 0x22,
  ',': 0x2c,
  '-': 0x2d,
  '.': 0x2e,
  _0: 0x30,
  _9: 0x39,
  ';': 0x3b,
  '=': 0x3d,
  '>': 0x3e,
  a: 0x41,
  z: 0x5a,
  _: 0x5f,
  A: 0x61,
  Z: 0x7a,
  '[': 0x5b,
  ']': 0x5d,
  '{': 0x7b,
  '}': 0x7d,
  NON_ASCII_START: 0x80,
} as const;

class Lexer {
  #dotStr: string;
  #line = 1;
  #lineStart = 0;
  #nextIndex = 0;

  constructor(dotStr: string) {
    this.#dotStr = dotStr;
  }

  nextToken(): Token {
    this.#skipUntilTokenStart();
    const tokenStartChar = this.#peekNextChar();
    switch (tokenStartChar) {
      case undefined:
        return { kind: 'EOF' };

      case Char[',']:
        this.#readNextChar();
        return { kind: ',' };
      case Char[';']:
        this.#readNextChar();
        return { kind: ';' };
      case Char['=']:
        this.#readNextChar();
        return { kind: '=' };
      case Char['[']:
        this.#readNextChar();
        return { kind: '[' };
      case Char[']']:
        this.#readNextChar();
        return { kind: ']' };
      case Char['{']:
        this.#readNextChar();
        return { kind: '{' };
      case Char['}']:
        this.#readNextChar();
        return { kind: '}' };

      case Char['-']: {
        const nextChar = this.#peekNextChar(1);
        switch (nextChar) {
          case Char['-']:
            this.#readNextChar();
            this.#readNextChar();
            return { kind: '--' };
          case Char['>']:
            this.#readNextChar();
            this.#readNextChar();
            return { kind: '->' };
          default:
            if (isNumberContinue(nextChar)) {
              return this.#readNumberToken();
            }
        }
        break;
      }

      case Char['"']:
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
      `(${line}:${column})Unexpected character: '${String.fromCodePoint(tokenStartChar)}'`,
    );
  }

  #skipUntilTokenStart() {
    while (true) {
      switch (this.#peekNextChar()) {
        // Ignored:
        case Char.BOM:
        case Char['\n']:
        case Char['\r']:
        case Char['\t']:
        case Char[' ']:
          this.#readNextChar();
          continue;
      }
      return;
    }
  }

  #readNumberToken(): Token {
    const valueStart = this.#nextIndex;
    this.#skipChar(Char['-']);
    if (this.#peekNextChar() !== Char['.']) {
      this.#readDigits();
    }
    if (this.#skipChar(Char['.'])) {
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
    while (this.#peekNextChar() !== Char['"']) {
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

  #skipChar(char: number): boolean {
    if (this.#peekNextChar() === char) {
      this.#readNextChar();
      return true;
    }
    return false;
  }

  #peekNextChar(offset = 0): number | undefined {
    return this.#dotStr.codePointAt(this.#nextIndex + offset);
  }

  #readNextChar(): number | undefined {
    const charIndex = this.#nextIndex++;
    const char = this.#dotStr.codePointAt(charIndex);
    if (char === Char['\r']) {
      if (this.#peekNextChar() !== Char['\n']) {
        // "Carriage Return (U+000D)" [lookahead != "New Line (U+000A)"]
        ++this.#line;
        this.#lineStart = charIndex;
      }
    } else if (char === Char['\n']) {
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

function isDigit(ch: number | undefined): boolean {
  return ch !== undefined && ch >= Char._0 && ch <= Char._9;
}

function isNumberStart(ch: number): boolean {
  return isNumberContinue(ch) || ch == Char['-'];
}

function isNumberContinue(ch: number | undefined): boolean {
  return isDigit(ch) || ch === Char['.'];
}

function isLetter(ch: number): boolean {
  return (ch >= Char.A && ch <= Char.Z) || (ch >= Char.a && ch <= Char.z);
}

function isNameStart(ch: number | undefined): boolean {
  if (ch === undefined) return false;
  return isLetter(ch) || ch === Char._ || ch >= Char.NON_ASCII_START;
}

function isNameContinue(code: number | undefined): boolean {
  return isNameStart(code) || isDigit(code);
}

class Parser {
  #lexer: Lexer;
  #token: Token | ID;

  constructor(dotStr: string) {
    this.#lexer = new Lexer(dotStr);
    this.#token = this.#lexer.nextToken();
  }

  static parseDot(this: void, dotStr: string): Graph {
    const parser = new Parser(dotStr);
    return parser.#parseGraph();
  }

  #parseGraph(): Graph {
    if (this.#isEOF()) {
      throw new Error('Missing graph definition!');
    }

    // graph:	[ strict ] (graph | digraph) [ ID ] '{' stmt_list '}'
    const graph: Required<Graph> = {
      strict: this.#optionalToken('strict'),
      directed: this.#parseIsDirectedGraph(),
      name: this.#optionalID(),
      graphAttributes: {},
      nodeAttributes: {},
      edgeAttributes: {},
      nodes: [],
      edges: [],
      subgraphs: [],
    } as const;

    this.#expectedToken('{');
    // stmt_list	:	[ stmt [ ';' ] stmt_list ]
    while (!this.#peekIs('}')) {
      this.#parseStatement(graph);
      this.#optionalToken(';');
    }
    this.#consume();

    if (!this.#isEOF()) {
      throw new Error(
        `Unexpected ${tokenStr(this.#token)}, after closing '}' of the graph!`,
      );
    }
    return graph;
  }

  #parseStatement(graph: Required<Graph>): void {
    const token = this.#consume();
    // stmt: node_stmt |	edge_stmt |	attr_stmt |	ID '=' ID |	subgraph
    switch (token.kind) {
      case 'ID': {
        if (this.#peekIs('=')) {
          // ID '=' ID
          this.#parseAttr(token, graph.graphAttributes);
          break;
        }

        // node_id:	ID [ port ]
        // FIXME: handle string escape characters
        const nodeID = token.value;
        // port: ':' ID [ ':' compass_pt ]
        // FIXME

        if (this.#optionalEdgeOp(graph.directed)) {
          const edge: Required<Edge> = {
            head: this.#expectID('node name').value,
            tail: nodeID,
            attributes: {},
          };
          if (this.#peekIs('[')) {
            this.#parseAttrList(edge.attributes);
          }
          graph.edges.push(edge);
          // FIXME
        } else {
          // node_stmt: node_id [ attr_list ]
          const node: Required<Node> = { name: nodeID, attributes: {} };
          if (this.#peekIs('[')) {
            this.#parseAttrList(node.attributes);
          }
          graph.nodes.push(node);
          // FIXME
        }

        break;
      }
      // case 'subgraph':
      //   break;

      // attr_stmt:	(graph | node | edge) attr_list
      case 'graph':
        this.#parseAttrList(graph.graphAttributes);
        break;
      case 'node':
        this.#parseAttrList(graph.nodeAttributes);
        break;
      case 'edge':
        this.#parseAttrList(graph.edgeAttributes);
        break;
      case 'EOF':
        throw new Error(
          `Unexpected end of file, expected ${kindStr('}')} before the end of the graph!`,
        );
    }
  }

  #optionalEdgeOp(directed: boolean): boolean {
    if (this.#optionalToken('--')) {
      if (directed) {
        throw new Error(
          `Unexpected keyword '--' in directed graph, expected keyword '->'!`,
        );
      }
      return true;
    }
    if (this.#optionalToken('->')) {
      if (!directed) {
        throw new Error(
          `Unexpected keyword '->' in undirected graph, expected keyword '--'!`,
        );
      }
      return true;
    }
    return false;
  }

  #parseIsDirectedGraph(): boolean {
    const token = this.#consume();
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

  #parseAttrList(attributes: Attributes) {
    // attr_list:	'[' [ a_list ] ']' [ attr_list ]
    do {
      this.#expectedToken('[');
      // a_list: ID '=' ID [ (';' | ',') ] [ a_list ]
      while (!this.#peekIs(']')) {
        this.#parseAttr(this.#expectID('attribute name'), attributes);
        // Either ';' or ',' but doesn't allow both
        if (!this.#optionalToken(';')) {
          this.#optionalToken(',');
        }
      }
      this.#expectedToken(']');
    } while (this.#peekIs('['));
  }

  #parseAttr(name: ID, attributes: Attributes) {
    this.#expectedToken('=');
    const value = this.#expectID('attribute value');
    // FIXME: handle string escape characters
    attributes[name.value] = value.value;
  }

  #isEOF(): boolean {
    return this.#token.kind === 'EOF';
  }

  #peekIs(kind: LiteralToken): boolean {
    return this.#token.kind === kind;
  }

  #consume(): Token | ID {
    const token = this.#token;
    this.#token = this.#lexer.nextToken();
    return token;
  }

  #expectID(description: string): ID {
    const token = this.#consume();
    if (token.kind !== 'ID') {
      throw new Error(
        `Unexpected ${tokenStr(this.#token)}, expected ${description}!`,
      );
    }
    return token;
  }

  #expectedToken(kind: LiteralToken): void {
    const token = this.#consume();
    if (token.kind !== kind) {
      throw new Error(
        `Unexpected ${tokenStr(token)}, expected ${kindStr(kind)}!`,
      );
    }
  }

  #optionalID(): string | undefined {
    const token = this.#token;
    if (token.kind === 'ID') {
      this.#consume();
      // FIXME: handle string escape characters
      return token.value;
    }
  }

  #optionalToken(kind: LiteralToken): boolean {
    if (this.#peekIs(kind)) {
      this.#consume();
      return true;
    }
    return false;
  }
}

export const parseDot = Parser.parseDot;
