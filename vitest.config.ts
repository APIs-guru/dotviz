import { defineConfig } from 'vitest/config';

// oxlint-disable-next-line import/no-default-export
export default defineConfig({
  test: {
    setupFiles: 'test/setup/extend-expect.ts',
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
