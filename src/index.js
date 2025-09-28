import { decode } from '../lib/encoded.js';
import Viz from './viz.js';

export async function instance() {
  let vizInstance;
  let wasm;
  let memory;
  const stderrMessages = [];
  const decoder = new TextDecoder('utf8');

  const fdBuffers = {
    1: '', // stdout
    2: '', // stderr
  };

  const flush = (fd) => {
    const output = fdBuffers[fd];
    if (!output) return;
    if (fd === 1) console.log(output.trimEnd());
    else if (fd === 2) {
      console.error(output.trimEnd());
      stderrMessages.push(output.trimEnd()); // FIXME: check if trimEnd needed
    }
    fdBuffers[fd] = '';
  };

  let flushTimer;
  const scheduleFlush = () => {
    if (flushTimer !== undefined) clearTimeout(flushTimer);
    flushTimer = setTimeout(() => {
      flush(1);
      flush(2);
    }, 10);
  };

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
      jsHandleGraphvizError(ptr) {
        vizInstance._jsHandleGraphvizError(ptr);
      },
    },
    wasi_snapshot_preview1: {
      fd_write(fd, iovs_ptr, iovs_len, nwritten_ptr) {
        if (!memory) return 52; // WASI_ERRNO_NOTSUP
        const mem = new Uint8Array(memory.buffer);
        const view = new DataView(memory.buffer);

        let totalWritten = 0;

        for (let i = 0; i < iovs_len; i++) {
          const base = view.getUint32(iovs_ptr + i * 8, true);
          const len = view.getUint32(iovs_ptr + i * 8 + 4, true);
          const chunk = mem.subarray(base, base + len);
          const text = decoder.decode(chunk);
          totalWritten += len;

          if (fdBuffers[fd] === undefined) {
            console.warn(`fd_write: unknown fd ${fd}`);
          } else {
            fdBuffers[fd] += text;
          }
        }

        view.setUint32(nwritten_ptr, totalWritten, true);
        scheduleFlush();

        return 0;
      },
      environ_sizes_get() {
        return 0;
      },
      environ_get() {
        return 0;
      },
      path_filestat_get() {
        return 44; // __WASI_ERRNO_NOENT
      },
      fd_close: wasiErrnoBadF,
      fd_fdstat_set_flags: wasiErrnoBadF,
      fd_filestat_get: wasiErrnoBadF,
      fd_prestat_get: wasiErrnoBadF,
      fd_prestat_dir_name: wasiErrnoBadF,
      fd_read: wasiErrnoBadF,
      fd_seek: wasiErrnoBadF,
      fd_fdstat_get: wasiErrnoBadF,
      random_get: wasiErrnoNoSys,
      path_open: wasiErrnoNoSys,
      proc_exit: wasiErrnoNoSys, // FIXME: maybe handle errors
      args_sizes_get: wasiErrnoNoSys,
      args_get: wasiErrnoNoSys,
      clock_time_get: wasiErrnoNoSys,
    },
  });

  wasm = instance.instance.exports;
  memory = wasm.memory;

  vizInstance = new Viz(instance, stderrMessages);
  return vizInstance;
}

function wasiErrnoBadF() {
  return 8; // WASI_ERRNO_BADF
}

function wasiErrnoNoSys() {
  return 52; // WASI_ERRNO_NOSYS
}
