import fs from 'node:fs';
import path from 'node:path';

import { describe, expect, it } from 'vitest';

import * as VizPackage from '../src/index.ts';
import { expectString } from './util/raw-string-serializer.ts';
import { expectErrors, expectSuccessResult } from './util/render-result.ts';

const __dirname = import.meta.dirname;
function readSnapshot(filepath: string): string {
  return fs.readFileSync(path.join(__dirname, filepath), 'utf8');
}

describe('Viz', () => {
  describe('render', () => {
    it('comment attribute', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render(
        `digraph {
          comment = "I am a graph"
          A[comment = "I am node A"]
          B[comment = "I am node B"]
          A -> B[comment = "I am an edge"]
        }`,
        { format: 'svg' },
      );

      await expectSuccessResult(result).toMatchFileSnapshot(
        './snapshots/comment_attribute.svg',
      );
    });
    it('layers support', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderFormats(
        `digraph G {
         	layers="local:pvt:test:new:ofc";

         	node1  [layer="pvt"];
         	node2  [layer="all"];
         	node3  [layer="pvt:ofc"];		/* pvt, test, new, and ofc */
         	node2 -> node3  [layer="pvt:all"];	/* same as pvt:ofc */
         	node2 -> node4 [layer=3];		/* same as test */
        }`,
        ['dot', 'svg'],
      );

      expect(result).toStrictEqual({
        status: 'success',
        output: {
          dot: expect.any(String) as unknown,
          svg: expect.any(String) as unknown,
        },
        errors: expect.any(Array) as unknown[],
      });

      expectErrors(result).toMatchInlineSnapshot(`RenderingBackendError: layers not supported in dot output`);

      expectString(result.output?.dot).toMatchInlineSnapshot(`
        digraph G {
        	graph [bb="0,0,199.27,108",
        		layers="local:pvt:test:new:ofc"
        	];
        	node [label="\\N"];
        	node1	[height=0.5,
        		layer=pvt,
        		pos="34.637,90",
        		width=0.96213];
        	node2	[height=0.5,
        		layer=all,
        		pos="121.64,90",
        		width=0.96213];
        	node3	[height=0.5,
        		layer="pvt:ofc",
        		pos="77.637,18",
        		width=0.96213];
        	node2 -> node3	[layer="pvt:all",
        		pos="e,87.989,35.47 111.21,72.411 106.06,64.216 99.724,54.14 93.951,44.955"];
        	node4	[height=0.5,
        		pos="164.64,18",
        		width=0.96213];
        	node2 -> node4	[layer=3,
        		pos="e,154.52,35.47 131.83,72.411 136.81,64.304 142.92,54.354 148.51,45.248"];
        }
      `);

      await expectString(result.output?.svg).toMatchFileSnapshot(
        './snapshots/layers_support.svg',
      );
    });
    it('_background attribute', async () => {
      const viz = await VizPackage.instance();
      const result = viz.renderFormats(
        `digraph G {
          _background="c 7 -#ff0000 p 4 4 4 36 4 36 36 4 36";
          a -> b
        }`,
        ['dot', 'svg'],
      );

      expect(result).toStrictEqual({
        status: 'success',
        output: {
          dot: expect.any(String) as unknown,
          svg: expect.any(String) as unknown,
        },
        errors: [],
      });
      expectString(result.output?.dot).toMatchInlineSnapshot(`
        digraph G {
        	graph [_background="c 7 -#ff0000 p 4 4 4 36 4 36 36 4 36",
        		bb="0,0,54,108"
        	];
        	node [label="\\N"];
        	a	[height=0.5,
        		pos="27,90",
        		width=0.75];
        	b	[height=0.5,
        		pos="27,18",
        		width=0.75];
        	a -> b	[pos="e,27,36.104 27,71.697 27,64.407 27,55.726 27,47.536"];
        }
      `);
      await expectString(result.output?.svg).toMatchFileSnapshot(
        './snapshots/_background_attribute.svg',
      );
    });
  });
  it('multiple pages in ps, one in svg', async () => {
    const viz = await VizPackage.instance();
    const result = viz.render(readSnapshot('./snapshots/multiple_pages.gv'), {
      format: 'svg',
    });

    await expectSuccessResult(result).toMatchFileSnapshot(
      './snapshots/multiple_pages.svg',
    );
  });
  it('circo layout', async () => {
    const viz = await VizPackage.instance();
    const result = viz.renderFormats(
      readSnapshot('./snapshots/circo.gv'),
      ['dot', 'svg'],
      { engine: 'circo' },
    );

    expect(result).toStrictEqual({
      status: 'success',
      output: {
        dot: expect.any(String) as unknown,
        svg: expect.any(String) as unknown,
      },
      errors: [],
    });

    await expectString(result.output?.dot).toMatchFileSnapshot(
      './snapshots/circo.dot',
    );
    await expectString(result.output?.svg).toMatchFileSnapshot(
      './snapshots/circo.svg',
    );
  });
});
