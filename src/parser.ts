import type { Attributes } from './graph.d.ts';
import { type Location, printLocation } from './location.ts';
import {
  type NormalizedEdgeEndpoint,
  NormalizedGraph,
  NormalizedSubgraph,
  type OverrideAttributes,
} from './normalize-graph.ts';
import type { RenderError } from './viz.ts';

// To make parser internally consistent, all characters are read as UTF-16:
/* eslint-disable unicorn/prefer-code-point */

const Char = {
  '\t': 0x09,
  '\n': 0x0a,
  '\r': 0x0d,
  ' ': 0x20,
  '"': 0x22,
  '#': 0x23,
  '*': 0x2a,
  '+': 0x2b,
  ',': 0x2c,
  '-': 0x2d,
  '.': 0x2e,
  '/': 0x2f,
  '0': 0x30,
  '9': 0x39,
  ':': 0x3a,
  ';': 0x3b,
  '<': 0x3c,
  '=': 0x3d,
  '>': 0x3e,
  A: 0x41,
  Z: 0x5a,
  '[': 0x5b,
  '\\': 0x5c,
  ']': 0x5d,
  _: 0x5f,
  a: 0x61,
  z: 0x7a,
  '{': 0x7b,
  '}': 0x7d,
  BOM: 0xfe_ff,
} as const;

const LITERAL = 0x01_00 as const;
const LiteralKind = {
  '--': 0x01_2d, // Char['-'] | LITERAL
  '->': 0x01_3e, // Char['>'] | LITERAL
  '+': 0x01_2b, //  Char['+'] | LITERAL
  ',': 0x01_2c, //  Char[','] | LITERAL
  ':': 0x01_3a, //  Char[':'] | LITERAL
  ';': 0x01_3b, //  Char[';'] | LITERAL
  '=': 0x01_3d, //  Char['='] | LITERAL
  '[': 0x01_5b, //  Char['['] | LITERAL
  ']': 0x01_5d, //  Char[']'] | LITERAL
  '{': 0x01_7b, //  Char['{'] | LITERAL
  '}': 0x01_7d, //  Char['}'] | LITERAL
} as const;
type LiteralKind = (typeof LiteralKind)[keyof typeof LiteralKind];

interface LiteralToken {
  readonly kind: LiteralKind;
  readonly start: Location;
  readonly length: number;
  readonly value: undefined;
}

const KEYWORD = 0x02_00 as const;
const KeywordKind = {
  node: 0x02_00,
  edge: 0x02_01,
  graph: 0x02_02,
  digraph: 0x02_03,
  subgraph: 0x02_04,
  strict: 0x02_05,
} as const;
type KeywordKind = (typeof KeywordKind)[keyof typeof KeywordKind];

interface KeywordToken {
  readonly kind: KeywordKind;
  readonly start: Location;
  readonly length: number;
  readonly value: undefined;
}

const keywordStrings = [
  'node',
  'edge',
  'graph',
  'digraph',
  'subgraph',
  'strict',
];

const ID = 0x04_00;
const IDKind = {
  Name: 0x04_00,
  Number: 0x04_01,
  String: 0x04_02,
  HTML: 0x04_03,
} as const;
type IDKind = (typeof IDKind)[keyof typeof IDKind];

interface IDToken {
  readonly kind: IDKind;
  readonly start: Location;
  readonly length: number;
  readonly value: string;
}

const Kind = {
  EOF: 0,
  UnexpectedChar: 1,
  UnterminatedString: 2,
  UnterminatedHTML: 3,
  UnterminatedBlockComment: 4,
  ...LiteralKind,
  ...KeywordKind,
  ...IDKind,
} as const;
type Kind = (typeof Kind)[keyof typeof Kind];

type Token =
  | LiteralToken
  | KeywordToken
  | IDToken
  | {
      readonly kind:
        | typeof Kind.EOF
        | typeof Kind.UnexpectedChar
        | typeof Kind.UnterminatedString
        | typeof Kind.UnterminatedHTML
        | typeof Kind.UnterminatedBlockComment;
      readonly start: Location;
      readonly length: number;
      readonly value: undefined;
    };

