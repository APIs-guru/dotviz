import { describe, expect, it } from 'vitest';

import * as VizPackage from '../src/index.ts';
import { dedent } from './util/dedent.ts';
import {
  expectDot,
  expectDotWithWarnings,
  expectFailureResult,
  stringifyDiagnostics,
} from './util/render-result.ts';

describe('Viz', () => {
  describe('render', () => {
    it('renders valid input with a single graph', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderDot('graph a { }');

      expectDot(result).toMatchInlineSnapshot(`
        graph a {
        	graph [bb="0,0,0,0"];
        	node [label="\\N"];
        }
      `);
    });

    it('renders valid input with multiple graphs', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderDot('graph a { } graph b { }');

      expectDotWithWarnings(result).toMatchInlineSnapshot(`
        RenderingBackendWarning: Multiple graphs found. Using the first one.

        graph a {
        	graph [bb="0,0,0,0"];
        	node [label="\\N"];
        }
      `);
    });

    it('each call renders the first graph in input', async () => {
      const viz = await VizPackage.instance();
      expect(
        viz.renderDot('graph a { } graph b { } graph c { }').output?.dot,
      ).toMatch(/graph a {/);
      expect(viz.renderDot('graph d { } graph e { }').output?.dot).toMatch(
        /graph d {/,
      );
      expect(viz.renderDot('graph f { }').output?.dot).toMatch(/graph f {/);
    });

    it('accepts the format option, defaulting to dot', async () => {
      const viz = await VizPackage.instance();

      expect(viz.renderDot('digraph { a -> b }')).toStrictEqual({
        status: 'success',
        diagnostics: [],
        output: {
          dot: expect.stringMatching(/pos="/) as unknown,
          svg: undefined,
        },
      });

      expect(
        viz.renderDot('digraph { a -> b }', { formats: ['dot'] }),
      ).toStrictEqual({
        status: 'success',
        diagnostics: [],
        output: {
          dot: expect.stringMatching(/pos="/) as unknown,
          svg: undefined,
        },
      });

      expect(
        viz.renderDot('digraph { a -> b }', { formats: ['svg'] }),
      ).toStrictEqual({
        status: 'success',
        diagnostics: [],
        output: {
          dot: undefined,
          svg: expect.stringMatching(/<svg/) as unknown,
        },
      });
    });

    it('accepts yInvert option', async () => {
      const viz = await VizPackage.instance();
      const result1 = viz.renderDot('graph { a }', { yInvert: false });
      const result2 = viz.renderDot('graph { a }', { yInvert: true });

      expectDot(result1).toMatchInlineSnapshot(`
        graph {
        	graph [bb="0,0,54,36"];
        	node [label="\\N"];
        	a	[height=0.5,
        		pos="27,18",
        		width=0.75];
        }
      `);

      expectDot(result2).toMatchInlineSnapshot(`
        graph {
        	graph [bb="0,36,54,0"];
        	node [label="\\N"];
        	a	[height=0.5,
        		pos="27,18",
        		width=0.75];
        }
      `);
    });

    it('accepts default attributes', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderDot('graph {}', {
        overrideAttributes: {
          graphAttributes: { a: 123 },
          nodeAttributes: { b: false },
          edgeAttributes: { c: 'test' },
        },
      });

      expectDot(result).toMatchInlineSnapshot(`
        graph {
        	graph [a=123,
        		bb="0,0,0,0"
        	];
        	node [b=false,
        		label="\\N"
        	];
        	edge [c=test];
        }
      `);
    });

    it('default attribute values can be html strings', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderDot('graph {}', {
        overrideAttributes: {
          nodeAttributes: {
            label: { html: '<b>test</b>' },
          },
        },
      });

      expectDot(result).toMatchInlineSnapshot(`
        graph {
        	graph [bb="0,0,0,0"];
        	node [label=<<b>test</b>>];
        }
      `);
    });

    it('returns an error for empty input', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderDot('');

      expectFailureResult(result).toMatchInlineSnapshot(
        `RenderingBackendError: Missing graph definition. Start your file with 'graph {}' or 'digraph {}'.`,
      );
    });

    it('returns error messages for invalid input', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderDot('invalid');

      expectFailureResult(result).toMatchInlineSnapshot(`
        ParserError: Unexpected identifier 'invalid', expected keyword 'strict', 'graph' or 'digraph' at the beginning of the file.

        1 | invalid
          | ^
      `);
    });

    it('returns error messages for invalid options', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderDot('digraph {a -> b}', {
        // @ts-expect-error invalid value for test
        yInvert: 'bad value',
      });

      expectFailureResult(result).toMatchInlineSnapshot(
        `RenderingBackendError: JSON error UnexpectedToken at 1:377: \`[]},"engine":"dot","yInvert":"bad value","reduce":false,"images":{},"renderSvg":\``,
      );
    });

    it('returns only the error messages emitted for the current call', async () => {
      const viz = await VizPackage.instance();
      const result1 = viz.renderDot('invalid1');
      const result2 = viz.renderDot('invalid2');

      expectFailureResult(result1).toMatchInlineSnapshot(`
        ParserError: Unexpected identifier 'invalid1', expected keyword 'strict', 'graph' or 'digraph' at the beginning of the file.

        1 | invalid1
          | ^
      `);

      expectFailureResult(result2).toMatchInlineSnapshot(`
        ParserError: Unexpected identifier 'invalid2', expected keyword 'strict', 'graph' or 'digraph' at the beginning of the file.

        1 | invalid2
          | ^
      `);
    });

    it('renders valid input and includes error messages when followed by a graph with a syntax error', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderDot('graph a { } graph {');

      expectDotWithWarnings(result).toMatchInlineSnapshot(`
        ParserError: Unexpected end of file. Add a closing '}' to match the opening '{' of the graph or subgraph.

        1 | graph a { } graph {
          |                    ^

        RenderingBackendWarning: Multiple graphs found. Using the first one.

        graph a {
        	graph [bb="0,0,0,0"];
        	node [label="\\N"];
        }
      `);
    });

    it('returns error messages for layout errors', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderDot(
        'graph a { layout=invalid } graph b { layout=dot }',
      );

      expectFailureResult(result).toMatchInlineSnapshot(`
        RenderingBackendWarning: Multiple graphs found. Using the first one.

        RenderingBackendError: Layout type: "invalid" not recognized. Use one of: dot circo neato fdp twopi patchwork osage sfdp
      `);
    });

    it('returns error message for conflicting layout', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderDot('graph a { layout=dot }', {
        engine: 'circo',
      });

      expectFailureResult(result).toMatchInlineSnapshot(
        `RenderingBackendError: Engine mismatch: layout attribute in graph ("dot") conflicts with engine option ("circo"). Remove one or make them match.`,
      );
    });

    it('returns error for non-utf8 charset', async () => {
      const viz = await VizPackage.instance();
      const resultLatin = viz.renderDot('graph a { charset=latin1 }');

      expectFailureResult(resultLatin).toMatchInlineSnapshot(
        `RenderingBackendError: Unsupported charset: "latin1". Only 'utf-8' and 'utf8' are supported.`,
      );

      const resultHTML = viz.renderDot('graph a { charset=<utf8> }');

      expectFailureResult(resultHTML).toMatchInlineSnapshot(
        `RenderingBackendError: Unsupported charset: <utf8>. Only 'utf-8' and 'utf8' are supported.`,
      );
    });

    it('renders graphs with syntax warnings', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderDot('graph a { x=1.2.3=y } graph b { }');

      expectDotWithWarnings(result).toMatchInlineSnapshot(`
        ParserWarning: Ambiguous token sequence: '1.2.3' will be split into number '1.2' and number '.3'. If you want it interpreted as a single value, use quotes: "...". Otherwise, use whitespace or other delimiters to separate tokens.

        1 | graph a { x=1.2.3=y } graph b { }
          |             ^

        RenderingBackendWarning: Multiple graphs found. Using the first one.

        graph a {
        	graph [.3=y,
        		bb="0,0,0,0",
        		x=1.2
        	];
        	node [label="\\N"];
        }
      `);
    });

    it('returns both warnings and errors', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderDot('graph { layout=invalid; x=1.2.3=y }');

      expectFailureResult(result).toMatchInlineSnapshot(`
        ParserWarning: Ambiguous token sequence: '1.2.3' will be split into number '1.2' and number '.3'. If you want it interpreted as a single value, use quotes: "...". Otherwise, use whitespace or other delimiters to separate tokens.

        1 | graph { layout=invalid; x=1.2.3=y }
          |                           ^

        RenderingBackendError: Layout type: "invalid" not recognized. Use one of: dot circo neato fdp twopi patchwork osage sfdp
      `);
    });

    it('returns error messages printed to stderr', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderDot('graph { a [label=図] }');

      expectDotWithWarnings(result).toMatchInlineSnapshot(`
        RenderingBackendWarning: Warning: no value for width of non-ASCII character 229. Falling back to width of space character

        graph {
        	graph [bb="0,0,54,36"];
        	node [label="\\N"];
        	a	[height=0.5,
        		label=図,
        		pos="27,18",
        		width=0.75];
        }
      `);
    });

    it('returns an error that uses AGPREV with the correct level', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderDot('graph { _background=123 }');

      expectDotWithWarnings(result).toMatchInlineSnapshot(`
        RenderingBackendWarning: Could not parse "_background" attribute in graph %1

        RenderingBackendWarning:   "123"

        graph {
        	graph [_background=123,
        		bb="0,0,0,0"
        	];
        	node [label="\\N"];
        }
      `);
    });

    it('the graph is read with the default node label set', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderDot('graph { a; b[label=test] }');

      expectDot(result).toMatchInlineSnapshot(`
        graph {
        	graph [bb="0,0,126,36"];
        	node [label="\\N"];
        	a	[height=0.5,
        		pos="27,18",
        		width=0.75];
        	b	[height=0.5,
        		label=test,
        		pos="99,18",
        		width=0.75];
        }
      `);
    });

    it('accepts an images option', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderDot('graph { a[image="test.png"] }', {
        images: { 'test.png': { width: 300, height: 200 } },
      });

      expectDot(result).toMatchInlineSnapshot(`
        graph {
        	graph [bb="0,0,321.03,214.96"];
        	node [label="\\N"];
        	a	[height=2.9856,
        		image="test.png",
        		pos="160.51,107.48",
        		width=4.4587];
        }
      `);
    });

    it('the same image can be used twice', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderDot(
        'graph { a[image="test.png"]; b[image="test.png"] }',
        { images: { 'test.png': { width: 300, height: 200 } } },
      );

      expectDot(result).toMatchInlineSnapshot(`
        graph {
        	graph [bb="0,0,660.03,214.96"];
        	node [label="\\N"];
        	a	[height=2.9856,
        		image="test.png",
        		pos="160.51,107.48",
        		width=4.4587];
        	b	[height=2.9856,
        		image="test.png",
        		pos="499.51,107.48",
        		width=4.4587];
        }
      `);
    });

    describe('split long lines based on `linelength` attribute', () => {
      it.for([
        ['0', 'a '.repeat(1000) + 'b', 'a '.repeat(1000) + 'b'],
        ['60', 'a '.repeat(30) + 'b', 'a '.repeat(30) + '\\\nb'],
        ['80', 'a '.repeat(40) + 'b', 'a '.repeat(40) + '\\\nb'],
        ['128', 'a '.repeat(64) + 'b', 'a '.repeat(64) + '\\\nb'],
        // "" resets to default (128)
        ['""', 'a '.repeat(64) + 'b', 'a '.repeat(64) + '\\\nb'],
      ])('linelength=$0', async ([linelength, input, output]) => {
        const viz = await VizPackage.instance();
        const result = viz.renderDot(
          `graph { linelength=${linelength}; test="${input}"}`,
        );
        expectDot(result);
        expect(result.output?.dot?.trimEnd()).toStrictEqual(dedent`
          graph {
          \tgraph [bb="0,0,0,0",
          \t\tlinelength=${linelength},
          \t\ttest="${output}"
          \t];
          \tnode [label="\\N"];
          }
        `);
      });
    });

    describe('returns an error for invalid `linelength` values', () => {
      it.for(['-1', '59', '129', '1.5', 'abc', '<0>'])('$0', async (value) => {
        const viz = await VizPackage.instance();
        const result = viz.renderDot(`graph { linelength=${value} }`);

        expect(result.status).toBe('failure');
        expect(stringifyDiagnostics(result.diagnostics)).toBe(
          "RenderingBackendError: linelength must be '0' or an integer number in [60, 128] range",
        );
      });
    });

    it('accepts URLs for image names', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderDot(
        'graph { a[image="http://example.com/test.png"] }',
        {
          images: {
            'http://example.com/test.png': { width: 300, height: 200 },
          },
        },
      );

      expectDot(result).toMatchInlineSnapshot(`
        graph {
        	graph [bb="0,0,321.03,214.96"];
        	node [label="\\N"];
        	a	[height=2.9856,
        		image="http://example.com/test.png",
        		pos="160.51,107.48",
        		width=4.4587];
        }
      `);
    });
  });
});
