import { assert, expect } from 'vitest';

import type { RenderResult } from '../../src/index.ts';
import { RawString } from './raw-string-serializer.ts';

export function expectSuccessResult(result: RenderResult) {
  const { output } = result;
  expect(result).toStrictEqual({
    status: 'success',
    output,
    errors: [],
  });
  assert.exists(output);
  return expect(new RawString(output));
}

export function expectFailureResult(result: RenderResult) {
  const { errors } = result;
  expect(result).toStrictEqual({
    status: 'failure',
    output: null,
    errors,
  });
  assert.isArray(errors);
  return expect(errors);
}
