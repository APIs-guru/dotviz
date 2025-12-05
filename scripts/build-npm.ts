import crypto from 'node:crypto';
import fs from 'node:fs';

import {
  packageJSON,
  showDirStats,
  spawn,
  writeGeneratedFile,
} from './utils.ts';

fs.rmSync('lib', { recursive: true, force: true });
fs.mkdirSync('lib');
const zigOptimizeMode = process.env.ZIG_OPTIMIZE_MODE ?? 'ReleaseSmall';
/* cspell:disable-next-line */
spawn('zig', ['build', '-Doptimize=' + zigOptimizeMode], { cwd: 'backend/' });
fs.copyFileSync('backend/zig-out/bin/dotviz.wasm', 'lib/module.wasm');

const wasm = fs.readFileSync('lib/module.wasm');
const hash = crypto.createHash('sha256').update(wasm).digest('hex');
const encoded_ts = `const encoded = "${wasm.toString('base64')}";

export function decode(): ArrayBuffer {
  const data = atob(encoded);
  const bytes = new Uint8Array(data.length);
  for (let i = 0; i < data.length; i++) {
    bytes[i] = data.charCodeAt(i);
  }
  return bytes.buffer;
}

export const WASM_HASH = \`${hash}\`;
`;
await writeGeneratedFile('lib/encoded.ts', encoded_ts);

fs.rmSync('npmDist', { recursive: true, force: true });
fs.mkdirSync('npmDist');

fs.copyFileSync('./LICENSE', './npmDist/LICENSE');
fs.copyFileSync('./README.md', './npmDist/README.md');

spawn('rollup', ['-c']);

const worker = fs
  .readFileSync('./npmDist/dotviz-worker.js', 'utf8')
  .replaceAll('\\', '\\\\')
  .replaceAll('`', '\\`')
  .replaceAll('${', '\\${');

const inline_worker_js = `
  const worker_string = String.raw \`${worker}\`;
  export default worker_string;
`;
await writeGeneratedFile('./npmDist/dotviz-inline-worker.js', inline_worker_js);

const inline_worker_dts = `
declare const worker_string: string;
export default worker_string;
`;
await writeGeneratedFile(
  './npmDist/dotviz-inline-worker.d.ts',
  inline_worker_dts,
);

const releasePackageJSON = {
  ...packageJSON,
  private: undefined,
  scripts: undefined,
  devDependencies: undefined,
  exports: {
    '.': {
      default: './dotviz.js',
      types: './dotviz.d.ts',
    },
    './dotviz-worker': {
      default: './dotviz-worker.js',
      types: './dotviz-worker.d.ts',
    },
    './dotviz-inline-worker': {
      default: './dotviz-inline-worker.js',
      types: './dotviz-inline-worker.d.ts',
    },
  },
};

// Should be done as the last step so only valid packages can be published
await writeGeneratedFile(
  './npmDist/package.json',
  JSON.stringify(releasePackageJSON, undefined, 2),
);

showDirStats('./npmDist');