class Lexer {
  readonly #dotStr: string;
  #line = 1;
  #lineStart = 0;
  #nextIndex = 0;

  constructor(dotStr: string) {
    this.#dotStr = dotStr;
  }

  #countNewLine(): void {
    ++this.#line;
    this.#lineStart = this.#nextIndex;
  }

  #skipChar(char: number): boolean {
    if (this.#peekChar() === char) {
      ++this.#nextIndex;
      return true;
    }
    return false;
  }

  #readChar(): number {
    return this.#dotStr.charCodeAt(this.#nextIndex++);
  }

  #peekChar(): number {
    return this.#dotStr.charCodeAt(this.#nextIndex);
  }

  #peekAheadChar(): number {
    return this.#dotStr.charCodeAt(this.#nextIndex + 1);
  }

  #readUntilNewLine(): void {
    while (this.#nextIndex < this.#dotStr.length) {
      if (this.#readChar() === Char['\n']) {
        this.#countNewLine();
        return;
      }
    }
  }

  #nextIndexLocation(): Location {
    return {
      index: this.#nextIndex,
      line: this.#line,
      column: this.#nextIndex - this.#lineStart + 1,
    };
  }

  #readBlockComment(): Token | undefined {
    const start = this.#nextIndexLocation();
    this.#nextIndex += 2; // read `/` and `*`
    while (this.#nextIndex < this.#dotStr.length) {
      switch (this.#readChar()) {
        case Char['\n']:
          this.#countNewLine();
          break;
        case Char['*']:
          if (this.#skipChar(Char['/'])) {
            return undefined;
          }
          break;
      }
    }

    const length = this.#nextIndex - start.index;
    return {
      kind: Kind.UnterminatedBlockComment,
      start,
      length,
      value: undefined,
    };
  }

  nextToken(): Token {
    while (this.#nextIndex < this.#dotStr.length) {
      const char = this.#peekChar();
      switch (char) {
        // Ignored:
        case Char.BOM:
        case Char['\r']:
        case Char['\t']:
        case Char[' ']:
          ++this.#nextIndex;
          continue;
        case Char['\n']:
          ++this.#nextIndex;
          this.#countNewLine();
          continue;
        case Char['#']:
          this.#readUntilNewLine();
          continue;
        case Char['/']:
          switch (this.#peekAheadChar()) {
            case Char['/']:
              this.#readUntilNewLine();
              continue;
            case Char['*']: {
              const invalidToken = this.#readBlockComment();
              if (invalidToken) {
                return invalidToken;
              }
              continue;
            }
          }
          break;
        case Char['+']:
        case Char[',']:
        case Char[':']:
        case Char[';']:
        case Char['=']:
        case Char['[']:
        case Char[']']:
        case Char['{']:
        case Char['}']: {
          const start = this.#nextIndexLocation();
          const kind = (this.#readChar() | LITERAL) as LiteralKind;
          return { kind, start, length: 1, value: undefined };
        }
        case Char['<']:
          return this.#readHTML();
        case Char['"']:
          return this.#readString();
        case Char['-']:
          switch (this.#peekAheadChar()) {
            case Char['-']: {
              const start = this.#nextIndexLocation();
              this.#nextIndex += 2;
              return { kind: Kind['--'], start, length: 2, value: undefined };
            }
            case Char['>']: {
              const start = this.#nextIndexLocation();
              this.#nextIndex += 2;
              return { kind: Kind['->'], start, length: 2, value: undefined };
            }
          }
      }

      const start = this.#nextIndexLocation();
      if (isNameStart(char)) {
        return this.#readName();
      }
      if (isNumberStart(char)) {
        return this.#readNumber();
      }

      ++this.#nextIndex;
      return { kind: Kind.UnexpectedChar, start, length: 1, value: undefined };
    }

    return {
      kind: Kind.EOF,
      start: this.#nextIndexLocation(),
      length: 0,
      value: undefined,
    };
  }

  #readNumber(): Token {
    const start = this.#nextIndexLocation();
    // [-]?.[0-9]⁺ or [-]?[0-9]⁺(.[0-9]*)?
    this.#skipChar(Char['-']);
    const hasLeadingDecimalPoint = this.#skipChar(Char['.']);
    if (!isDigit(this.#peekChar())) {
      this.#nextIndex = start.index + 1;
      return { kind: Kind.UnexpectedChar, start, length: 1, value: undefined };
    }
    while (isDigit(this.#peekChar())) {
      ++this.#nextIndex;
    }
    if (!hasLeadingDecimalPoint) {
      this.#skipChar(Char['.']);
      while (isDigit(this.#peekChar())) {
        ++this.#nextIndex;
      }
    }

    const length = this.#nextIndex - start.index;
    const value = this.#dotStr.slice(start.index, this.#nextIndex);
    return { kind: Kind.Number, start, length, value };
  }

  #readHTML(): Token {
    const start = this.#nextIndexLocation();
    let nestDepth = 0;
    while (this.#nextIndex < this.#dotStr.length) {
      switch (this.#readChar()) {
        case Char['\n']:
          this.#countNewLine();
          break;
        case Char['<']:
          ++nestDepth;
          break;
        case Char['>']:
          --nestDepth;
          if (nestDepth === 0) {
            const length = this.#nextIndex - start.index;
            const value = this.#dotStr.slice(
              start.index + 1,
              this.#nextIndex - 1,
            );
            return { kind: Kind.HTML, start, length, value };
          }
          break;
      }
    }

    return {
      kind: Kind.UnterminatedHTML,
      start,
      length: this.#nextIndex - start.index,
      value: undefined,
    };
  }

  #readString(): Token {
    const start = this.#nextIndexLocation();
    let value = '';
    ++this.#nextIndex; // skip opening `"`
    let checkpoint = this.#nextIndex;

    let escapeStartIndex = undefined;
    while (this.#nextIndex < this.#dotStr.length) {
      switch (this.#readChar()) {
        case Char['"']:
          if (escapeStartIndex === undefined) {
            value += this.#dotStr.slice(checkpoint, this.#nextIndex - 1);
            const length = this.#nextIndex - start.index;
            return { kind: Kind.String, start, length, value };
          }
          value += this.#dotStr.slice(checkpoint, escapeStartIndex);
          checkpoint = this.#nextIndex - 1;
          escapeStartIndex = undefined;
          break;
        case Char['\r']:
          if (this.#peekChar() !== Char['\n']) {
            escapeStartIndex = undefined;
          }
          break;
        case Char['\n']:
          this.#countNewLine();
          if (escapeStartIndex !== undefined) {
            value += this.#dotStr.slice(checkpoint, escapeStartIndex);
            checkpoint = this.#nextIndex;
            escapeStartIndex = undefined;
          }
          break;
        case Char['\\']:
          escapeStartIndex =
            escapeStartIndex === undefined ? this.#nextIndex - 1 : undefined;
          break;
        default:
          escapeStartIndex = undefined;
          break;
      }
    }

    const length = this.#nextIndex - start.index;
    return { kind: Kind.UnterminatedString, start, length, value: undefined };
  }

  #readName(): Token {
    const start = this.#nextIndexLocation();
    while (isNameContinue(this.#peekChar())) {
      ++this.#nextIndex;
    }

    const length = this.#nextIndex - start.index;
    const value = this.#dotStr.slice(start.index, this.#nextIndex);
    const keywordIndex = keywordStrings.indexOf(value.toLowerCase());
    if (keywordIndex === -1) {
      return { kind: Kind.Name, start, length, value };
    }

    const kind = (keywordIndex | KEYWORD) as KeywordKind;
    return { kind, start, length, value: undefined };
  }
}

