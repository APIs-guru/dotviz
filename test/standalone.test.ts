import { describe, expect, it } from 'vitest';

import { dotvizInstance } from '../src/index.ts';
import { Viz } from '../src/viz.ts';

describe('instance', () => {
  it('returns a promise that resolves to an instance of the Viz class', async () => {
    const viz = await dotvizInstance();

    expect(viz).toBeInstanceOf(Viz);
  });
});
