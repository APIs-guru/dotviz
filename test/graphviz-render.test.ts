import assert from 'node:assert/strict';
import fs from 'node:fs';
import { describe, it } from 'node:test';

import * as VizPackage from '../src/index.ts';

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
        {
          format: 'svg',
        },
      );

      assert.deepStrictEqual(result, {
        status: 'success',
        output: fs.readFileSync('test/snapshots/comment_attribute.svg', 'utf8'),
        errors: [],
      });
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
      const dot = String.raw`digraph G {
	graph [bb="0,0,199.27,108",
		layers="local:pvt:test:new:ofc"
	];
	node [label="\N"];
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
`;
      const svg = fs.readFileSync('test/snapshots/layers_support.svg', 'utf8');
      assert.deepStrictEqual(result, {
        status: 'success',
        output: { dot, svg },
        errors: [
          {
            level: 'warning',
            message: 'layers not supported in dot output',
          },
        ],
      });
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
      const dot = String.raw`digraph G {
	graph [_background="c 7 -#ff0000 p 4 4 4 36 4 36 36 4 36",
		bb="0,0,54,108"
	];
	node [label="\N"];
	a	[height=0.5,
		pos="27,90",
		width=0.75];
	b	[height=0.5,
		pos="27,18",
		width=0.75];
	a -> b	[pos="e,27,36.104 27,71.697 27,64.407 27,55.726 27,47.536"];
}
`;
      const svg = fs.readFileSync(
        'test/snapshots/_background_attribute.svg',
        'utf8',
      );
      assert.deepStrictEqual(result, {
        status: 'success',
        output: { dot, svg },
        errors: [],
      });
    });
  });
});
