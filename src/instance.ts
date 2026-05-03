import { WASM_BASE64 } from '../lib/encoded.ts';
import { Viz } from './viz.ts';

let cachedModule: Promise<WebAssembly.Module> | null = null;
export async function compile(): Promise<WebAssembly.Module> {
  if (cachedModule === null) {
    // eslint-disable-next-line @typescript-eslint/no-unsafe-assignment
    const bytes: Uint8Array<ArrayBuffer> =
      // @ts-expect-error FIXME: definition for Uint8Array.fromBase64 should be added in TS6.0
      // eslint-disable-next-line @typescript-eslint/no-unsafe-call
      Uint8Array.fromBase64(WASM_BASE64);
    cachedModule = WebAssembly.compile(bytes);
  }

  return cachedModule;
}

/**
 * Returns a promise that resolves to an instance of the {@link Viz} class.
 */
export async function instance(
  precompiledModule?: WebAssembly.Module,
): Promise<Viz> {
  // eslint-disable-next-line prefer-const
  let vizInstance: Viz | undefined;
  const moduleObject = precompiledModule ?? (await compile());
  const instance = await WebAssembly.instantiate(moduleObject, {
    env: {
      __indirect_function_table: new WebAssembly.Table({
        initial: 0,
        element: 'anyfunc',
      }),
      __stack_pointer: new WebAssembly.Global(
        { value: 'i32', mutable: true },
        1024,
      ),
    },
    wasi_snapshot_preview1: {
      /* v8 ignore next -- used only for debugging */
      fd_write(
        fd: number,
        iovs_ptr: number,
        iovs_len: number,
        nwritten_ptr: number,
      ): number {
        if (vizInstance)
          return vizInstance._wasi_fd_write(
            fd,
            iovs_ptr,
            iovs_len,
            nwritten_ptr,
          );
        return 52; // WASI_ERRNO_NOTSUP
      },
      environ_sizes_get: wasiErrnoNoSys,
      /* v8 ignore next -- FIXME: removed, but currently used by graphviz */
      path_filestat_get() {
        return 44; // __WASI_ERRNO_NOENT
      },
      path_create_directory: wasiErrnoBadF,
      path_filestat_set_times: wasiErrnoBadF,
      path_link: wasiErrnoBadF,
      path_readlink: wasiErrnoBadF,
      path_remove_directory: wasiErrnoBadF,
      path_rename: wasiErrnoBadF,
      path_symlink: wasiErrnoBadF,
      path_unlink_file: wasiErrnoBadF,
      fd_pread: wasiErrnoBadF,
      fd_pwrite: wasiErrnoBadF,
      fd_close: wasiErrnoBadF,
      fd_fdstat_set_flags: wasiErrnoBadF,
      fd_filestat_get: wasiErrnoBadF,
      fd_prestat_get: wasiErrnoBadF,
      fd_prestat_dir_name: wasiErrnoBadF,
      fd_read: wasiErrnoBadF,
      fd_seek: wasiErrnoBadF,
      fd_fdstat_get: wasiErrnoBadF,
      fd_filestat_set_size: wasiErrnoBadF,
      fd_filestat_set_times: wasiErrnoBadF,
      fd_sync: wasiErrnoBadF,
      fd_readdir: wasiErrnoBadF,
      environ_get: wasiErrnoNoSys,
      random_get: wasiErrnoNoSys,
      path_open: wasiErrnoNoSys,
      proc_exit: wasiErrnoNoSys, // FIXME: maybe handle errors
      args_sizes_get: wasiErrnoNoSys,
      args_get: wasiErrnoNoSys,
      clock_time_get: wasiErrnoNoSys,
      clock_res_get: wasiErrnoNoSys,
      poll_oneoff: wasiErrnoNoSys,
    },
  });

  vizInstance = new Viz(instance);
  return vizInstance;
}

function wasiErrnoBadF() {
  return 8; // WASI_ERRNO_BADF
}

function wasiErrnoNoSys() {
  return 52; // WASI_ERRNO_NOSYS
}
