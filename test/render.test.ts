import assert from 'node:assert/strict';
import { describe, it } from 'node:test';

import * as VizPackage from '../src/index.ts';

describe('Viz', () => {
  describe('render', () => {
    it('renders valid input with a single graph', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph a { }');

      assert.deepStrictEqual(result, {
        status: 'success',
        output:
          'graph a {\n\tgraph [bb="0,0,0,0"];\n\tnode [label="\\N"];\n}\n',
        errors: [],
      });
    });

    it('renders valid input with multiple graphs', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph a { } graph b { }');

      assert.deepStrictEqual(result, {
        status: 'success',
        output:
          'graph a {\n\tgraph [bb="0,0,0,0"];\n\tnode [label="\\N"];\n}\n',
        errors: [],
      });
    });

    it('each call renders the first graph in input', async () => {
      const viz = await VizPackage.instance();
      assert.match(
        viz.render('graph a { } graph b { } graph c { }').output ?? '',
        /graph a {/,
      );
      assert.match(
        viz.render('graph d { } graph e { }').output ?? '',
        /graph d {/,
      );
      assert.match(viz.render('graph f { }').output ?? '', /graph f {/);
    });

    it('accepts the format option, defaulting to dot', async () => {
      const viz = await VizPackage.instance();
      assert.match(viz.render('digraph { a -> b }').output ?? '', /pos="/);
      assert.match(
        viz.render('digraph { a -> b }', { format: 'dot' }).output ?? '',
        /pos="/,
      );
      assert.match(
        viz.render('digraph { a -> b }', { format: 'gv' }).output ?? '',
        /pos="/,
      );
      assert.match(
        viz.render('digraph { a -> b }', { format: 'svg' }).output ?? '',
        /<svg/,
      );
    });

    it('accepts yInvert option', async () => {
      const viz = await VizPackage.instance();
      const result1 = viz.render('graph { a }', { yInvert: false });
      const result2 = viz.render('graph { a }', { yInvert: true });

      assert.deepStrictEqual(result1, {
        status: 'success',
        output:
          'graph {\n\tgraph [bb="0,0,54,36"];\n\tnode [label="\\N"];\n\ta\t[height=0.5,\n\t\tpos="27,18",\n\t\twidth=0.75];\n}\n',
        errors: [],
      });

      assert.deepStrictEqual(result2, {
        status: 'success',
        output:
          'graph {\n\tgraph [bb="0,36,54,0"];\n\tnode [label="\\N"];\n\ta\t[height=0.5,\n\t\tpos="27,18",\n\t\twidth=0.75];\n}\n',
        errors: [],
      });
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

      assert.deepStrictEqual(result, {
        status: 'success',
        output: `graph {
	graph [a=123,
		bb="0,0,0,0"
	];
	node [b=false,
		label="\\N"
	];
	edge [c=test];
}
`,
        errors: [],
      });
    });

    it('default attribute values can be html strings', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph {}', {
        nodeAttributes: {
          label: { html: '<b>test</b>' },
        },
      });

      assert.deepStrictEqual(result, {
        status: 'success',
        output: `graph {
	graph [bb="0,0,0,0"];
	node [label=<<b>test</b>>];
}
`,
        errors: [],
      });
    });

    it('returns an error for empty input', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('');

      assert.deepStrictEqual(result, {
        status: 'failure',
        output: null,
        errors: [],
      });
    });

    it('returns error messages for invalid input', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('invalid');

      assert.deepStrictEqual(result, {
        status: 'failure',
        output: null,
        errors: [
          { level: 'error', message: "syntax error in line 1 near 'invalid'" },
        ],
      });
    });

    it('returns only the error messages emitted for the current call', async () => {
      const viz = await VizPackage.instance();
      const result1 = viz.render('invalid1');
      const result2 = viz.render('invalid2');

      assert.deepStrictEqual(result1, {
        status: 'failure',
        output: null,
        errors: [
          { level: 'error', message: "syntax error in line 1 near 'invalid1'" },
        ],
      });

      assert.deepStrictEqual(result2, {
        status: 'failure',
        output: null,
        errors: [
          { level: 'error', message: "syntax error in line 1 near 'invalid2'" },
        ],
      });
    });

    it('renders valid input and does not include error messages when followed by a graph with a syntax error', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph a { } graph {');

      assert.deepStrictEqual(result, {
        status: 'success',
        output:
          'graph a {\n\tgraph [bb="0,0,0,0"];\n\tnode [label="\\N"];\n}\n',
        errors: [],
      });
    });

    it('returns error messages for layout errors', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render(
        'graph a { layout=invalid } graph b { layout=dot }',
      );

      assert.deepStrictEqual(result, {
        status: 'failure',
        output: null,
        errors: [
          {
            level: 'error',
            message: 'Layout type: "invalid" not recognized. Use one of: dot',
          },
        ],
      });
    });

    it('renders graphs with syntax warnings', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph a { x=1.2.3=y } graph b { }');

      assert.deepStrictEqual(result, {
        status: 'success',
        output:
          'graph a {\n\tgraph [.3=y,\n\t\tbb="0,0,0,0",\n\t\tx=1.2\n\t];\n\tnode [label="\\N"];\n}\n',
        errors: [
          {
            level: 'warning',
            message:
              "syntax ambiguity - badly delimited number '1.2.' in line 1 of input splits into two tokens",
          },
        ],
      });
    });

    it('returns both warnings and errors', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph { layout=invalid; x=1.2.3=y }');

      assert.deepStrictEqual(result, {
        status: 'failure',
        output: null,
        errors: [
          {
            level: 'warning',
            message:
              "syntax ambiguity - badly delimited number '1.2.' in line 1 of input splits into two tokens",
          },
          {
            level: 'error',
            message: 'Layout type: "invalid" not recognized. Use one of: dot',
          },
        ],
      });
    });

    it('returns error messages printed to stderr', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph { a [label=図] }', { format: 'dot' });

      assert.deepStrictEqual(result, {
        status: 'success',
        output:
          'graph {\n' +
          '\tgraph [bb="0,0,54,36"];\n' +
          '\tnode [label="\\N"];\n' +
          '\ta\t[height=0.5,\n' +
          '\t\tlabel=図,\n' +
          '\t\tpos="27,18",\n' +
          '\t\twidth=0.75];\n' +
          '}\n',
        errors: [
          {
            level: 'warning',
            message:
              'Warning: no value for width of non-ASCII character 229. Falling back to width of space character',
          },
        ],
      });
    });

    it.skip('returns error messages for invalid engine option', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph { }', { engine: 'invalid' });

      assert.deepStrictEqual(result, {
        status: 'failure',
        output: null,
        errors: [
          {
            level: 'error',
            message: 'Layout type: "invalid" not recognized. Use one of: dot',
          },
        ],
      });
    });

    it('returns error messages for invalid format option', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph { }', { format: 'invalid' });

      assert.deepStrictEqual(result, {
        status: 'failure',
        output: null,
        errors: [
          {
            level: 'error',
            message: 'Format: "invalid" not recognized. Use one of: dot gv svg',
          },
        ],
      });
    });

    it('returns an error that contains newlines as a single item', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph { " }');

      assert.deepStrictEqual(result, {
        status: 'failure',
        output: null,
        errors: [
          {
            level: 'error',
            message:
              /* cspell:disable-next-line */
              'syntax error in line 1 scanning a quoted string (missing endquote? longer than 16384?)\nString starting:" }',
          },
        ],
      });
    });

    it('returns an error that uses AGPREV with the correct level', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph { _background=123 }');

      assert.deepStrictEqual(result, {
        status: 'success',
        output:
          'graph {\n\tgraph [_background=123,\n\t\tbb="0,0,0,0"\n\t];\n\tnode [label="\\N"];\n}\n',
        errors: [
          {
            level: 'warning',
            message: 'Could not parse "_background" attribute in graph %1',
          },
          { level: 'warning', message: '  "123"' },
        ],
      });
    });

    it('the graph is read with the default node label set', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph { a; b[label=test] }');

      assert.deepStrictEqual(result, {
        status: 'success',
        output: `graph {
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
`,
        errors: [],
      });
    });

    it.skip('accepts an images option', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render('graph { a[image="test.png"] }', {
        images: { 'test.png': { width: 300, height: 200 } },
      });

      assert.deepStrictEqual(result, {
        status: 'success',
        output: `graph {
	graph [bb="0,0,321.03,214.96"];
	node [label="\\N"];
	a	[height=2.9856,
		image="test.png",
		pos="160.51,107.48",
		width=4.4587];
}
`,
        errors: [],
      });
    });

    it.skip('the same image can be used twice', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render(
        'graph { a[image="test.png"]; b[image="test.png"] }',
        { images: { 'test.png': { width: 300, height: 200 } } },
      );

      assert.deepStrictEqual(result, {
        status: 'success',
        output: `graph {
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
`,
        errors: [],
      });
    });

    it.skip('accepts URLs for image names', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render(
        'graph { a[image="http://example.com/test.png"] }',
        {
          images: {
            'http://example.com/test.png': { width: 300, height: 200 },
          },
        },
      );

      assert.deepStrictEqual(result, {
        status: 'success',
        output: `graph {
	graph [bb="0,0,321.03,214.96"];
	node [label="\\N"];
	a	[height=2.9856,
		image="http://example.com/test.png",
		pos="160.51,107.48",
		width=4.4587];
}
`,
        errors: [],
      });
    });
  });
});
