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

    return {
      status: 'success',
      output: parseGraph(lexer),
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

function parseGraph(lexer: Lexer): Graph {
  // graph:	[ strict ] (graph | digraph) [ ID ] '{' stmt_list '}'
  const isStrict = lexer.optionalKeyword('strict');
  const isDirected = parseIsDirectedGraph();
  const graphName = lexer.optionalID()?.value;
  lexer.expectedLiteral('{');

  const graphAttributes: Attributes = {};
  const nodeAttributes: Attributes = {
    // FIXME: check if it's viz.js hack or it also present in graphviz
    label: String.raw`\N`,
  };
  const edgeAttributes: Attributes = {};
  const nodes: Required<Node>[] = [];
  const edges: Required<Edge>[] = [];
  const subgraphs: Required<Subgraph>[] = [];

  while (!lexer.optionalLiteral('}')) {
    // stmt_list	:	[ stmt [ ';' ] stmt_list ]
    parseStatement();
    lexer.optionalLiteral(';');
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
          return;
        }

        const nodeID = token.value;
        const nodePort = optionalNodePort();

        if (optionalEdgeOp()) {
          let tail = nodeID;
          let tailport = nodePort;
          const newEdges: Required<Edge>[] = [];
          do {
            const [head, headport] = parseNodeID();
            newEdges.push({
              head,
              tail,
              attributes: { headport, tailport },
            });
            tail = head;
            tailport = headport;
          } while (optionalEdgeOp());

          if (lexer.peekIsLiteral('[')) {
            const attributes: Attributes = { ...edgeAttributes };
            parseAttrList(attributes);
            for (const edge of newEdges) {
              edge.attributes = { ...edge.attributes, ...attributes };
            }
          }
          edges.push(...newEdges);
        } else {
          // node_stmt: node_id [ attr_list ]
          const node: Required<Node> = {
            name: nodeID,
            attributes: { ...nodeAttributes },
          };
          if (lexer.peekIsLiteral('[')) {
            parseAttrList(node.attributes);
          }
          nodes.push(node);
          // FIXME
        }

        return;
      }
      // case 'subgraph':
      //   break;

      // attr_stmt:	(graph | node | edge) attr_list
      case 'graph':
        parseAttrList(graphAttributes);
        return;
      case 'node':
        parseAttrList(nodeAttributes);
        return;
      case 'edge':
        parseAttrList(edgeAttributes);
        return;
      case 'EOF':
        throw new Error(
          `Unexpected end of file, expected ${kindStr('}')} before the end of the graph!`,
        );
    }
    throw new Error(
      `Unexpected ${tokenStr(token)}, expected node, edge, subgraph or attribute statement!`,
    );
  }

  function parseNodeID(): [string, string | undefined] {
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
    attributes[name.value] =
      value.idType === IDType.HTML ? { html: value.value } : value.value;
  }
}
