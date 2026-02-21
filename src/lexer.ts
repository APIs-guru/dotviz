import type { Attributes, Graph } from './graph.js';

type KeywordOrPunctuation =
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
  | 'strict'
  | 'EOF';

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

type Token = { kind: KeywordOrPunctuation } | ID;
type TokenKind = KeywordOrPunctuation | 'ID';

const Char = {
  BOM: 0xfe_ff,
  '\t': 0x09,
  '\n': 0x0a,
  '\r': 0x0d,
  ' ': 0x20,
  '"': 0x22,
  ',': 0x2c,
  '-': 0x2d,
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
          const kind = String.fromCodePoint(
            this.#currentChar,
          ) as KeywordOrPunctuation;
          this.#readNextChar();
          return { kind };
        }

        case Char['-']: {
          const line = this.#line.toString();
          const column = (this.#position - this.#lineStart + 1).toString();
          this.#readNextChar();
          switch (this.#currentChar as number) {
            case Char['-']:
              this.#readNextChar();
              return { kind: '--' };
            case Char['>']:
              this.#readNextChar();
              return { kind: '->' };
          }
          throw new Error(
            `(${line}:${column})Unexpected character: '${String.fromCodePoint(this.#currentChar)}'`,
          );
        }

        case Char['"']:
          return this.#readString();

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
    return { kind: 'EOF' };
  }

  #readString(): ID {
    const line = this.#line.toString();
    const column = (this.#position - this.#lineStart + 1).toString();

    this.#readNextChar(); // skip `"`
    const start = this.#position;
    do {
      this.#readNextChar();
      if (this.#currentChar === undefined) {
        const value = this.#dotStr.slice(start, this.#position);
        throw new Error(
          `(${line}:${column})Unterminated string: '${ellipsize(value)}'`,
        );
      }
    } while (this.#currentChar !== Char['"']);

    const value = this.#dotStr.slice(start, this.#position);
    this.#readNextChar(); // skip `"`
    return { kind: 'ID', idType: IDType.String, value };
  }

  #readNameToken(): Token | ID {
    const start = this.#position;
    do {
      this.#readNextChar();
    } while (
      this.#currentChar !== undefined &&
      isNameContinue(this.#currentChar)
    );

    const value = this.#dotStr.slice(start, this.#position);
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

function kindStr(kind: KeywordOrPunctuation): string {
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
    if (this.#peekIs('EOF')) {
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

    if (!this.#peekIs('EOF')) {
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
          const head = this.#expectID('node name').value;
          graph.edges.push({
            head,
            tail: nodeID,
          });
          // FIXME
        } else {
          // node_stmt: node_id [ attr_list ]
          // FIXME
        }

        break;
      }
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
      // case 'subgraph':
      //   break;
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

  #peekIs(kind: TokenKind): boolean {
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

  #expectedToken(kind: KeywordOrPunctuation): void {
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

  #optionalToken(kind: KeywordOrPunctuation): boolean {
    if (this.#peekIs(kind)) {
      this.#consume();
      return true;
    }
    return false;
  }
}

export const parseDot = Parser.parseDot;
