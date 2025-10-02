import childProcess from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import url from 'node:url';

import { format } from 'prettier';

import prettierConfig from '../.prettierrc.json' with { type: 'json' };

export function localRepoPath(...paths: ReadonlyArray<string>): string {
  const resourcesDir = path.dirname(url.fileURLToPath(import.meta.url));
  const repoDir = path.join(resourcesDir, '..');
  return path.join(repoDir, ...paths);
}

interface MakeTmpDirReturn {
  tmpDirPath: (...paths: ReadonlyArray<string>) => string;
}

export function makeTmpDir(name: string): MakeTmpDirReturn {
  const tmpDir = path.join(os.tmpdir(), name);
  fs.rmSync(tmpDir, { recursive: true, force: true });
  fs.mkdirSync(tmpDir);

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
    run(...args: ReadonlyArray<string>): void {
      spawn('npm', [...globalOptions, 'run', ...args], options);
    },
    install(...args: ReadonlyArray<string>): void {
      spawn('npm', [...globalOptions, 'install', ...args], options);
    },
    ci(...args: ReadonlyArray<string>): void {
      spawn('npm', [...globalOptions, 'ci', ...args], options);
    },
    exec(...args: ReadonlyArray<string>): void {
      spawn('npm', [...globalOptions, 'exec', ...args], options);
    },
    pack(...args: ReadonlyArray<string>): string {
      return spawnOutput('npm', [...globalOptions, 'pack', ...args], options);
    },
    diff(...args: ReadonlyArray<string>): string {
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
    clone(...args: ReadonlyArray<string>): void {
      spawn('git', ['clone', ...cmdOptions, ...args], options);
    },
    checkout(...args: ReadonlyArray<string>): void {
      spawn('git', ['checkout', ...cmdOptions, ...args], options);
    },
    revParse(...args: ReadonlyArray<string>): string {
      return spawnOutput('git', ['rev-parse', ...cmdOptions, ...args], options);
    },
    revList(...args: ReadonlyArray<string>): Array<string> {
      const allArgs = ['rev-list', ...cmdOptions, ...args];
      const result = spawnOutput('git', allArgs, options);
      return result === '' ? [] : result.split('\n');
    },
    catFile(...args: ReadonlyArray<string>): string {
      return spawnOutput('git', ['cat-file', ...cmdOptions, ...args], options);
    },
    log(...args: ReadonlyArray<string>): string {
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
  args: ReadonlyArray<string>,
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

  return result.stdout.toString().trimEnd();
}

export function spawn(
  command: string,
  args: ReadonlyArray<string>,
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
