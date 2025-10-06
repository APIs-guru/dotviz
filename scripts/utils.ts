import childProcess from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import url from 'node:url';

import { format } from 'prettier';

import prettierConfig from '../.prettierrc.json' with { type: 'json' };
import packageJSON from '../package.json' with { type: 'json' };

export { packageJSON };

export function localRepoPath(...paths: readonly string[]): string {
  const resourcesDir = path.dirname(url.fileURLToPath(import.meta.url));
  const repoDir = path.join(resourcesDir, '..');
  return path.join(repoDir, ...paths);
}

interface MakeTmpDirReturn {
  tmpDirPath: (...paths: readonly string[]) => string;
}

export function makeTmpDir(name: string): MakeTmpDirReturn {
  const tmpDir = path.join(os.tmpdir(), packageJSON.name, name);
  fs.rmSync(tmpDir, { recursive: true, force: true });
  fs.mkdirSync(tmpDir, { recursive: true });

  return {
    tmpDirPath: (...paths) => path.join(tmpDir, ...paths),
  };
}

interface NPMOptions extends SpawnOptions {
  quiet?: boolean;
}

export function npm(options?: NPMOptions) {
  const globalOptions = options?.quiet === true ? ['--quiet'] : [];

  return {
    run(...args: readonly string[]): void {
      spawn('npm', [...globalOptions, 'run', ...args], options);
    },
    install(...args: readonly string[]): void {
      spawn('npm', [...globalOptions, 'install', ...args], options);
    },
    ci(...args: readonly string[]): void {
      spawn('npm', [...globalOptions, 'ci', ...args], options);
    },
    exec(...args: readonly string[]): void {
      spawn('npm', [...globalOptions, 'exec', ...args], options);
    },
    pack(...args: readonly string[]): string {
      return spawnOutput('npm', [...globalOptions, 'pack', ...args], options);
    },
    diff(...args: readonly string[]): string {
      return spawnOutput('npm', [...globalOptions, 'diff', ...args], options);
    },
  };
}

interface GITOptions extends SpawnOptions {
  quiet?: boolean;
}

export function git(options?: GITOptions) {
  const cmdOptions = options?.quiet === true ? ['--quiet'] : [];
  return {
    clone(...args: readonly string[]): void {
      spawn('git', ['clone', ...cmdOptions, ...args], options);
    },
    checkout(...args: readonly string[]): void {
      spawn('git', ['checkout', ...cmdOptions, ...args], options);
    },
    revParse(...args: readonly string[]): string {
      return spawnOutput('git', ['rev-parse', ...cmdOptions, ...args], options);
    },
    revList(...args: readonly string[]): string[] {
      const allArgs = ['rev-list', ...cmdOptions, ...args];
      const result = spawnOutput('git', allArgs, options);
      return result === '' ? [] : result.split('\n');
    },
    catFile(...args: readonly string[]): string {
      return spawnOutput('git', ['cat-file', ...cmdOptions, ...args], options);
    },
    log(...args: readonly string[]): string {
      return spawnOutput('git', ['log', ...cmdOptions, ...args], options);
    },
  };
}

interface SpawnOptions {
  cwd?: string;
  env?: typeof process.env;
}

function spawnOutput(
  command: string,
  args: readonly string[],
  options?: SpawnOptions,
): string {
  const result = childProcess.spawnSync(command, args, {
    maxBuffer: 10 * 1024 * 1024, // 10MB
    stdio: ['inherit', 'pipe', 'inherit'],
    encoding: 'utf8',
    ...options,
  });

  if (result.status !== 0) {
    throw new Error(`Command failed: ${command} ${args.join(' ')}`);
  }

  return result.stdout.trimEnd();
}

export function spawn(
  command: string,
  args: readonly string[],
  options?: SpawnOptions,
): void {
  const result = childProcess.spawnSync(command, args, {
    stdio: 'inherit',
    ...options,
  });
  if (result.status !== 0) {
    throw new Error(`Command failed: ${command} ${args.join(' ')}`);
  }
}

function* readdirRecursive(dirPath: string): Generator<{
  name: string;
  filepath: string;
  stats: fs.Stats;
}> {
  for (const name of fs.readdirSync(dirPath)) {
    const filepath = path.join(dirPath, name);
    const stats = fs.lstatSync(filepath);

    if (stats.isDirectory()) {
      yield* readdirRecursive(filepath);
    } else {
      yield { name, filepath, stats };
    }
  }
}

export function showDirStats(dirPath: string): void {
  const fileTypes: Record<string, { filepaths: string[]; size: number }> = {};
  let totalSize = 0;

  for (const { name, filepath, stats } of readdirRecursive(dirPath)) {
    const ext = name.split('.').slice(1).join('.');
    const filetype = ext ? '*.' + ext : name;

    fileTypes[filetype] ??= { filepaths: [], size: 0 };

    totalSize += stats.size;
    fileTypes[filetype].size += stats.size;
    fileTypes[filetype].filepaths.push(filepath);
  }

  const stats: [string, number][] = [];
  for (const [filetype, typeStats] of Object.entries(fileTypes)) {
    const numFiles = typeStats.filepaths.length;

    if (numFiles > 1) {
      stats.push([filetype + ' x' + numFiles.toString(), typeStats.size]);
    } else {
      const relativePath = path.relative(dirPath, typeStats.filepaths[0]);
      stats.push([relativePath, typeStats.size]);
    }
  }
  stats.sort((a, b) => b[1] - a[1]);

  const prettyStats = stats.map(([type, size]) => [
    type,
    (size / 1024).toFixed(2) + ' KB',
  ]);

  const typeMaxLength = Math.max(...prettyStats.map((x) => x[0].length));
  const sizeMaxLength = Math.max(...prettyStats.map((x) => x[1].length));
  for (const [type, size] of prettyStats) {
    console.log(
      type.padStart(typeMaxLength) + ' | ' + size.padStart(sizeMaxLength),
    );
  }

  console.log('-'.repeat(typeMaxLength + 3 + sizeMaxLength));
  const totalMB = (totalSize / 1024 / 1024).toFixed(2) + ' MB';
  console.log(
    'Total'.padStart(typeMaxLength) + ' | ' + totalMB.padStart(sizeMaxLength),
  );
}

export async function writeGeneratedFile(
  filepath: string,
  body: string,
): Promise<void> {
  fs.mkdirSync(path.dirname(filepath), { recursive: true });
  const formatted = await format(body, {
    filepath,
    ...prettierConfig,
  });
  fs.writeFileSync(filepath, formatted);
}
