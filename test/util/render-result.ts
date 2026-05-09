import { assert, expect } from 'vitest';

import type { Diagnostic, RenderResult } from '../../src/index.ts';
import { RawString } from './raw-string-serializer.ts';

export function expectDot(result: RenderResult) {
  const { output, diagnostics } = result;
  expect(stringifyDiagnostics(diagnostics)).toStrictEqual('');
  expect(result).toStrictEqual({ status: 'success', output, diagnostics });
  assert.exists(output?.dot);
  return expect(new RawString(output.dot));
}

export function expectSvg(result: RenderResult) {
  const { output, diagnostics } = result;
  expect(stringifyDiagnostics(diagnostics)).toStrictEqual('');
  expect(result).toStrictEqual({ status: 'success', output, diagnostics });
  assert.exists(output?.svg);
  return expect(new RawString(output.svg));
}

export function expectDotWithWarnings(result: RenderResult) {
  const { output, diagnostics } = result;
  expect(result).toStrictEqual({ status: 'success', output, diagnostics });
  assert.exists(output?.dot);
  return expect(
    new RawString(stringifyDiagnostics(diagnostics) + '\n\n' + output.dot),
  );
}

export function expectFailureResult(result: RenderResult) {
  expect(result).toStrictEqual({
    status: 'failure',
    output: undefined,
    diagnostics: expect.any(Array) as unknown[],
  });
  return expectDiagnostics(result);
}

export function expectDiagnostics(result: RenderResult) {
  assert.isArray(result.diagnostics);
  return expect(new RawString(stringifyDiagnostics(result.diagnostics)));
}

export function stringifyDiagnostics(diagnostics: Diagnostic[]): string {
  return diagnostics.map((e) => e.toString()).join('\n\n');
}
