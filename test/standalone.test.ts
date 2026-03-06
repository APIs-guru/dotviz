import { describe, expect, it } from 'vitest';

import * as VizPackage from '../src/index.ts';
import { Viz } from '../src/viz.ts';

describe('instance', () => {
  it('returns a promise that resolves to an instance of the Viz class', async () => {
    const viz = await VizPackage.instance();

    expect(viz).toBeInstanceOf(Viz);
  });
});
