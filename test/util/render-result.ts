import { assert, expect } from 'vitest';

import type { RenderError, RenderResult } from '../../src/index.ts';
import { RawString } from './raw-string-serializer.ts';

export function expectDot(result: RenderResult) {
  const { output, errors } = result;
  expect(stringifyErrors(errors)).toStrictEqual('');
  expect(result).toStrictEqual({ status: 'success', output, errors });
  assert.exists(output?.dot);
  return expect(new RawString(output.dot));
}

export function expectSvg(result: RenderResult) {
  const { output, errors } = result;
  expect(stringifyErrors(errors)).toStrictEqual('');
  expect(result).toStrictEqual({ status: 'success', output, errors });
  assert.exists(output?.svg);
  return expect(new RawString(output.svg));
}

export function expectDotWithWarnings(result: RenderResult) {
  const { output, errors } = result;
  expect(result).toStrictEqual({ status: 'success', output, errors });
  assert.exists(output?.dot);
  return expect(new RawString(stringifyErrors(errors) + '\n\n' + output.dot));
}

export function expectFailureResult(result: RenderResult) {
  expect(result).toStrictEqual({
    status: 'failure',
    output: undefined,
    errors: expect.any(Array) as unknown[],
  });
  return expectErrors(result);
}

export function expectErrors(result: RenderResult) {
  assert.isArray(result.errors);
  return expect(new RawString(stringifyErrors(result.errors)));
}

export function stringifyErrors(errors: RenderError[]): string {
  return errors.map((e) => e.toString()).join('\n\n');
}
