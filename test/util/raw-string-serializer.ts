import { assert, expect, type SnapshotSerializer } from 'vitest';

const RawStringSymbol = Symbol.for('RawString');

export class RawString {
  private [RawStringSymbol] = true;

  constructor(public readonly str: string) {}

  static isRawString(val: unknown): val is RawString {
    return typeof val === 'object' && val !== null && RawStringSymbol in val;
  }
}

export function expectString(str: string | null | undefined) {
  assert.exists(str);
  return expect(new RawString(str));
}

export default {
  test: RawString.isRawString,
  serialize(val: RawString) {
    return val.str;
  },
} satisfies SnapshotSerializer;
