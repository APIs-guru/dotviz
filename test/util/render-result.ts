import { assert, expect } from 'vitest';

import { type Diagnostic, type RenderResult } from '../../src/index.ts';

export function expectDot(result: RenderResult) {
  const { output, diagnostics } = result;
  expect(stringifyDiagnostics(diagnostics)).toBe('');
  expect(result).toStrictEqual({ status: 'success', output, diagnostics });
  assert.exists(output?.dot);
  // oxlint-disable-next-line vitest/valid-expect
  return expect(output.dot);
}

export function expectSvg(result: RenderResult) {
  const { output, diagnostics } = result;
  expect(stringifyDiagnostics(diagnostics)).toBe('');
  expect(result).toStrictEqual({ status: 'success', output, diagnostics });
  assert.exists(output?.svg);
  // oxlint-disable-next-line vitest/valid-expect
  return expect(output.svg);
}

export function expectDotWithWarnings(result: RenderResult) {
  const { output, diagnostics } = result;
  expect(result).toStrictEqual({ status: 'success', output, diagnostics });
  assert.exists(output?.dot);
  // oxlint-disable-next-line vitest/valid-expect
  return expect(stringifyDiagnostics(diagnostics) + '\n\n' + output.dot);
}

export function expectFailureResult(result: RenderResult) {
  expect(result).toStrictEqual({
    status: 'failure',
    output: undefined,
    diagnostics: expect.any(Array) as unknown,
  });
  assert.isArray(result.diagnostics);
  // oxlint-disable-next-line vitest/valid-expect
  return expect(stringifyDiagnostics(result.diagnostics));
}

function stringifyDiagnostics(diagnostics: Diagnostic[]): string {
  return diagnostics.map((e) => e.toString()).join('\n\n');
}
