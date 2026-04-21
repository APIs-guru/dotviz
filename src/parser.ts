import type { Attributes } from './graph.d.ts';
import {
  extractKeyFromEdgeAttributes,
  type FixedAttributes,
  NormalizedGraph,
  NormalizedNode,
  NormalizedSubgraph,
} from './normalize-graph.ts';
import type { FailureResult, Location, RenderError } from './viz.ts';

const Char = {
  '\t': 0x09,
  '\n': 0x0a,
  '\r': 0x0d,
  ' ': 0x20,
  '"': 0x22,
  '#': 0x23,
  '*': 0x2a,
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
  kind: Kind;
  start: Location;
  length: number;
}

class Lexer {
  #dotStr: string;
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
    ++this.#nextIndex;
    ++this.#line;
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

  #readBlockComment(): Token | null {
    const start: Location = {
      index: this.#nextIndex,
      line: this.#line,
      column: this.#nextIndex - this.#lineStart,
    };

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

  extractText(token: Token): string {
    const start = token.start.index;
    const end = start + token.length;
    return this.#dotStr.slice(start, end);
  }

  tokenToDebug(token: Token): string {
    const value = debugStringValue(this.extractText(token));
    const { kind } = token;
    switch (kind) {
      case Kind.EOF:
        return 'end of file';
      case Kind.Number:
        return `number '${value}'`;
      case Kind.Name:
        return `identifier '${value}'`;
      case Kind.String:
        return `string ${value}`;
      case Kind.HTML:
        return `HTML string ${value}`;
      case Kind.UnexpectedChar:
        return `character '${value}'`;
      case Kind.UnterminatedString:
        return `unterminated string '${value}'`;
      case Kind.UnterminatedHTML:
        return `unterminated HTML string '${value}'`;
      case Kind.UnterminatedBlockComment:
        return `unterminated block comment '${value}'`;
    }
    return kindStr(kind);
  }

