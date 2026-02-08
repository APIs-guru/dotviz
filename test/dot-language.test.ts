import assert from 'node:assert/strict';
import { describe, it } from 'node:test';

import * as VizPackage from '../src/index.ts';

describe('Dot language support', () => {
  void it.only('empty graph', async () => {
    const viz = await VizPackage.instance();
    const result = viz.render('graph {}');

    assert.deepStrictEqual(result, {
      status: 'success',
      output: `graph {
	graph [bb="0,0,0,0"];
	node [label="\\N"];
}
`,
      errors: [],
    });
  });
});
