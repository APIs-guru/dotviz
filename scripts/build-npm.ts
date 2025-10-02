import fs from 'node:fs';

import packageJSON from '../package.json' with { type: 'json' };
import { spawn, writeGeneratedFile } from './utils.ts';

fs.rmSync('lib', { recursive: true, force: true });
fs.mkdirSync('lib');
spawn('zig', ['build'], { cwd: 'backend/' });
fs.copyFileSync('backend/zig-out/bin/dotviz.wasm', 'lib/module.wasm');

const wasm = fs.readFileSync('lib/module.wasm');
const encoded_js = `const encoded = "${wasm.toString('base64')}";

export function decode() {
  const data = atob(encoded);
  const bytes = new Uint8Array(data.length);
  for (let i = 0; i < data.length; i++) {
    bytes[i] = data.charCodeAt(i);
  }
  return bytes.buffer;
}
`;
await writeGeneratedFile('lib/encoded.js', encoded_js);

fs.rmSync('npmDist', { recursive: true, force: true });
fs.mkdirSync('npmDist');

fs.copyFileSync('./LICENSE', './npmDist/LICENSE');
fs.copyFileSync('./README.md', './npmDist/README.md');
fs.copyFileSync('./types/index.d.ts', './npmDist/index.d.ts');

spawn('rollup', ['-c']);

const releasePackageJSON = {
  ...packageJSON,
  private: undefined,
  scripts: undefined,
  devDependencies: undefined,
  main: './viz.js',
  exports: {
    types: './index.d.ts',
    require: './viz.cjs',
    default: './viz.js',
  },
};

// Should be done as the last step so only valid packages can be published
await writeGeneratedFile(
  './npmDist/package.json',
  JSON.stringify(releasePackageJSON, undefined, 2),
);
