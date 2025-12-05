import { decode } from '../lib/encoded.ts';
import Viz from './viz.ts';

export { WASM_HASH } from '../lib/encoded.ts';

/**
 * Returns a promise that resolves to an instance of the {@link Viz} class.
 */
export async function instance(): Promise<Viz> {
  // eslint-disable-next-line prefer-const
  let vizInstance: Viz | undefined;
  const buffer = decode();
  const instance = await WebAssembly.instantiate(buffer, {
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
      environ_sizes_get(environCount: number, environBufSize: number) {
        // @ts-expect-error not sure how to properly type it
        const memory: Uint8Array = instance.instance.exports.memory;
        const view = new DataView(memory.buffer);
        view.setUint32(environCount, 0, true);
        view.setUint32(environBufSize, 0, true);
        return 0;
      },
      path_filestat_get() {
        return 44; // __WASI_ERRNO_NOENT
      },
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
      environ_get: wasiErrnoNoSys,
      random_get: wasiErrnoNoSys,
      path_open: wasiErrnoNoSys,
      proc_exit: wasiErrnoNoSys, // FIXME: maybe handle errors
      args_sizes_get: wasiErrnoNoSys,
      args_get: wasiErrnoNoSys,
      clock_time_get: wasiErrnoNoSys,
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
