import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    snapshotSerializers: ['test/util/raw-string-serializer.ts'],
    coverage: {
      provider: 'v8',
      include: ['src/**/*.ts', 'test/**/*.ts'],
      exclude: ['**/*.d.ts', 'test/types/**'],
      reportsDirectory: './reports/coverage',
      thresholds: {
        statements: 100,
        lines: 100,
        branches: 100,
        functions: 100,
      },
    },
  },
});
