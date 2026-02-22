import js from '@eslint/js';
import json from '@eslint/json';
import markdown from '@eslint/markdown';
import { defineConfig } from 'eslint/config';
import { createNodeResolver, importX } from 'eslint-plugin-import-x';
import simpleImportSort from 'eslint-plugin-simple-import-sort';
import eslintPluginUnicorn from 'eslint-plugin-unicorn';
import globals from 'globals';
import tseslint from 'typescript-eslint';

export default defineConfig([
  {
    ignores: ['reports', 'npmDist/', 'lib/', 'test/types/', 'backend/**'],
  },
  {
    linterOptions: {
      noInlineConfig: false,
      reportUnusedInlineConfigs: 'error',
      reportUnusedDisableDirectives: 'error',
    },
    settings: {
      'import-x/resolver-next': [
        createNodeResolver(/* Your override options go here */),
      ],
    },
  },
  {
    files: ['**/*.{js,mjs,cjs}'],
    plugins: {
      'simple-import-sort': simpleImportSort,
    },
    languageOptions: { globals: globals['shared-node-browser'] },
    extends: [
      js.configs.recommended,
      importX.flatConfigs.recommended,
      eslintPluginUnicorn.configs.recommended,
    ],
    rules: {
      'simple-import-sort/imports': 'error',
      'simple-import-sort/exports': 'error',
      'import-x/first': 'error',
      'import-x/newline-after-import': 'error',
      'import-x/no-duplicates': 'error',

      'unicorn/prevent-abbreviations': 'off',
      'unicorn/prefer-query-selector': 'off',
      'unicorn/prefer-export-from': ['error', { ignoreUsedVariables: true }],
    },
  },
  {
    files: ['**/*.{ts,tsx}'],
    plugins: {
      'simple-import-sort': simpleImportSort,
    },
    extends: [
      js.configs.recommended,
      tseslint.configs.strictTypeChecked,
      tseslint.configs.stylisticTypeChecked,
      importX.flatConfigs.recommended,
      importX.flatConfigs.typescript,
      eslintPluginUnicorn.configs.recommended,
    ],
    languageOptions: {
      parserOptions: {
        projectService: true,
        tsconfigRootDir: import.meta.dirname,
      },
    },
    rules: {
      'simple-import-sort/imports': 'error',
      'simple-import-sort/exports': 'error',
      'import-x/first': 'error',
      'import-x/newline-after-import': 'error',
      'import-x/no-duplicates': 'error',

      '@typescript-eslint/unbound-method': ['error', { ignoreStatic: true }],
      '@typescript-eslint/no-invalid-void-type': [
        'error',
        { allowAsThisParameter: true },
      ],
      '@typescript-eslint/no-floating-promises': [
        'error',
        {
          allowForKnownSafeCalls: [
            {
              from: 'package',
              package: 'node:test',
              name: ['it', 'describe', 'skip', 'only'],
            },
          ],
        },
      ],
      '@typescript-eslint/no-unnecessary-condition': [
        'error',
        { allowConstantLoopConditions: 'only-allowed-literals' },
      ],

      'unicorn/prevent-abbreviations': 'off',
      'unicorn/prefer-query-selector': 'off',
      'unicorn/no-null': 'off',
      'unicorn/prefer-export-from': ['error', { ignoreUsedVariables: true }],
      'unicorn/prefer-ternary': ['error', 'only-single-line'],
      'unicorn/number-literal-case': [
        'error',
        { hexadecimalValue: 'lowercase' }, // required to not have conflicts with prettier
      ],
      'unicorn/switch-case-braces': ['error', 'avoid'],
    },
  },
  {
    files: ['src/**/*.{js,mjs,cjs}'],
    languageOptions: { globals: globals['browser'] },
  },
  {
    files: ['scripts/**/*.{js,mjs,cjs}'],
    languageOptions: { globals: globals['node'] },
  },
  {
    files: ['**/*.json'],
    ignores: ['**/package-lock.json'],
    plugins: { json },
    language: 'json/json',
    extends: ['json/recommended'],
  },
  {
    files: ['**/*.json5', '**/tsconfig.json'],
    plugins: { json },
    language: 'json/json5',
    extends: ['json/recommended'],
  },
  {
    files: ['**/*.md'],
    plugins: { markdown },
    language: 'markdown/gfm',
    extends: ['markdown/recommended'],
  },
]);