  nextToken(): Token {
    const invalidToken = this.#skipUntilTokenStart();
    if (invalidToken) {
      return invalidToken;
    }
    const start: Location = {
      index: this.#nextIndex,
      line: this.#line,
      column: this.#nextIndex - this.#lineStart,
    };

    const char = this.#peekChar();
    switch (char) {
      case undefined:
        return { kind: Kind.EOF, start, length: 0 };
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
    const sawPeriod = this.#skipChar(Char['.']);
    if (!isDigit(this.#peekChar())) {
      this.#nextIndex = start.index + 1;
      return { kind: Kind.UnexpectedChar, start, length: 1 };
    }
    while (isDigit(this.#peekChar())) {
      ++this.#nextIndex;
    }
    if (!sawPeriod) {
      this.#skipChar(Char['.']);
      while (isDigit(this.#peekChar())) {
        ++this.#nextIndex;
      }
    }

    const length = this.#nextIndex - start.index;
    return { kind: Kind.Number, start, length };
  }

  #readHTML(start: Location): Token {
    let unclosedAngleBrackets = 0;
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
          ++unclosedAngleBrackets;
          break;
        case Char['>']:
          ++this.#nextIndex;
          --unclosedAngleBrackets;
          break;
        default:
          ++this.#nextIndex;
          break;
      }
    } while (unclosedAngleBrackets !== 0);

    const length = this.#nextIndex - start.index;
    return { kind: Kind.HTML, start, length };
  }

  #readString(start: Location): Token {
    let escapedChar = false;
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
          escapedChar = false;
          break;
        case Char['\\']:
          ++this.#nextIndex;
          escapedChar = !escapedChar;
          continue;
        case Char['"']:
          ++this.#nextIndex;
          if (!escapedChar) {
            const length = this.#nextIndex - start.index;
            return { kind: Kind.String, start, length };
          }
          escapedChar = false;
          break;
        default:
          ++this.#nextIndex;
          escapedChar = false;
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

function kindStr(kind: LiteralKind | KeywordKind): string {
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

function debugStringValue(value: string) {
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
  status: 'success';
  output: NormalizedGraph;
  errors: RenderError[];
}

export type ParseResult = ParseSuccessResult | FailureResult;

interface PortID {
  port: string;
  portToken: Token;
}

const validCompassPoints = new Set([
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

type NodeID = [NormalizedNode, PortID | null];

class Parser {
  static #AbortError = new Error(
    'Internal error, thrown when parsing of dot file fails',
  );

  static parseDot(
    dotStr: string,
    fixedAttributes: FixedAttributes,
  ): ParseResult {
    const parser = new Parser(dotStr);
    try {
      const graph = parser.#parseGraph(fixedAttributes);
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

  #lexer: Lexer;
  #peekToken: Token;
  #diagnostics: RenderError[] = [];

  constructor(dotStr: string) {
    this.#lexer = new Lexer(dotStr);
    this.#peekToken = this.#lexer.nextToken();
  }

  #isEOF(): boolean {
    return this.#peekToken.kind === Kind.EOF;
  }

  #peekIs(flags: number): boolean {
    return (this.#peekToken.kind & flags) != 0;
  }

  #peekKind(): Kind {
    return this.#peekToken.kind;
  }

  #readToken(): Token {
    const result = this.#peekToken;
    if (result.kind === Kind.UnterminatedBlockComment) {
      const tokenDebug = this.#lexer.tokenToDebug(result);
      this.#failWithError(
        `Unexpected ${tokenDebug}, add a closing '*/' to the comment.`,
        result,
      );
    }

    this.#peekToken = this.#lexer.nextToken();
    this.#warnIfAmbiguous(result, this.#peekToken);
    return result;
  }

  #warnIfAmbiguous(lastToken: Token, nextToken: Token): void {
    const lastEnd = lastToken.start.index + lastToken.length;
    const nextStart = nextToken.start.index;
    if (lastEnd !== nextStart) {
      return;
    }

    const lastKind = lastToken.kind;
    const lastCanClash =
      lastKind & KEYWORD || lastKind === Kind.Name || lastKind == Kind.Number;
    if (!lastCanClash) {
      return;
    }

    const nextKind = nextToken.kind;
    const nextCanClash =
      nextKind & KEYWORD || nextKind === Kind.Name || nextKind == Kind.Number;
    if (!nextCanClash) {
      return;
    }

    const lastTokenText = this.#lexer.tokenToDebug(lastToken);
    const nextTokenText = this.#lexer.tokenToDebug(nextToken);
    const ambiguousText: string = debugStringValue(
      this.#lexer.extractText(lastToken) + this.#lexer.extractText(nextToken),
    );
    this.#diagnostics.push({
      level: 'warning',
      message: `Ambiguous token sequence: '${ambiguousText}' will be split into ${lastTokenText} and ${nextTokenText}. If you want it interpreted as a single value, use quotes: "...". Otherwise, use whitespace or other delimiters to separate tokens.`,
      location: lastToken.start,
    });
  }

  #optional(kind: Kind): boolean {
    if (this.#peekToken.kind === kind) {
      this.#peekToken = this.#lexer.nextToken();
      return true;
    }
    return false;
  }

  #expected(kind: LiteralKind | KeywordKind): void {
    if (!this.#optional(kind)) {
      const token = this.#readToken();
      const tokenDebug = this.#lexer.tokenToDebug(token);
      this.#failWithError(
        `Unexpected ${tokenDebug}, expected ${kindStr(kind)}.`,
        token,
      );
    }
  }

  #optionalName(description: string): string | null {
    if (this.#peekIs(ID | KEYWORD)) {
      return this.#expectedName(description);
    }
    return null;
  }

  #expectedName(description: string): string {
    return this.#parseName(this.#readToken(), description);
  }

  #expectedValue(description: string): string | { html: string } {
    return this.#parseValue(this.#readToken(), description);
  }

  #parseName(token: Token, description: string): string {
    const name = this.#parseValue(token, description);
    /* v8 ignore start -- FIXME: it's weird edge case, so in future we should forbid using HTML as names */
    if (typeof name === 'object') {
      this.#failWithError(`HTML as ${description} is not supported`, token);
    }
    /* v8 ignore end */
    return name;
  }

  #parseValue(token: Token, description: string): string | { html: string } {
    if (token.kind & KEYWORD) {
      const keyword = this.#lexer.extractText(token);
      this.#failWithError(
        `Unexpected reserved keyword '${keyword}' where ${description} was expected. If you want to use it as an identifier, enclose it in quotes: "${keyword}".`,
        token,
      );
    }

    const text = this.#lexer.extractText(token);
    switch (token.kind) {
      case Kind.Name:
      case Kind.Number:
        return text;
      case Kind.String:
        return text
          .slice(1, -1)
          .replaceAll(String.raw`\"`, '"')
          .replaceAll('\\\r\n', '')
          .replaceAll('\\\r', '')
          .replaceAll('\\\n', '');
      case Kind.HTML:
        return { html: text.slice(1, -1) };
    }

    const tokenDebug = this.#lexer.tokenToDebug(token);
    switch (token.kind) {
      case Kind.UnterminatedString:
        return this.#failWithError(
          `${capitalize(tokenDebug)}, add a closing '"' to the string.`,
          token,
        );
      case Kind.UnterminatedHTML:
        return this.#failWithError(
          `${capitalize(tokenDebug)}, add a closing '>' to the HTML string.`,
          token,
        );
      default:
        this.#failWithError(
          `Unexpected ${tokenDebug}, expected ${description}. If this is meant to be part of a label or name, enclose it in quotes ("...").`,
          token,
        );
    }
  }

  #failWithError(message: string, token: Token): never {
    this.#diagnostics.push({
      level: 'error',
      message,
      location: token.start,
    });
    throw Parser.#AbortError;
  }

  #parseGraph(fixedAttributes: FixedAttributes): NormalizedGraph {
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
      const tokenDebug = this.#lexer.tokenToDebug(token);
      this.#failWithError(
        `Unexpected ${tokenDebug}, expected keyword ` +
          (strict
            ? `'graph' or 'digraph' after 'strict'.`
            : `'strict', 'graph' or 'digraph' at the beginning of the file.`),
        token,
      );
    }

    const name = this.#optionalName('graph name');
    const graph = new NormalizedGraph(
      { strict, directed, name },
      fixedAttributes,
    );
    // FIXME: check if it's viz.js hack or it also present in graphviz
    graph.mergeNodeAttributes({ label: String.raw`\N` });

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
      const token = this.#readToken();
      if (this.#optional(Kind['='])) {
        // ID '=' ID
        const name = this.#parseName(token, 'attribute name');
        const attributes: Attributes = {
          [name]: this.#expectedValue('attribute value'),
        };
        owner.mergeGraphAttributes(attributes);
        return;
      }

      const [node] = owner.upsertNode(this.#parseName(token, 'node name'));
      const nodeIDs: NodeID[] = [[node, this.#optionalNodePort()]];
      while (this.#optional(Kind[','])) {
        nodeIDs.push(this.#parseNodeID(owner));
      }

      if (this.#optionalEdgeOp(owner)) {
        this.#parseEdges(nodeIDs, owner);
        return;
      }

      // node_stmt: node_id [ attr_list ]
      const attributes = this.#optionalAttrList();
      for (const [node, port] of nodeIDs) {
        if (port) {
          this.#failWithError(
            `Unexpected '${port.port}' port in node statement`,
            port.portToken,
          );
        }
        if (attributes) {
          node.mergeAttributes(attributes);
        }
      }
      return;
    }

    switch (this.#peekKind()) {
      case Kind['{']: {
        const tailNodes = this.#parseSubgraph(owner, null);
        if (this.#optionalEdgeOp(owner)) {
          this.#parseEdges(tailNodes, owner);
        }
        break;
      }
      case Kind.subgraph: {
        const tailNodes = this.#parseNamedSubgraph(owner);
        if (this.#optionalEdgeOp(owner)) {
          this.#parseEdges(tailNodes, owner);
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
        const tokenDebug = this.#lexer.tokenToDebug(token);
        this.#failWithError(
          `Unexpected ${tokenDebug}, expected node, edge, subgraph or attribute statement. If this is meant to be part of a label or name, enclose it in quotes ("...").`,
          token,
        );
      }
    }
  }

  #parseNodeID(owner: NormalizedGraph | NormalizedSubgraph): NodeID {
    // node_id:	ID [ port ]
    const name = this.#expectedName('node name');
    const [node] = owner.upsertNode(name);
    return [node, this.#optionalNodePort()];
  }

  #optionalNodePort(): PortID | null {
    // port: ':' ID [ ':' compass_pt ]
    if (!this.#optional(Kind[':'])) {
      return null;
    }

    const portToken = this.#readToken();
    let port = this.#parseName(portToken, 'port name');

    // compass_pt: n | ne | e | se | s | sw | w | nw | c | _
    if (this.#optional(Kind[':'])) {
      const compassToken = this.#readToken();
      const compass = this.#parseName(compassToken, 'compass point value');

      if (!validCompassPoints.has(compass)) {
        const debugToken = this.#lexer.tokenToDebug(compassToken);
        const allowedValues = [...validCompassPoints.values()].join(', ');
        this.#failWithError(
          `Invalid compass point ${debugToken}. Allowed values: ${allowedValues}.`,
          compassToken,
        );
      }
      port += ':' + compass;
    }
    return { port, portToken };
  }

  #optionalAttrList(): Attributes | null {
    return this.#peekKind() === Kind['['] ? this.#parseAttrList() : null;
  }

  #parseAttrList(): Attributes {
    const attributes: Attributes = {};

    // attr_list:	'[' [ a_list ] ']' [ attr_list ]
    do {
      this.#expected(Kind['[']);
      // a_list: ID '=' ID [ (';' | ',') ] [ a_list ]
      while (!this.#optional(Kind[']'])) {
        const name = this.#expectedName('attribute name');
        this.#expected(Kind['=']);
        attributes[name] = this.#expectedValue('attribute value');

        // Either ';' or ',' but doesn't allow both
        if (!this.#optional(Kind[';'])) {
          this.#optional(Kind[',']);
        }
      }
    } while (this.#peekKind() === Kind['[']);

    return attributes;
  }

  #parseNamedSubgraph(owner: NormalizedGraph | NormalizedSubgraph): NodeID[] {
    this.#expected(Kind.subgraph);
    const name = this.#optionalName('subgraph name');
    return this.#parseSubgraph(owner, name);
  }

  #parseSubgraph(
    owner: NormalizedGraph | NormalizedSubgraph,
    name: string | null,
  ): NodeID[] {
    const subgraph = owner.upsertSubgraph(name);
    this.#parseStatementList(subgraph);
    return subgraph.sortedNodes().map((node) => [node, null]);
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
    tailNodes: NodeID[],
    owner: NormalizedGraph | NormalizedSubgraph,
  ) {
    const newEdges: [NodeID, NodeID][] = [];
    do {
      let headNodes: NodeID[];
      switch (this.#peekKind()) {
        case Kind['{']:
          headNodes = this.#parseSubgraph(owner, null);
          break;
        case Kind.subgraph:
          headNodes = this.#parseNamedSubgraph(owner);
          break;
        default:
          headNodes = [];
          do {
            headNodes.push(this.#parseNodeID(owner));
          } while (this.#optional(Kind[',']));
      }

      for (const tail of tailNodes) {
        for (const head of headNodes) {
          newEdges.push([tail, head]);
        }
      }
      tailNodes = headNodes;
    } while (this.#optionalEdgeOp(owner));

    let key: string | null = null;
    let attributes = this.#optionalAttrList();
    if (attributes) {
      [key, attributes] = extractKeyFromEdgeAttributes(attributes);
    }
    for (const [tailID, headID] of newEdges) {
      const [tail, tailPort] = tailID;
      const [head, headPort] = headID;
      const [edge] = owner.upsertEdge({ tail, head, key });
      edge.mergeAttributes({
        headport: headPort?.port,
        tailport: tailPort?.port,
        ...attributes,
      });
    }
  }
}

export const parseDot = Parser.parseDot;
