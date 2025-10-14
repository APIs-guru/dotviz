import terser from '@rollup/plugin-terser';
import typescript from '@rollup/plugin-typescript';
import { dts } from 'rollup-plugin-dts';

import packageJSON from './package.json' with { type: 'json' };

const banner = `/*!
dotviz ${packageJSON.version}

This distribution contains other software in object code form:
Graphviz https://www.graphviz.org
Expat https://libexpat.github.io
*/`;

const tsOptions = {
  noEmit: false,
  sourceMap: true,
  inlineSources: true,
  declaration: false,
};

export default [
  {
    input: 'src/index.ts',
    output: {
      file: 'npmDist/dotviz.js',
      format: 'es',
      sourcemap: true,
      banner,
    },
    plugins: [typescript(tsOptions), terser()],
  },
  {
    input: './src/index.ts',
    output: [{ file: 'npmDist/dotviz.d.ts', format: 'es' }],
    plugins: [dts()],
  },
];