function canTokenClash(token: Token): boolean {
  return (
    token.kind === Kind.Name ||
    token.kind === Kind.Number ||
    (token.kind & KEYWORD) !== 0
  );
}

function literalOrKeywordLabel(kind: LiteralKind | KeywordKind): string {
  if (kind & LITERAL) {
    let literal = String.fromCharCode(kind ^ LITERAL);
    if (kind === Kind['--'] || kind === Kind['->']) {
      literal = '-' + literal;
    }
    return "'" + literal + "'";
  }

  const keyword = keywordStrings[kind ^ KEYWORD];
  return `keyword '${keyword}'`;
}

function formatValueForMessage(value: string) {
  const truncated = value.length > 20 ? value.slice(0, 17) + '...' : value;
  return JSON.stringify(truncated)
    .replaceAll(String.raw`\"`, '"')
    .replaceAll(String.raw`\\`, '\\')
    .slice(1, -1);
}

function capitalize(str: string): string {
  return str.charAt(0).toUpperCase() + str.slice(1);
}

function isDigit(code: number): boolean {
  return code >= Char['0'] && code <= Char['9'];
}

function isNumberStart(code: number): boolean {
  return isNumberContinue(code) || code === Char['-'];
}

function isNumberContinue(code: number): boolean {
  return isDigit(code) || code === Char['.'];
}

