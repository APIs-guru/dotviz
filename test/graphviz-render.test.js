import assert from 'node:assert/strict';
import fs from 'node:fs';
import { beforeEach, describe, it } from 'node:test';

import * as VizPackage from '../src/index.js';

describe('Viz', function () {
  let viz;

  beforeEach(async function () {
    viz = await VizPackage.instance();
  });

  describe('render', function () {
    it('comment attribute', function () {
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
  });
});
