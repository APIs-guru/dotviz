import { assert, expect } from 'vitest';

import type {
  MultipleRenderResult,
  RenderError,
  RenderResult,
} from '../../src/index.ts';
import { RawString } from './raw-string-serializer.ts';

export function expectSuccessResult(result: RenderResult) {
  const { output } = result;
  expect(stringifyErrors(result.errors)).toStrictEqual('');
  expect(result).toStrictEqual({
    status: 'success',
    output,
    errors: [],
  });
  assert.exists(output);
  return expect(new RawString(output));
}

export function expectFailureResult(
  result: RenderResult | MultipleRenderResult,
) {
  expect(result).toStrictEqual({
    status: 'failure',
    output: null,
    errors: expect.any(Array) as unknown[],
  });
  return expectErrors(result);
}

export function expectErrors(result: RenderResult | MultipleRenderResult) {
  assert.isArray(result.errors);
  return expect(new RawString(stringifyErrors(result.errors)));
}

export function stringifyErrors(errors: RenderError[]): string {
  return errors.map((e) => e.toString()).join('\n\n');
}