function isLetter(code: number): boolean {
  return (
    (code >= Char.A && code <= Char.Z) || (code >= Char.a && code <= Char.z)
  );
}

function isNameStart(code: number): boolean {
  return isLetter(code) || code === Char._ || code > 127;
}

function isNameContinue(code: number): boolean {
  return isNameStart(code) || isDigit(code);
}

interface ParsedID {
  readonly value: string | { html: string };
  readonly token: Token;
}

interface ParsedName {
  readonly value: string;
  readonly token: Token;
}

const COMPASS_POINTS = new Set([
  'n',
  'ne',
  'e',
  'se',
  's',
  'sw',
  'w',
  'nw',
  'c',
  '_',
]);

interface NodeID {
  readonly node: ParsedName;
  readonly port: ParsedName | undefined;
  readonly compass: ParsedName | undefined;
}

class ParserError implements RenderError {
  readonly level = 'error' as const;
  readonly message: string;
  readonly location: Readonly<Location>;
  readonly #dotStr: string;

  constructor(message: string, location: Location, dotStr: string) {
    this.message = message;
    this.location = location;
    this.#dotStr = dotStr;
  }

  toString() {
    return printLocation(
      'ParserError: ' + this.message,
      this.#dotStr,
      this.location,
    );
  }
}

class ParserWarning implements RenderError {
  readonly level = 'warning' as const;
  readonly message: string;
  readonly location: Readonly<Location>;
  readonly #dotStr: string;

  constructor(message: string, location: Location, dotStr: string) {
    this.message = message;
    this.location = location;
    this.#dotStr = dotStr;
  }

  toString() {
    return printLocation(
      'ParserWarning: ' + this.message,
      this.#dotStr,
      this.location,
    );
  }
}

export interface ParseResult {
  readonly graph: NormalizedGraph | undefined;
  readonly diagnostics: (ParserWarning | ParserError)[];
}

