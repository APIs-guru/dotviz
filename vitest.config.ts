import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    snapshotSerializers: ['test/util/raw-string-serializer.ts'],
  },
});
