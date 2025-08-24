import fs from 'node:fs';

import packageJSON from '../package.json' with { type: 'json' };
import Viz from '../src/viz.js';
import { spawn, writeGeneratedFile } from './utils.js';

fs.rmSync('lib', { recursive: true, force: true });
fs.mkdirSync('lib');
spawn('zig', ['build'], { cwd: 'backend/zig' });
fs.copyFileSync('backend/zig/zig-out/bin/dotviz.wasm', 'lib/module.wasm');

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

delete packageJSON.private;
delete packageJSON.scripts;
delete packageJSON.devDependencies;
packageJSON.main = './viz.js';
packageJSON.exports = {
  types: './index.d.ts',
  require: './viz.cjs',
  default: './viz.js',
};

// Should be done as the last step so only valid packages can be published
await writeGeneratedFile(
  './npmDist/package.json',
  JSON.stringify(packageJSON, undefined, 2),
);