class Parser {
  static readonly #AbortError = new Error(
    'Internal error, thrown when parsing of dot file fails',
  );

  static parseDot(
    dotStr: string,
    overrideAttributes: OverrideAttributes,
  ): ParseResult[] {
    const parser = new Parser(dotStr);
    const result: ParseResult[] = [];
    try {
      while (!parser.#isEOF()) {
        const graph = parser.#parseGraph(overrideAttributes);
        const diagnostics = parser.#diagnostics.splice(0);
        result.push({ graph, diagnostics });
      }
    } catch (error: unknown) {
      /* c8 ignore start -- Should only happen in case of internal errors */
      if (error !== Parser.#AbortError) {
        throw error;
      }
      /* c8 ignore end */
      const diagnostics = parser.#diagnostics.splice(0);
      result.push({ graph: undefined, diagnostics });
    }

    return result;
  }

  readonly #dotStr: string;
  readonly #lexer: Lexer;
  #peekToken: Token;
  #peekAheadToken: Token;
  readonly #diagnostics: (ParserError | ParserWarning)[] = [];

  constructor(dotStr: string) {
    this.#dotStr = dotStr;
    this.#lexer = new Lexer(dotStr);
    this.#peekToken = this.#lexer.nextToken();
    this.#peekAheadToken = this.#lexer.nextToken();
  }

  #isEOF(): boolean {
    return this.#peekToken.kind === Kind.EOF;
  }

  #peekIs(mask: number): boolean {
    return (this.#peekToken.kind & mask) != 0;
  }

  #peekKind(): Kind {
    return this.#peekToken.kind;
  }

  #extractText(token: Token): string {
    const start = token.start.index;
    const end = start + token.length;
    return this.#dotStr.slice(start, end);
  }

  #describeToken(token: Token): string {
    const { kind } = token;
    switch (kind) {
      case Kind.EOF:
        return 'end of file';
      case Kind.Number: {
        const value = formatValueForMessage(this.#extractText(token));
        return `number '${value}'`;
      }
      case Kind.Name: {
        const value = formatValueForMessage(this.#extractText(token));
        return `identifier '${value}'`;
      }
      case Kind.String: {
        const value = formatValueForMessage(
          this.#extractText(token).slice(1, -1),
        );
        return `string "${value}"`;
      }
      case Kind.HTML: {
        const value = formatValueForMessage(
          this.#extractText(token).slice(1, -1),
        );
        return `HTML string <${value}>`;
      }
      case Kind.UnexpectedChar: {
        const value = formatValueForMessage(this.#extractText(token));
        return `character '${value}'`;
      }
      case Kind.UnterminatedString: {
        const value = formatValueForMessage(this.#extractText(token));
        return `unterminated string '${value}'`;
      }
      case Kind.UnterminatedHTML: {
        const value = formatValueForMessage(this.#extractText(token));
        return `unterminated HTML string '${value}'`;
      }
      case Kind.UnterminatedBlockComment: {
        const value = formatValueForMessage(this.#extractText(token));
        return `unterminated block comment '${value}'`;
      }
    }
    return literalOrKeywordLabel(kind);
  }

  #readToken(): Token {
    const result = this.#peekToken;
    if (result.kind === Kind.UnterminatedBlockComment) {
      const tokenDesc = this.#describeToken(result);
      this.#failWithError(
        `Unexpected ${tokenDesc}, add a closing '*/' to the comment.`,
        result,
      );
    }

    this.#peekToken = this.#peekAheadToken;
    this.#peekAheadToken = this.#lexer.nextToken();
    this.#warnIfAmbiguous(result, this.#peekToken);
    return result;
  }

  #warnIfAmbiguous(lastToken: Token, nextToken: Token): void {
    const lastEnd = lastToken.start.index + lastToken.length;
    const nextStart = nextToken.start.index;

    if (
      lastEnd === nextStart &&
      canTokenClash(lastToken) &&
      canTokenClash(nextToken)
    ) {
      const lastTokenText = this.#describeToken(lastToken);
      const nextTokenText = this.#describeToken(nextToken);
      const ambiguousText: string = formatValueForMessage(
        this.#extractText(lastToken) + this.#extractText(nextToken),
      );
      const message =
        `Ambiguous token sequence: '${ambiguousText}' will be split into ${lastTokenText} and ${nextTokenText}.` +
        ' If you want it interpreted as a single value, use quotes: "...". Otherwise, use whitespace or other delimiters to separate tokens.';

      this.#diagnostics.push(
        new ParserWarning(message, lastToken.start, this.#dotStr),
      );
    }
  }

  #optional(kind: Kind): boolean {
    if (this.#peekToken.kind === kind) {
      this.#readToken();
      return true;
    }
    return false;
  }

  #expected(kind: LiteralKind | KeywordKind): void {
    if (!this.#optional(kind)) {
      const token = this.#readToken();
      const tokenDesc = this.#describeToken(token);
      this.#failWithError(
        `Unexpected ${tokenDesc}, expected ${literalOrKeywordLabel(kind)}.`,
        token,
      );
    }
  }

  #optionalName(description: string): string | undefined {
    if (this.#peekIs(ID | KEYWORD)) {
      return this.#expectedName(description).value;
    }
    return undefined;
  }

  #expectedName(description: string): ParsedName {
    const { value, token } = this.#expectedValue(description);
    if (typeof value === 'object') {
      const html = this.#extractText(token);
      this.#failWithError(
        `HTML string as ${description} is not supported. If you want to use it as an identifier, enclose it in quotes: "${html}".`,
        token,
      );
    }
    return { value, token };
  }

  #expectedValue(description: string): ParsedID {
    const token = this.#readToken();
    if (token.kind & KEYWORD) {
      const keyword = this.#extractText(token);
      this.#failWithError(
        `Unexpected reserved keyword '${keyword}' where ${description} was expected. If you want to use it as an identifier, enclose it in quotes: "${keyword}".`,
        token,
      );
    }

    switch (token.kind) {
      case Kind.Name:
      case Kind.Number:
        return { value: token.value, token };
      case Kind.String:
        return { value: this.#readConcatenatedString(token), token };
      case Kind.HTML:
        return { value: { html: token.value }, token };
    }

    const tokenDesc = this.#describeToken(token);
    switch (token.kind) {
      case Kind.UnterminatedString:
        return this.#failWithError(
          `${capitalize(tokenDesc)}, add a closing '"' to the string.`,
          token,
        );
      case Kind.UnterminatedHTML:
        return this.#failWithError(
          `${capitalize(tokenDesc)}, add a closing '>' to the HTML string.`,
          token,
        );
      default:
        this.#failWithError(
          `Unexpected ${tokenDesc}, expected ${description}. If this is meant to be part of a label or name, enclose it in quotes ("...").`,
          token,
        );
    }
  }

  #readConcatenatedString(firstToken: IDToken): string {
    let text = firstToken.value;

    while (this.#optional(Kind['+'])) {
      const token = this.#readToken();
      if (token.kind !== Kind.String) {
        this.#failWithError(
          `Unexpected ${this.#describeToken(token)}, expected a string literal.`,
          token,
        );
      }
      text += token.value;
    }
    return text;
  }

  #failWithError(message: string, token: Token): never {
    this.#diagnostics.push(new ParserError(message, token.start, this.#dotStr));
    throw Parser.#AbortError;
  }

  #parseGraph(overrideAttributes: OverrideAttributes): NormalizedGraph {
    // graph:	[ strict ] (graph | digraph) [ ID ] '{' stmt_list '}'
    const strict = this.#optional(Kind.strict);
    const directed = this.#optional(Kind.digraph);

    if (!directed && !this.#optional(Kind.graph)) {
      const token = this.#readToken();
      const tokenDesc = this.#describeToken(token);
      this.#failWithError(
        `Unexpected ${tokenDesc}, expected keyword ` +
          (strict
            ? `'graph' or 'digraph' after 'strict'.`
            : `'strict', 'graph' or 'digraph' at the beginning of the file.`),
        token,
      );
    }

    const name = this.#optionalName('graph name');
    const graph = new NormalizedGraph(
      {
        strict,
        directed,
        name,
        graphAttributes: {},
        // FIXME: check if it's viz.js hack or it also present in graphviz
        nodeAttributes: { label: String.raw`\N` },
        edgeAttributes: {},
      },
      overrideAttributes,
    );

    this.#parseStatementList(graph);
    return graph;
  }

  #parseStatementList(owner: NormalizedGraph | NormalizedSubgraph): void {
    // '{' stmt_list '}'
    this.#expected(Kind['{']);
    while (!this.#optional(Kind['}'])) {
      if (this.#isEOF()) {
        this.#failWithError(
          `Unexpected end of file. Add a closing '}' to match the opening '{' of the graph or subgraph.`,
          this.#readToken(),
        );
      }

      // stmt_list	:	[ stmt [ ';' ] stmt_list ]
      this.#parseStatement(owner);
      this.#optional(Kind[';']);
    }
  }

  #parseStatement(owner: NormalizedGraph | NormalizedSubgraph): void {
    // stmt: node_stmt |	edge_stmt |	attr_stmt |	ID '=' ID |	subgraph
    if (this.#peekIs(ID)) {
      if (this.#peekAheadToken.kind === Kind['=']) {
        // ID '=' ID
        const attributes: Attributes = {};
        this.#parseAttr(attributes);
        owner.mergeGraphAttributes(attributes);
        return;
      }

      // node_stmt: node_id [ attr_list ]
      const nodeIDs = this.#parseNodeIDList();
      if (this.#optionalEdgeOp(owner)) {
        const tailNodes = this.#upsertEdgeEndpoints(owner, nodeIDs);
        this.#parseEdges(owner, tailNodes);
        return;
      }

      const attributes = this.#optionalAttrListOrEmpty();
      for (const { node, port } of nodeIDs) {
        if (port !== undefined) {
          this.#failWithError(
            `Unexpected '${port.value}' port in node statement`,
            port.token,
          );
        }
        owner.root.upsertNode(owner, { name: node.value, attributes });
      }
      return;
    }

    switch (this.#peekKind()) {
      case Kind['{']: {
        const subgraph = this.#parseSubgraph(owner, undefined);
        if (this.#optionalEdgeOp(owner)) {
          const tailNodes = subgraph
            .sortedMemberNodes()
            .map((node) => node.defaultEndpoint);
          this.#parseEdges(owner, tailNodes);
        }
        break;
      }
      case Kind.subgraph: {
        const subgraph = this.#parseNamedSubgraph(owner);
        if (this.#optionalEdgeOp(owner)) {
          const tailNodes = subgraph
            .sortedMemberNodes()
            .map((node) => node.defaultEndpoint);
          this.#parseEdges(owner, tailNodes);
        }
        break;
      }

      // attr_stmt:	(graph | node | edge) attr_list
      case Kind.graph:
        this.#readToken();
        owner.mergeGraphAttributes(this.#parseAttrList());
        break;
      case Kind.node:
        this.#readToken();
        owner.mergeNodeAttributes(this.#parseAttrList());
        break;
      case Kind.edge:
        this.#readToken();
        owner.mergeEdgeAttributes(this.#parseAttrList());
        break;
      default: {
        const token = this.#readToken();
        const tokenDesc = this.#describeToken(token);
        this.#failWithError(
          `Unexpected ${tokenDesc}, expected node, edge, subgraph or attribute statement. If this is meant to be part of a label or name, enclose it in quotes ("...").`,
          token,
        );
      }
    }
  }

  #upsertEdgeEndpoints(
    owner: NormalizedGraph | NormalizedSubgraph,
    nodeIDs: NodeID[],
  ): NormalizedEdgeEndpoint[] {
    return nodeIDs.map((nodeID) => {
      const node = owner.root.upsertNode(owner, {
        name: nodeID.node.value,
        attributes: {},
      });
      const compass = nodeID.compass?.value;
      if (nodeID.port === undefined) {
        return nodeID.compass
          ? { port: node.defaultPort, compass }
          : node.defaultEndpoint;
      }
      const port = node.upsertPort(nodeID.port.value);
      return { port, compass };
    });
  }

  #parseNodeIDList(): NodeID[] {
    const list: NodeID[] = [];
    do {
      list.push(this.#parseNodeID());
    } while (this.#optional(Kind[',']));
    return list;
  }

  #parseNodeID(): NodeID {
    // node_id:	ID [ port ]
    const node = this.#expectedName('node name');

    // port: ':' ID [ ':' compass_pt ]
    if (!this.#optional(Kind[':'])) {
      return { node, port: undefined, compass: undefined };
    }

    const port = this.#expectedName('port name');
    if (!this.#optional(Kind[':'])) {
      return { node, port, compass: undefined };
    }

    // compass_pt: n | ne | e | se | s | sw | w | nw | c | _
    const compass = this.#expectedName('compass point value');

    if (!COMPASS_POINTS.has(compass.value)) {
      const tokenDesc = this.#describeToken(compass.token);
      const allowedValues = [...COMPASS_POINTS.values()].join(', ');
      this.#failWithError(
        `Invalid compass point ${tokenDesc}. Allowed values: ${allowedValues}.`,
        compass.token,
      );
    }
    return { node, port, compass };
  }

  #optionalAttrListOrEmpty(): Readonly<Attributes> {
    return this.#peekKind() === Kind['['] ? this.#parseAttrList() : {};
  }

  #parseAttrList(): Readonly<Attributes> {
    const attributes: Attributes = {};

    // attr_list:	'[' [ a_list ] ']' [ attr_list ]
    do {
      this.#expected(Kind['[']);
      // a_list: ID '=' ID [ (';' | ',') ] [ a_list ]
      while (!this.#optional(Kind[']'])) {
        this.#parseAttr(attributes);

        // Either ';' or ',' but doesn't allow both
        if (!this.#optional(Kind[';'])) {
          this.#optional(Kind[',']);
        }
      }
    } while (this.#peekKind() === Kind['[']);

    return attributes;
  }

  #parseAttr(attributes: Attributes): void {
    const name = this.#expectedName('attribute name').value;
    this.#expected(Kind['=']);
    attributes[name] = this.#expectedValue('attribute value').value;
  }

  #parseNamedSubgraph(
    owner: NormalizedGraph | NormalizedSubgraph,
  ): NormalizedSubgraph {
    this.#expected(Kind.subgraph);
    const name = this.#optionalName('subgraph name');
    return this.#parseSubgraph(owner, name);
  }

  #parseSubgraph(
    owner: NormalizedGraph | NormalizedSubgraph,
    name: string | undefined,
  ): NormalizedSubgraph {
    const subgraph = owner.upsertSubgraph({
      name,
      graphAttributes: {},
      nodeAttributes: {},
      edgeAttributes: {},
    });
    this.#parseStatementList(subgraph);
    return subgraph;
  }

  #optionalEdgeOp(owner: NormalizedGraph | NormalizedSubgraph): boolean {
    const { directed } = owner.root;
    const kind = this.#peekKind();
    if (kind === Kind['--']) {
      const token = this.#readToken();
      if (directed) {
        this.#failWithError(
          `Unexpected '--' in a directed graph. Use '->' for directed edges in a 'digraph'.`,
          token,
        );
      }
      return true;
    }
    if (kind === Kind['->']) {
      const token = this.#readToken();
      if (!directed) {
        this.#failWithError(
          `Unexpected '->' in an undirected graph. Use '--' for undirected edges in a 'graph'.`,
          token,
        );
      }
      return true;
    }
    return false;
  }

  #parseEdges(
    owner: NormalizedGraph | NormalizedSubgraph,
    tailNodes: NormalizedEdgeEndpoint[],
  ) {
    const newEdges: [NormalizedEdgeEndpoint, NormalizedEdgeEndpoint][] = [];
    do {
      let headNodes: NormalizedEdgeEndpoint[];
      switch (this.#peekKind()) {
        case Kind['{']:
          headNodes = this.#parseSubgraph(owner, undefined)
            .sortedMemberNodes()
            .map((node) => node.defaultEndpoint);
          break;
        case Kind.subgraph:
          headNodes = this.#parseNamedSubgraph(owner)
            .sortedMemberNodes()
            .map((node) => node.defaultEndpoint);
          break;
        default:
          headNodes = this.#upsertEdgeEndpoints(owner, this.#parseNodeIDList());
      }

      for (const tail of tailNodes) {
        for (const head of headNodes) {
          newEdges.push([tail, head]);
        }
      }

      // head of this step becomes the tail on next step, e.g. a -> b -> c
      tailNodes = headNodes;
    } while (this.#optionalEdgeOp(owner));

    const attributes = this.#optionalAttrListOrEmpty();
    for (const [tail, head] of newEdges) {
      owner.root.upsertEdge(owner, { tail, head, key: undefined, attributes });
    }
  }
}

export const parseDot = Parser.parseDot;
