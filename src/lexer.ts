import type { Graph } from './graph.js';

type Token =
  | ','
  | ';'
  | '='
  | '['
  | ']'
  | '{'
  | '}'
  | 'node'
  | 'edge'
  | 'graph'
  | 'digraph'
  | 'subgraph'
  | 'strict'
  | 'EOF'
  | { id: string; kind: IDKind };

const IDKind = {
  Number: 0,
  Name: 1,
  String: 2,
  HTML: 3,
} as const;
type IDKind = (typeof IDKind)[keyof typeof IDKind];

const Char = {
  BOM: 0xfe_ff,
  '\t': 0x09,
  '\n': 0x0a,
  '\r': 0x0d,
  ' ': 0x20,
  ',': 0x2c,
  _0: 0x30,
  _9: 0x39,
  ';': 0x3b,
  '=': 0x3d,
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
  #position = -1;
  #currentChar: number | undefined = undefined;

  constructor(dotStr: string) {
    this.#dotStr = dotStr;
    this.#readNextChar();
  }

  nextToken(): Token {
    while (this.#currentChar != undefined) {
      switch (this.#currentChar) {
        // Ignored:
        case Char.BOM:
        case Char['\n']:
        case Char['\r']:
        case Char['\t']:
        case Char[' ']:
          break;

        case Char[',']:
        case Char[';']:
        case Char['=']:
        case Char['[']:
        case Char[']']:
        case Char['{']:
        case Char['}']: {
          const token = String.fromCodePoint(this.#currentChar) as Token;
          this.#readNextChar();
          return token;
        }

        // Comment
        // case 0x00_23: // #
        //   return readComment(lexer, position);

        default: {
          if (isNameStart(this.#currentChar)) {
            return this.#readNameToken();
          }
          const line = this.#line.toString();
          const column = (this.#position - this.#lineStart + 1).toString();
          throw new Error(
            `(${line}:${column})Unexpected character: '${String.fromCodePoint(this.#currentChar)}'`,
          );
        }
      }
      this.#readNextChar();
    }
    return 'EOF';
  }

  #readNameToken(): Token {
    const start = this.#position;
    do {
      this.#readNextChar();
    } while (
      this.#currentChar !== undefined &&
      isNameContinue(this.#currentChar)
    );

    const name = this.#dotStr.slice(start, this.#position);
    const token = name.toLowerCase();
    switch (token) {
      case 'node':
      case 'edge':
      case 'graph':
      case 'digraph':
      case 'subgraph':
      case 'strict':
        return token;
      default:
        return { id: name, kind: IDKind.Name };
    }
  }

  #readNextChar(): void {
    this.#currentChar = this.#dotStr.codePointAt(++this.#position);
    if (this.#currentChar === Char['\r']) {
      if (this.#dotStr.codePointAt(this.#position) !== Char['\n']) {
        // "Carriage Return (U+000D)" [lookahead != "New Line (U+000A)"]
        ++this.#line;
        this.#lineStart = this.#position;
      }
    } else if (this.#currentChar === Char['\n']) {
      // "New Line (U+000A)"
      ++this.#line;
      this.#lineStart = this.#position;
    }
  }
}

function tokenStr(token: Token): string {
  if (typeof token === 'object') {
    switch (token.kind) {
      case IDKind.Number:
        return `number '${token.id}'`;
      case IDKind.Name:
        return `identifier '${token.id}'`;
      case IDKind.String:
        return `string "${ellipsize(token.id)}"`;
      case IDKind.HTML:
        return `html "${ellipsize(token.id)}"`;
      default:
        return `unknown token '${token.id}'`;
    }
  }
  switch (token) {
    case ',':
    case ';':
    case '=':
    case '[':
    case ']':
    case '{':
    case '}':
      return `'${token}'`;
    case 'node':
    case 'edge':
    case 'graph':
    case 'digraph':
    case 'subgraph':
    case 'strict':
      return `keyword '${token}'`;
    case 'EOF':
      return 'end of file';
  }
}

function ellipsize(str: string): string {
  return str.length > 20 ? str.slice(0, 17) + '...' : str;
}

function isDigit(ch: number): boolean {
  return ch >= Char._0 && ch <= Char._9;
}

function isLetter(ch: number): boolean {
  return (ch >= Char.A && ch <= Char.Z) || (ch >= Char.a && ch <= Char.z);
}

function isNameStart(ch: number): boolean {
  return isLetter(ch) || ch === Char._ || ch >= Char.NON_ASCII_START;
}

function isNameContinue(code: number): boolean {
  return isNameStart(code) || isDigit(code);
}

class Parser {
  #graph: Graph;
  #lexer: Lexer;
  #token: Token;

  constructor(dotStr: string) {
    this.#lexer = new Lexer(dotStr);
    this.#token = this.#lexer.nextToken();
    this.#graph = {};
  }

  static parseDot(this: void, dotStr: string): Graph {
    const parser = new Parser(dotStr);
    return parser.#parseGraph();
  }

  #parseGraph(): Graph {
    if (this.#isEOF()) {
      throw new Error('Missing graph definition!');
    }

    const graph = this.#graph;
    // graph:	[ strict ] (graph | digraph) [ ID ] '{' stmt_list '}'
    graph.strict = this.#optionalToken('strict');
    graph.directed = this.#parseIsDirectedGraph();
    graph.name = this.#optionalID();
    this.#expectedToken('{');
    // stmt_list	:	[ stmt [ ';' ] stmt_list ]
    while (this.#token != '}') {
      // console.log(this.#token);
      switch (this.#token) {
        case 'EOF':
          throw new Error(
            `Unexpected ${tokenStr('EOF')}, expected ${tokenStr('}')} before the end of the graph!`,
          );
        // stmt:	node_stmt |	edge_stmt |	attr_stmt |	ID '=' ID |	subgraph
        // attr_stmt:	(graph | node | edge) attr_list
        //
        // case 'graph':
        //   this.parseGraphAttributes(graph.graphAttributes);
        // case 'node':
        // case 'edge':
        // default:
      }
      this.#optionalToken(';');
    }

    this.#token = this.#lexer.nextToken();
    if (!this.#isEOF()) {
      throw new Error(
        `Unexpected ${tokenStr(this.#token)}, after closing ${tokenStr('}')} of the graph!`,
      );
    }
    return graph; // successfully parse the graph
  }

  #isEOF(): boolean {
    return this.#token === 'EOF';
  }

  #parseIsDirectedGraph(): boolean {
    switch (this.#token) {
      case 'graph':
        this.#token = this.#lexer.nextToken();
        return false;
      case 'digraph':
        this.#token = this.#lexer.nextToken();
        return true;
      default:
        throw new Error(
          `Unexpected ${tokenStr(this.#token)}, expected ${tokenStr('graph')} or ${tokenStr('digraph')}!`,
        );
    }
  }

  #expectedToken(expected: Token): void {
    if (this.#token !== expected) {
      throw new Error(
        `Unexpected ${tokenStr(this.#token)}, expected ${tokenStr(expected)}!`,
      );
    }
    this.#token = this.#lexer.nextToken();
  }

  #optionalID(): string | undefined {
    if (typeof this.#token === 'object') {
      const { id } = this.#token;
      this.#token = this.#lexer.nextToken();
      return id;
    }
  }

  #optionalToken(token: Token): boolean {
    if (token === this.#token) {
      this.#token = this.#lexer.nextToken();
      return true;
    }
    return false;
  }
}

export const parseDot = Parser.parseDot;
