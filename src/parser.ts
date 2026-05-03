import type { Attributes } from './graph.d.ts';
import { type Location, printLocation } from './location.ts';
import {
  type NormalizedEdgeEndpoint,
  NormalizedGraph,
  NormalizedSubgraph,
  type OverrideAttributes,
} from './normalize-graph.ts';
import type { FailureResult, RenderError } from './viz.ts';

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

interface Token {
  readonly kind: Kind;
  readonly start: Location;
  readonly length: number;
}

class Lexer {
  readonly #dotStr: string;
  #line = 1;
  #lineStart = 0;
  #nextIndex = 0;

  constructor(dotStr: string) {
    this.#dotStr = dotStr;
  }

  #peekChar(offset = 0): number | undefined {
    return this.#dotStr.codePointAt(this.#nextIndex + offset);
  }

  #skipChar(char: number): boolean {
    if (this.#peekChar() === char) {
      ++this.#nextIndex;
      return true;
    }
    return false;
  }

  #readNewLine(): void {
    ++this.#line;
    ++this.#nextIndex;
    this.#lineStart = this.#nextIndex;
  }

  #readUntilNewLine(): void {
    while (this.#nextIndex < this.#dotStr.length) {
      if (this.#peekChar() === Char['\n']) {
        this.#readNewLine();
        break;
      }
      ++this.#nextIndex;
    }
  }

  #nextIndexLocation(): Location {
    return {
      index: this.#nextIndex,
      line: this.#line,
      column: this.#nextIndex - this.#lineStart + 1,
    };
  }

  #readBlockComment(): Token | null {
    const start = this.#nextIndexLocation();

    this.#nextIndex += 2; // skip `/*`
    while (true) {
      switch (this.#peekChar()) {
        case undefined:
          return {
            kind: Kind.UnterminatedBlockComment,
            start,
            length: this.#nextIndex - start.index,
          };
        case Char['\n']:
          this.#readNewLine();
          break;
        case Char['*']:
          ++this.#nextIndex;
          if (this.#skipChar(Char['/'])) {
            return null;
          }
          break;
        default:
          ++this.#nextIndex;
      }
    }
  }

  #skipUntilTokenStart(): Token | null {
    while (true) {
      switch (this.#peekChar()) {
        // Ignored:
        case Char.BOM:
        case Char['\r']:
        case Char['\t']:
        case Char[' ']:
          ++this.#nextIndex;
          break;
        case Char['\n']:
          this.#readNewLine();
          continue;
        case Char['#']:
          this.#readUntilNewLine();
          continue;
        case Char['/']:
          switch (this.#peekChar(1)) {
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
            default:
              return null;
          }
        default:
          return null;
      }
    }
  }

  nextToken(): Token {
    const invalidToken = this.#skipUntilTokenStart();
    if (invalidToken) {
      return invalidToken;
    }

    const start = this.#nextIndexLocation();
    const char = this.#peekChar();
    switch (char) {
      case undefined:
        return { kind: Kind.EOF, start, length: 0 };
      case Char['+']:
      case Char[',']:
      case Char[':']:
      case Char[';']:
      case Char['=']:
      case Char['[']:
      case Char[']']:
      case Char['{']:
      case Char['}']:
        ++this.#nextIndex;
        return { kind: (char | LITERAL) as Kind, start, length: 1 };
      case Char['<']:
        return this.#readHTML(start);
      case Char['"']:
        return this.#readString(start);
      case Char['-']: {
        const nextChar = this.#peekChar(1);
        switch (nextChar) {
          case Char['-']:
          case Char['>']:
            this.#nextIndex += 2;
            return { kind: (nextChar | LITERAL) as Kind, start, length: 2 };
        }
        break;
      }
    }
    if (isNameStart(char)) {
      return this.#readName(start);
    }
    if (isNumberStart(char)) {
      return this.#readNumber(start);
    }

    ++this.#nextIndex;
    return { kind: Kind.UnexpectedChar, start, length: 1 };
  }

  #readNumber(start: Location): Token {
    // [-]?.[0-9]⁺ or [-]?[0-9]⁺(.[0-9]*)?
    this.#skipChar(Char['-']);
    const hasLeadingDecimalPoint = this.#skipChar(Char['.']);
    if (!isDigit(this.#peekChar())) {
      this.#nextIndex = start.index + 1;
      return { kind: Kind.UnexpectedChar, start, length: 1 };
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
    return { kind: Kind.Number, start, length };
  }

  #readHTML(start: Location): Token {
    let nestDepth = 0;
    do {
      switch (this.#peekChar()) {
        case undefined:
          return {
            kind: Kind.UnterminatedHTML,
            start,
            length: this.#nextIndex - start.index,
          };
        case Char['\n']:
          this.#readNewLine();
          break;
        case Char['<']:
          ++this.#nextIndex;
          ++nestDepth;
          break;
        case Char['>']:
          ++this.#nextIndex;
          --nestDepth;
          break;
        default:
          ++this.#nextIndex;
          break;
      }
    } while (nestDepth !== 0);

    const length = this.#nextIndex - start.index;
    return { kind: Kind.HTML, start, length };
  }

  #readString(start: Location): Token {
    let isEscaped = false;
    ++this.#nextIndex; // skip opening `"`
    while (true) {
      switch (this.#peekChar()) {
        case undefined:
          return {
            kind: Kind.UnterminatedString,
            start,
            length: this.#nextIndex - start.index,
          };
        case Char['\n']:
          this.#readNewLine();
          isEscaped = false;
          break;
        case Char['\\']:
          ++this.#nextIndex;
          isEscaped = !isEscaped;
          continue;
        case Char['"']:
          ++this.#nextIndex;
          if (!isEscaped) {
            const length = this.#nextIndex - start.index;
            return { kind: Kind.String, start, length };
          }
          isEscaped = false;
          break;
        default:
          ++this.#nextIndex;
          isEscaped = false;
          break;
      }
    }
  }

  #readName(start: Location): Token {
    ++this.#nextIndex;
    while (isNameContinue(this.#peekChar())) {
      ++this.#nextIndex;
    }

    const length = this.#nextIndex - start.index;
    const maybeKeyword = this.#dotStr
      .slice(start.index, this.#nextIndex)
      .toLowerCase();
    const keywordIndex = keywordStrings.indexOf(maybeKeyword);
    if (keywordIndex === -1) {
      return { kind: Kind.Name, start, length };
    }

    return { kind: (keywordIndex | KEYWORD) as Kind, start, length };
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
    let literal = String.fromCodePoint(kind ^ LITERAL);
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

function isDigit(code: number | undefined): boolean {
  return code != undefined && code >= Char['0'] && code <= Char['9'];
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

function isNameContinue(code: number | undefined): boolean {
  return code !== undefined && (isNameStart(code) || isDigit(code));
}

export interface ParseSuccessResult {
  readonly status: 'success';
  readonly output: NormalizedGraph;
  readonly errors: RenderError[];
}

export type ParseResult = ParseSuccessResult | FailureResult;

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
  readonly port: ParsedName | null;
  readonly compass: ParsedName | null;
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

class Parser {
  static readonly #AbortError = new Error(
    'Internal error, thrown when parsing of dot file fails',
  );

  static parseDot(
    dotStr: string,
    overrideAttributes: OverrideAttributes,
  ): ParseResult {
    const parser = new Parser(dotStr);
    try {
      const graph = parser.#parseGraph(overrideAttributes);
      return {
        status: 'success',
        output: graph,
        errors: parser.#diagnostics,
      };
    } catch (error: unknown) {
      /* c8 ignore start -- Should only happen in case of internal errors */
      if (error !== Parser.#AbortError) {
        throw error;
      }
      /* c8 ignore end */
      return {
        status: 'failure',
        output: null,
        errors: parser.#diagnostics,
      };
    }
  }

  readonly #dotStr: string;
  readonly #lexer: Lexer;
  #peekToken: Token;
  #peekToken2: Token;
  readonly #diagnostics: Readonly<ParserError | ParserWarning>[] = [];

  constructor(dotStr: string) {
    this.#dotStr = dotStr;
    this.#lexer = new Lexer(dotStr);
    this.#peekToken = this.#lexer.nextToken();
    this.#peekToken2 = this.#lexer.nextToken();
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

    this.#peekToken = this.#peekToken2;
    this.#peekToken2 = this.#lexer.nextToken();
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

  #optionalName(description: string): string | null {
    if (this.#peekIs(ID | KEYWORD)) {
      return this.#expectedName(description).value;
    }
    return null;
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
        return { value: this.#extractText(token), token };
      case Kind.String:
        return { value: this.#readString(token), token };
      case Kind.HTML:
        return {
          value: { html: this.#extractText(token).slice(1, -1) },
          token,
        };
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

  #readString(firstToken: Token): string {
    let text = this.#extractText(firstToken).slice(1, -1);

    while (this.#optional(Kind['+'])) {
      const token = this.#readToken();
      if (token.kind !== Kind.String) {
        this.#failWithError(
          `Unexpected ${this.#describeToken(token)}, expected a string literal.`,
          token,
        );
      }
      text += this.#extractText(token).slice(1, -1);
    }

    return text.replaceAll(String.raw`\"`, '"').replaceAll('\\\n', '');
  }

  #failWithError(message: string, token: Token): never {
    this.#diagnostics.push(new ParserError(message, token.start, this.#dotStr));
    throw Parser.#AbortError;
  }

  #parseGraph(overrideAttributes: OverrideAttributes): NormalizedGraph {
    if (this.#isEOF()) {
      this.#failWithError(
        "Missing graph definition. Start your file with 'graph {}' or 'digraph {}'.",
        this.#readToken(),
      );
    }

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

  #parseStatementList(scope: NormalizedGraph | NormalizedSubgraph): void {
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
      this.#parseStatement(scope);
      this.#optional(Kind[';']);
    }
  }

  #parseStatement(scope: NormalizedGraph | NormalizedSubgraph): void {
    // stmt: node_stmt |	edge_stmt |	attr_stmt |	ID '=' ID |	subgraph
    if (this.#peekIs(ID)) {
      if (this.#peekToken2.kind === Kind['=']) {
        // ID '=' ID
        const attributes: Attributes = {};
        this.#parseAttr(attributes);
        scope.mergeGraphAttributes(attributes);
        return;
      }

      // node_stmt: node_id [ attr_list ]
      const nodeIDs = this.#parseNodeIDList();
      if (this.#optionalEdgeOp(scope)) {
        const tailNodes = this.#upsertEdgeEndpoints(scope, nodeIDs);
        this.#parseEdges(scope, tailNodes);
        return;
      }

      const attributes = this.#optionalAttrList();
      for (const { node, port } of nodeIDs) {
        if (port != null) {
          this.#failWithError(
            `Unexpected '${port.value}' port in node statement`,
            port.token,
          );
        }
        scope.root.upsertNode(scope, { name: node.value, attributes });
      }
      return;
    }

    switch (this.#peekKind()) {
      case Kind['{']: {
        const subgraph = this.#parseSubgraph(scope, null);
        if (this.#optionalEdgeOp(scope)) {
          const tailNodes = subgraph
            .sortedMemberNodes()
            .map((node) => node.defaultEndpoint);
          this.#parseEdges(scope, tailNodes);
        }
        break;
      }
      case Kind.subgraph: {
        const subgraph = this.#parseNamedSubgraph(scope);
        if (this.#optionalEdgeOp(scope)) {
          const tailNodes = subgraph
            .sortedMemberNodes()
            .map((node) => node.defaultEndpoint);
          this.#parseEdges(scope, tailNodes);
        }
        break;
      }

      // attr_stmt:	(graph | node | edge) attr_list
      case Kind.graph:
        this.#readToken();
        scope.mergeGraphAttributes(this.#parseAttrList());
        break;
      case Kind.node:
        this.#readToken();
        scope.mergeNodeAttributes(this.#parseAttrList());
        break;
      case Kind.edge:
        this.#readToken();
        scope.mergeEdgeAttributes(this.#parseAttrList());
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
    scope: NormalizedGraph | NormalizedSubgraph,
    nodeIDs: NodeID[],
  ): NormalizedEdgeEndpoint[] {
    return nodeIDs.map((nodeID) => {
      const node = scope.root.upsertNode(scope, {
        name: nodeID.node.value,
        attributes: {},
      });
      const compass = nodeID.compass?.value ?? null;
      if (nodeID.port == null) {
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
      return { node, port: null, compass: null };
    }

    const port = this.#expectedName('port name');
    if (!this.#optional(Kind[':'])) {
      return { node, port, compass: null };
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

  #optionalAttrList(): Readonly<Attributes> {
    return this.#peekKind() === Kind['['] ? this.#parseAttrList() : {};
  }

  #parseAttrList(): Readonly<Attributes> {
    const attributes: Readonly<Attributes> = {};

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
    scope: NormalizedGraph | NormalizedSubgraph,
  ): NormalizedSubgraph {
    this.#expected(Kind.subgraph);
    const name = this.#optionalName('subgraph name');
    return this.#parseSubgraph(scope, name);
  }

  #parseSubgraph(
    scope: NormalizedGraph | NormalizedSubgraph,
    name: string | null,
  ): NormalizedSubgraph {
    const subgraph = scope.upsertSubgraph({
      name,
      graphAttributes: {},
      nodeAttributes: {},
      edgeAttributes: {},
    });
    this.#parseStatementList(subgraph);
    return subgraph;
  }

  #optionalEdgeOp(scope: NormalizedGraph | NormalizedSubgraph): boolean {
    const { directed } = scope.root;
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
    scope: NormalizedGraph | NormalizedSubgraph,
    tailNodes: NormalizedEdgeEndpoint[],
  ) {
    const newEdges: [NormalizedEdgeEndpoint, NormalizedEdgeEndpoint][] = [];
    do {
      let headNodes: NormalizedEdgeEndpoint[];
      switch (this.#peekKind()) {
        case Kind['{']:
          headNodes = this.#parseSubgraph(scope, null)
            .sortedMemberNodes()
            .map((node) => node.defaultEndpoint);
          break;
        case Kind.subgraph:
          headNodes = this.#parseNamedSubgraph(scope)
            .sortedMemberNodes()
            .map((node) => node.defaultEndpoint);
          break;
        default:
          headNodes = this.#upsertEdgeEndpoints(scope, this.#parseNodeIDList());
      }

      for (const tail of tailNodes) {
        for (const head of headNodes) {
          newEdges.push([tail, head]);
        }
      }

      // head of this step becomes the tail on next step, e.g. a -> b -> c
      tailNodes = headNodes;
    } while (this.#optionalEdgeOp(scope));

    const attributes = this.#optionalAttrList();
    for (const [tail, head] of newEdges) {
      scope.root.upsertEdge(scope, { tail, head, key: null, attributes });
    }
  }
}

export const parseDot = Parser.parseDot;
