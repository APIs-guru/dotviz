import { addSerializer } from '@vitest/snapshot';
import { expect, Snapshots } from 'vitest';

declare module 'vitest' {
  interface Assertion<T = any> {
    toMatchRawStringInlineSnapshot(inlineSnapshot?: string): T;
  }
}

const RawStringSymbol = Symbol.for('RawString');
class RawString {
  private [RawStringSymbol] = true;
  str: string;

  constructor(str: string) {
    this.str = str;
  }
}

// oxlint-disable-next-line vitest/require-hook
addSerializer({
  test(val: unknown): val is RawString {
    return typeof val === 'object' && val !== null && RawStringSymbol in val;
  },
  serialize(val: RawString) {
    return val.str;
  },
});

// oxlint-disable-next-line vitest/require-hook
expect.extend({
  toMatchRawStringInlineSnapshot(received: unknown, inlineSnapshot?: string) {
    if (typeof received !== 'string') {
      throw new TypeError('expected value must be a string');
    }
    return Snapshots.toMatchInlineSnapshot.call(
      this,
      new RawString(received),
      inlineSnapshot,
    );
  },
});
