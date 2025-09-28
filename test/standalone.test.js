import assert from 'node:assert/strict';
import { describe, it } from 'node:test';

import * as VizPackage from '../src/index.js';
import Viz from '../src/viz.js';

describe('instance', function () {
  it('returns a promise that resolves to an instance of the Viz class', async function () {
    const viz = await VizPackage.instance();

    assert.ok(viz instanceof Viz);
  });
});
