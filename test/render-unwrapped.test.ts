import { describe, expect, it } from 'vitest';

import * as VizPackage from '../src/index.ts';
import { expectString } from './util/raw-string-serializer.ts';

describe('Viz', () => {
  describe('renderString', () => {
    it('returns the output for the first graph, even if subsequent graphs have errors', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderString('graph a { } graph {');

      expectString(result).toMatchInlineSnapshot(`
        graph a {
        	graph [bb="0,0,0,0"];
        	node [label="\\N"];
        }
      `);
    });

    it('throws an error if the first graph has a syntax error', async () => {
      const viz = await VizPackage.instance();
      expect(() =>
        viz.renderString('graph {'),
      ).toThrowErrorMatchingInlineSnapshot(`[Error: syntax error in line 1]`);
    });

    it('throws an error for layout errors', async () => {
      const viz = await VizPackage.instance();
      expect(() =>
        viz.renderString('graph { layout=invalid }'),
      ).toThrowErrorMatchingInlineSnapshot(
        `[Error: Layout type: "invalid" not recognized. Use one of: dot circo neato fdp twopi patchwork]`,
      );
    });

    it('throws an error if there are no graphs in the input', async () => {
      const viz = await VizPackage.instance();
      expect(() => viz.renderString('')).toThrowErrorMatchingInlineSnapshot(
        `[Error: render failed]`,
      );
    });

    it('throws an error with the first render error message', async () => {
      const viz = await VizPackage.instance();
      expect(() =>
        viz.renderString('graph { layout=invalid; x=1.2.3=y }'),
      ).toThrowErrorMatchingInlineSnapshot(
        `[Error: Layout type: "invalid" not recognized. Use one of: dot circo neato fdp twopi patchwork]`,
      );
    });

    it('throws for invalid format option', async () => {
      const viz = await VizPackage.instance();
      expect(() =>
        viz.renderString('graph { }', { format: 'invalid' }),
      ).toThrowErrorMatchingInlineSnapshot(
        `[Error: Format: "invalid" not recognized. Use one of: dot gv svg]`,
      );
    });

    it('throws for invalid engine option', async () => {
      const viz = await VizPackage.instance();
      expect(() =>
        viz.renderString('graph { }', { engine: 'invalid' }),
      ).toThrowErrorMatchingInlineSnapshot(
        `[Error: Layout type: "invalid" not recognized. Use one of: dot circo neato fdp twopi patchwork]`,
      );
    });

    it('accepts a non-ASCII character', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderString('digraph { a [label=図] }');
      expect(result).toMatch(/label=図/);
    });

    it('a graph with unterminated string followed by another call with a valid graph', async () => {
      const viz = await VizPackage.instance();
      expect(() => viz.renderString('graph { a[label="blah'))
        .toThrowErrorMatchingInlineSnapshot(`
        [Error: syntax error in line 1 scanning a quoted string (missing endquote? longer than 16384?)
        String starting:"blah]
      `);
      expectString(viz.renderString('graph { a }')).toMatchInlineSnapshot(`
        graph {
        	graph [bb="0,0,54,36"];
        	node [label="\\N"];
        	a	[height=0.5,
        		pos="27,18",
        		width=0.75];
        }
      `);
    });
  });
});
