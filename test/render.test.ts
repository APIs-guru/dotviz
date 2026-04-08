import { describe, expect, it } from 'vitest';

import * as VizPackage from '../src/index.ts';
import { expectString } from './util/raw-string-serializer.ts';
import {
  expectFailureResult,
  expectSuccessResult,
} from './util/render-result.ts';

describe('Viz', () => {
  describe('render', () => {
    it('renders valid input with a single graph', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph a { }');

      expectSuccessResult(result).toMatchInlineSnapshot(`
        graph a {
        	graph [bb="0,0,0,0"];
        	node [label="\\N"];
        }
      `);
    });

    it('renders valid input with multiple graphs', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph a { } graph b { }');

      expectSuccessResult(result).toMatchInlineSnapshot(`
        graph a {
        	graph [bb="0,0,0,0"];
        	node [label="\\N"];
        }
      `);
    });

    it('each call renders the first graph in input', async () => {
      const viz = await VizPackage.instance();
      expect(viz.render('graph a { } graph b { } graph c { }').output).toMatch(
        /graph a {/,
      );
      expect(viz.render('graph d { } graph e { }').output).toMatch(/graph d {/);
      expect(viz.render('graph f { }').output).toMatch(/graph f {/);
    });

    it('accepts the format option, defaulting to dot', async () => {
      const viz = await VizPackage.instance();
      expect(viz.render('digraph { a -> b }').output).toMatch(/pos="/);
      expect(
        viz.render('digraph { a -> b }', { format: 'dot' }).output,
      ).toMatch(/pos="/);
      expect(viz.render('digraph { a -> b }', { format: 'gv' }).output).toMatch(
        /pos="/,
      );
      expect(
        viz.render('digraph { a -> b }', { format: 'svg' }).output,
      ).toMatch(/<svg/);
    });

    it('accepts yInvert option', async () => {
      const viz = await VizPackage.instance();
      const result1 = viz.render('graph { a }', { yInvert: false });
      const result2 = viz.render('graph { a }', { yInvert: true });

      expectSuccessResult(result1).toMatchInlineSnapshot(`
        graph {
        	graph [bb="0,0,54,36"];
        	node [label="\\N"];
        	a	[height=0.5,
        		pos="27,18",
        		width=0.75];
        }
      `);

      expectSuccessResult(result2).toMatchInlineSnapshot(`
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
      const result = viz.render('graph {}', {
        graphAttributes: {
          a: 123,
        },
        nodeAttributes: {
          b: false,
        },
        edgeAttributes: {
          c: 'test',
        },
      });

      expectSuccessResult(result).toMatchInlineSnapshot(`
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
      const result = viz.render('graph {}', {
        nodeAttributes: {
          label: { html: '<b>test</b>' },
        },
      });

      expectSuccessResult(result).toMatchInlineSnapshot(`
        graph {
        	graph [bb="0,0,0,0"];
        	node [label=<<b>test</b>>];
        }
      `);
    });

    it('returns an error for empty input', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('');

      expectFailureResult(result).toMatchInlineSnapshot(`[]`);
    });

    it('returns error messages for invalid input', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('invalid');

      expectFailureResult(result).toMatchInlineSnapshot(`
        [
          {
            "level": "error",
            "message": "syntax error in line 1 near 'invalid'",
          },
        ]
      `);
    });

    it('returns error messages for invalid options', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('digraph {a -> b}', {
        // @ts-expect-error invalid value for test
        yInvert: 'bad value',
      });

      expectFailureResult(result).toMatchInlineSnapshot(`
        [
          {
            "level": "error",
            "message": "JSON error UnexpectedToken at 11:25: \`
          "yInvert": "bad value",\`",
          },
        ]
      `);
    });

    it('returns only the error messages emitted for the current call', async () => {
      const viz = await VizPackage.instance();
      const result1 = viz.render('invalid1');
      const result2 = viz.render('invalid2');

      expectFailureResult(result1).toMatchInlineSnapshot(`
        [
          {
            "level": "error",
            "message": "syntax error in line 1 near 'invalid1'",
          },
        ]
      `);

      expectFailureResult(result2).toMatchInlineSnapshot(`
        [
          {
            "level": "error",
            "message": "syntax error in line 1 near 'invalid2'",
          },
        ]
      `);
    });

    it('renders valid input and does not include error messages when followed by a graph with a syntax error', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph a { } graph {');

      expectSuccessResult(result).toMatchInlineSnapshot(`
        graph a {
        	graph [bb="0,0,0,0"];
        	node [label="\\N"];
        }
      `);
    });

    it('returns error messages for layout errors', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render(
        'graph a { layout=invalid } graph b { layout=dot }',
      );

      expectFailureResult(result).toMatchInlineSnapshot(`
        [
          {
            "level": "error",
            "message": "Layout type: "invalid" not recognized. Use one of: dot circo neato fdp twopi",
          },
        ]
      `);
    });

    it('renders graphs with syntax warnings', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph a { x=1.2.3=y } graph b { }');

      expect(result).toStrictEqual({
        status: 'success',
        output: expect.any(String) as unknown,
        errors: expect.any(Array) as unknown,
      });
      expectString(result.output).toMatchInlineSnapshot(`
        graph a {
        	graph [.3=y,
        		bb="0,0,0,0",
        		x=1.2
        	];
        	node [label="\\N"];
        }
      `);
      expect(result.errors).toMatchInlineSnapshot(`
        [
          {
            "level": "warning",
            "message": "syntax ambiguity - badly delimited number '1.2.' in line 1 of input splits into two tokens",
          },
        ]
      `);
    });

    it('returns both warnings and errors', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph { layout=invalid; x=1.2.3=y }');

      expectFailureResult(result).toMatchInlineSnapshot(`
        [
          {
            "level": "warning",
            "message": "syntax ambiguity - badly delimited number '1.2.' in line 1 of input splits into two tokens",
          },
          {
            "level": "error",
            "message": "Layout type: "invalid" not recognized. Use one of: dot circo neato fdp twopi",
          },
        ]
      `);
    });

    it('returns error messages printed to stderr', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph { a [label=図] }', { format: 'dot' });

      expect(result).toStrictEqual({
        status: 'success',
        output: expect.any(String) as unknown,
        errors: expect.any(Array) as unknown,
      });
      expectString(result.output).toMatchInlineSnapshot(`
        graph {
        	graph [bb="0,0,54,36"];
        	node [label="\\N"];
        	a	[height=0.5,
        		label=図,
        		pos="27,18",
        		width=0.75];
        }
      `);
      expect(result.errors).toMatchInlineSnapshot(`
        [
          {
            "level": "warning",
            "message": "Warning: no value for width of non-ASCII character 229. Falling back to width of space character",
          },
        ]
      `);
    });

    it('returns error messages for invalid engine option', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph { }', { engine: 'invalid' });

      expectFailureResult(result).toMatchInlineSnapshot(`
        [
          {
            "level": "error",
            "message": "Layout type: "invalid" not recognized. Use one of: dot circo neato fdp twopi",
          },
        ]
      `);
    });

    it('returns error messages for invalid format option', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph { }', { format: 'invalid' });

      expectFailureResult(result).toMatchInlineSnapshot(`
        [
          {
            "level": "error",
            "message": "Format: "invalid" not recognized. Use one of: dot gv svg",
          },
        ]
      `);
    });

    it('returns an error that contains newlines as a single item', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph { " }');

      expectFailureResult(result).toMatchInlineSnapshot(`
        [
          {
            "level": "error",
            "message": "syntax error in line 1 scanning a quoted string (missing endquote? longer than 16384?)
        String starting:" }",
          },
        ]
      `);
    });

    it('returns an error that uses AGPREV with the correct level', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph { _background=123 }');

      expect(result).toStrictEqual({
        status: 'success',
        output: expect.any(String) as unknown,
        errors: expect.any(Array) as unknown,
      });
      expectString(result.output).toMatchInlineSnapshot(`
        graph {
        	graph [_background=123,
        		bb="0,0,0,0"
        	];
        	node [label="\\N"];
        }
      `);
      expect(result.errors).toMatchInlineSnapshot(`
        [
          {
            "level": "warning",
            "message": "Could not parse "_background" attribute in graph %1",
          },
          {
            "level": "warning",
            "message": "  "123"",
          },
        ]
      `);
    });

    it('the graph is read with the default node label set', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph { a; b[label=test] }');

      expectSuccessResult(result).toMatchInlineSnapshot(`
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
      const result = viz.render('graph { a[image="test.png"] }', {
        images: { 'test.png': { width: 300, height: 200 } },
      });

      expectSuccessResult(result).toMatchInlineSnapshot(`
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
      const result = viz.render(
        'graph { a[image="test.png"]; b[image="test.png"] }',
        { images: { 'test.png': { width: 300, height: 200 } } },
      );

      expectSuccessResult(result).toMatchInlineSnapshot(`
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

    it('accepts URLs for image names', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render(
        'graph { a[image="http://example.com/test.png"] }',
        {
          images: {
            'http://example.com/test.png': { width: 300, height: 200 },
          },
        },
      );

      expectSuccessResult(result).toMatchInlineSnapshot(`
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
