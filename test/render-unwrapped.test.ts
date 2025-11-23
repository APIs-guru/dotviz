import assert from 'node:assert/strict';
import { describe, it } from 'node:test';

import * as VizPackage from '../src/index.ts';

describe('Viz', () => {
  describe('renderString', () => {
    it('returns the output for the first graph, even if subsequent graphs have errors', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderString('graph a { } graph {');

      assert.strictEqual(
        result,
        'graph a {\n\tgraph [bb="0,0,0,0"];\n\tnode [label="\\N"];\n}\n',
      );
    });

    it('throws an error if the first graph has a syntax error', async () => {
      const viz = await VizPackage.instance();
      assert.throws(() => viz.renderString('graph {'), {
        name: 'Error',
        message: 'syntax error in line 1',
      });
    });

    it('throws an error for layout errors', async () => {
      const viz = await VizPackage.instance();
      assert.throws(() => viz.renderString('graph { layout=invalid }'), {
        name: 'Error',
        message: 'Layout type: "invalid" not recognized. Use one of: dot circo',
      });
    });

    it('throws an error if there are no graphs in the input', async () => {
      const viz = await VizPackage.instance();
      assert.throws(() => viz.renderString(''), {
        name: 'Error',
        message: 'render failed',
      });
    });

    it('throws an error with the first render error message', async () => {
      const viz = await VizPackage.instance();
      assert.throws(
        () => viz.renderString('graph { layout=invalid; x=1.2.3=y }'),
        {
          name: 'Error',
          message:
            'Layout type: "invalid" not recognized. Use one of: dot circo',
        },
      );
    });

    it('throws for invalid format option', async () => {
      const viz = await VizPackage.instance();
      assert.throws(
        () => viz.renderString('graph { }', { format: 'invalid' }),
        {
          name: 'Error',
          message: 'Format: "invalid" not recognized. Use one of: dot gv svg',
        },
      );
    });

    void it.skip('throws for invalid engine option', async () => {
      const viz = await VizPackage.instance();
      assert.throws(
        () => viz.renderString('graph { }', { engine: 'invalid' }),
        {
          name: 'Error',
          message:
            'Layout type: "invalid" not recognized. Use one of: dot circo',
        },
      );
    });

    it('accepts a non-ASCII character', async () => {
      const viz = await VizPackage.instance();
      assert.match(viz.renderString('digraph { a [label=図] }'), /label=図/);
    });

    it('a graph with unterminated string followed by another call with a valid graph', async () => {
      const viz = await VizPackage.instance();
      assert.throws(() => viz.renderString('graph { a[label="blah'), {
        name: 'Error',
        message:
          // cspell:disable-next-line
          'syntax error in line 1 scanning a quoted string (missing endquote? longer than 16384?)\nString starting:"blah',
      });
      assert.ok(viz.renderString('graph { a }'));
    });
  });
});
