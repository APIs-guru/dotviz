import type { Attributes } from './graph.js';
import {
  extractKeyFromEdgeAttributes,
  type FixedAttributes,
  NormalizedGraph,
  NormalizedNode,
  NormalizedSubgraph,
} from './normalize-graph.ts';
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
  errors: RenderError[] = [];

  static AbortError = new Error(
    'Internal error, thrown when parsing of dot file fails',
  );

  constructor(dotStr: string) {
    this.#dotStr = dotStr;
    this.#skipUntilTokenStart();
  }

  failWithError(message: string): never {
    this.errors.push({
      level: 'error',
      message,
    });
    throw Lexer.AbortError;
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

  expectID(description: string): ID {
    const token = this.#readKeywordOrID();
    if (token === undefined) {
      this.failWithError(
        `Unexpected ${tokenStr(this.#readNextToken())}, expected ${description}.`,
      );
    }
    if (token.kind === 'ID') {
      this.#skipUntilTokenStart();
      return token;
    }
    this.failWithError(
      `Unexpected reserved keyword ${tokenStr(token)} where ${description} was expected. If you want to use it as an identifier, enclose it in quotes: "${token.kind}".`,
    );
  }

  expectLiteral(literal: LiteralToken): void {
    if (!this.optionalLiteral(literal)) {
      this.failWithError(
        `Unexpected ${tokenStr(this.#readNextToken())}, expected ${kindStr(literal)}.`,
      );
    }
  }

  optionalLiteral(literal: LiteralToken): boolean {
    if (this.peekIsLiteral(literal)) {
      this.#nextIndex += literal.length;
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
    const token = this.#readKeywordOrID();
    if (token !== undefined) {
      return token;
    }

    const tokenStartChar = this.#peekNextChar();
    switch (tokenStartChar) {
      case undefined:
        return { kind: 'EOF' };
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
      case '-':
        if (this.optionalLiteral('--')) {
          return { kind: '--' };
        }
        if (this.optionalLiteral('->')) {
          return { kind: '->' };
        }
    }

    const line = this.#line.toString();
    const column = (this.#nextIndex - this.#lineStart + 1).toString();
    this.failWithError(
      `(${line}:${column}) Unexpected character: '${tokenStartChar}'. If this is meant to be part of a label or name, enclose it in quotes ("...").`,
    );
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
      if (value !== undefined) {
        const token = { kind: 'ID' as const, idType: IDType.Number, value };
        this.#warnIfAmbiguous(tokenStartIndex, token);
        return token;
      }
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
        errorStringValue(this.#dotStr.slice(tokenStartIndex, this.#nextIndex)) +
        nextChar;
      this.errors.push({
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

  #readNumber(): string | undefined {
    const valueStart = this.#nextIndex;
    // [-]?.[0-9]⁺ or [-]?[0-9]⁺(.[0-9]*)?
    this.#skipChar('-');
    const sawPeriod = this.#skipChar('.');
    if (!isDigit(this.#peekNextChar())) {
      this.#nextIndex = valueStart;
      return undefined;
    }
    while (isDigit(this.#peekNextChar())) {
      this.#readNextChar();
    }
    if (!sawPeriod) {
      this.#skipChar('.');
      while (isDigit(this.#peekNextChar())) {
        this.#readNextChar();
      }
    }

    return this.#dotStr.slice(valueStart, this.#nextIndex);
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
          this.failWithError(
            `(${line}:${column}) Unterminated HTML string. Add a closing '>' to complete the HTML started here: '<${errorStringValue(value)}'.`,
          );
          break;
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
    while (true) {
      switch (this.#readNextChar()) {
        case undefined: {
          const value = this.#dotStr.slice(valueStart, this.#nextIndex);
          this.failWithError(
            `(${line}:${column}) Unterminated string. Add a closing '"' to complete the string started here: '"${errorStringValue(value)}'.`,
          );
          break;
        }
        case '\\':
          this.#readNextChar();
          break;

        case '"':
          return this.#dotStr
            .slice(valueStart, this.#nextIndex - 1)
            .replaceAll(String.raw`\"`, '"')
            .replaceAll('\\\r\n', '')
            .replaceAll('\\\r', '')
            .replaceAll('\\\n', '');
      }
    }
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
    if (char === '\n') {
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
      return `string "${errorStringValue(value)}"`;
    case IDType.HTML:
      return `html <${errorStringValue(value)}>`;
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

function errorStringValue(value: string) {
  const truncated = value.length > 20 ? value.slice(0, 17) + '...' : value;
  return JSON.stringify(truncated).slice(1, -1);
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
  output: NormalizedGraph;
  errors: RenderError[];
}

export type ParseResult = ParseSuccessResult | FailureResult;
export function parseDot(
  dotStr: string,
  fixedAttributes: FixedAttributes,
): ParseResult {
  const lexer = new Lexer(dotStr);
  try {
    if (lexer.isEOF()) {
      lexer.failWithError(
        "Missing graph definition. Start your file with 'graph {}' or 'digraph {}'.",
      );
    }

    const graph = parseGraph(lexer, fixedAttributes);
    return {
      status: 'success',
      output: graph,
      errors: lexer.errors,
    };
  } catch (error: unknown) {
    /* c8 ignore start -- Should only happen in case of internal errors */
    if (error !== Lexer.AbortError) {
      throw error;
    }
    /* c8 ignore end */
    return {
      status: 'failure',
      output: null,
      errors: lexer.errors,
    };
  }
}

type NodeID = [NormalizedNode, string | undefined];

function parseGraph(
  lexer: Lexer,
  fixedAttributes: FixedAttributes,
): NormalizedGraph {
  // graph:	[ strict ] (graph | digraph) [ ID ] '{' stmt_list '}'
  const graph = new NormalizedGraph(
    {
      strict: lexer.optionalKeyword('strict'),
      directed: parseIsDirectedGraph(),
      name: lexer.peekIsLiteral('{')
        ? null
        : lexer.expectID('graph name').value,
    },
    fixedAttributes,
  );
  // FIXME: check if it's viz.js hack or it also present in graphviz
  graph.mergeNodeAttributes({ label: String.raw`\N` });

  lexer.expectLiteral('{');
  parseStatementList(graph);
  return graph;

  function parseStatementList(
    owner: NormalizedGraph | NormalizedSubgraph,
  ): void {
    // '{' stmt_list '}'
    while (!lexer.optionalLiteral('}')) {
      if (lexer.isEOF()) {
        lexer.failWithError(
          `Unexpected end of file. Add a closing '}' to match the opening '{' of the graph or subgraph.`,
        );
      }

      // stmt_list	:	[ stmt [ ';' ] stmt_list ]
      parseStatement(owner);
      lexer.optionalLiteral(';');
    }
  }

  function parseStatement(owner: NormalizedGraph | NormalizedSubgraph): void {
    const token = lexer.nextToken();
    // stmt: node_stmt |	edge_stmt |	attr_stmt |	ID '=' ID |	subgraph
    switch (token.kind) {
      case 'ID': {
        if (lexer.peekIsLiteral('=')) {
          // ID '=' ID
          const attributes: Attributes = {};
          parseAttr(token, attributes);
          owner.mergeGraphAttributes(attributes);
          break;
        }

        const [node] = owner.upsertNode(token.value);
        const nodeIDs: NodeID[] = [[node, optionalNodePort()]];
        while (lexer.optionalLiteral(',')) {
          nodeIDs.push(parseNodeID(owner));
        }

        if (lexer.peekIsLiteral('[')) {
          // node_stmt: node_id [ attr_list ]
          // FIXME: check that it's safe to ignore port
          const attributes: Attributes = parseAttrList();
          for (const [node] of nodeIDs) {
            node.mergeAttributes(attributes);
          }
        } else if (optionalEdgeOp()) {
          parseEdges(nodeIDs, owner);
        }
        break;
      }

      case '{': {
        const tailNodes = parseSubgraph(owner, null);
        if (optionalEdgeOp()) {
          parseEdges(tailNodes, owner);
        }
        break;
      }
      case 'subgraph': {
        const tailNodes = parseNamedSubgraph(owner);
        if (optionalEdgeOp()) {
          parseEdges(tailNodes, owner);
        }
        break;
      }

      // attr_stmt:	(graph | node | edge) attr_list
      case 'graph':
        owner.mergeGraphAttributes(parseAttrList());
        break;
      case 'node':
        owner.mergeNodeAttributes(parseAttrList());
        break;
      case 'edge':
        owner.mergeEdgeAttributes(parseAttrList());
        break;

      default:
        lexer.failWithError(
          `Unexpected ${tokenStr(token)}, expected node, edge, subgraph or attribute statement.`,
        );
    }
  }

  function parseNamedSubgraph(
    owner: NormalizedGraph | NormalizedSubgraph,
  ): NodeID[] {
    if (lexer.optionalLiteral('{')) {
      return parseSubgraph(owner, null);
    }
    const name = lexer.expectID('subgraph name').value;
    lexer.expectLiteral('{');
    return parseSubgraph(owner, name);
  }

  function parseSubgraph(
    owner: NormalizedGraph | NormalizedSubgraph,
    name: string | null,
  ): NodeID[] {
    const subgraph = owner.upsertSubgraph(name);
    parseStatementList(subgraph);
    return subgraph.sortedNodes().map((node) => [node, undefined]);
  }

  function parseEdges(
    tailNodes: NodeID[],
    owner: NormalizedGraph | NormalizedSubgraph,
  ) {
    const newEdges: [NodeID, NodeID][] = [];
    do {
      let headNodes: NodeID[];
      if (lexer.optionalLiteral('{')) {
        headNodes = parseSubgraph(owner, null);
      } else if (lexer.optionalKeyword('subgraph')) {
        headNodes = parseNamedSubgraph(owner);
      } else {
        headNodes = [];
        do {
          headNodes.push(parseNodeID(owner));
        } while (lexer.optionalLiteral(','));
      }

      for (const tail of tailNodes) {
        for (const head of headNodes) {
          newEdges.push([tail, head]);
        }
      }
      tailNodes = headNodes;
    } while (optionalEdgeOp());

    let key: string | null = null;
    let attributes: Attributes | undefined;
    if (lexer.peekIsLiteral('[')) {
      [key, attributes] = extractKeyFromEdgeAttributes(parseAttrList());
    }
    for (const [tail, head] of newEdges) {
      const [edge] = owner.upsertEdge({ tail: tail[0], head: head[0], key });
      edge.mergeAttributes({
        headport: head[1],
        tailport: tail[1],
        ...attributes,
      });
    }
  }

  function parseNodeID(owner: NormalizedGraph | NormalizedSubgraph): NodeID {
    // node_id:	ID [ port ]
    const name = lexer.expectID('node name').value;
    const [node] = owner.upsertNode(name);
    return [node, optionalNodePort()];
  }

  function optionalNodePort(): string | undefined {
    // port: ':' ID [ ':' compass_pt ]
    let port: string | undefined;
    if (lexer.optionalLiteral(':')) {
      port = lexer.expectID('port name').value;

      // compass_pt: n | ne | e | se | s | sw | w | nw | c | _
      if (lexer.optionalLiteral(':')) {
        port += ':' + lexer.expectID('compass point value').value;
      }
    }
    return port;
  }

  function optionalEdgeOp(): boolean {
    if (lexer.optionalLiteral('--')) {
      if (graph.directed) {
        lexer.failWithError(
          `Unexpected '--' in a directed graph. Use '->' for directed edges in a 'digraph'.`,
        );
      }
      return true;
    }
    if (lexer.optionalLiteral('->')) {
      if (!graph.directed) {
        lexer.failWithError(
          `Unexpected '->' in an undirected graph. Use '--' for undirected edges in a 'graph'.`,
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
    lexer.failWithError(
      `Unexpected ${tokenStr(lexer.nextToken())}, expected keyword 'graph' or 'digraph' at the beginning of the file.`,
    );
  }

  function parseAttrList(): Attributes {
    const attributes: Attributes = {};

    // attr_list:	'[' [ a_list ] ']' [ attr_list ]
    do {
      lexer.expectLiteral('[');
      // a_list: ID '=' ID [ (';' | ',') ] [ a_list ]
      while (!lexer.optionalLiteral(']')) {
        parseAttr(lexer.expectID('attribute name'), attributes);
        // Either ';' or ',' but doesn't allow both
        if (!lexer.optionalLiteral(';')) {
          lexer.optionalLiteral(',');
        }
      }
    } while (lexer.peekIsLiteral('['));

    return attributes;
  }

  function parseAttr(name: ID, attributes: Attributes) {
    lexer.expectLiteral('=');
    const value = lexer.expectID('attribute value');
    // FIXME: handle string escape characters
    attributes[name.value] =
      value.idType === IDType.HTML ? { html: value.value } : value.value;
  }
}
