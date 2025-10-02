import js from '@eslint/js';
import json from '@eslint/json';
import markdown from '@eslint/markdown';
import { defineConfig } from 'eslint/config';
import { createNodeResolver, importX } from 'eslint-plugin-import-x';
import simpleImportSort from 'eslint-plugin-simple-import-sort';
import eslintPluginUnicorn from 'eslint-plugin-unicorn';
import globals from 'globals';

export default defineConfig([
  {
    ignores: ['npmDist/', 'lib/'],
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
      js,
      'import-x': importX,
      'simple-import-sort': simpleImportSort,
    },
    languageOptions: { globals: globals['shared-node-browser'] },
    extends: [
      'js/recommended',
      'import-x/flat/recommended',
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
