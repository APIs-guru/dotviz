let wasm;
let memory;
const OUT_LEN = 1000 * 1024;

const fdBuffers = {
  1: "", // stdout
  2: "", // stderr
};

const flush = (fd) => {
  const output = fdBuffers[fd];
  if (!output) return;
  if (fd === 1) console.log(output.trimEnd());
  else if (fd === 2) console.error(output.trimEnd());
  fdBuffers[fd] = "";
};

let flushTimer = null;
const scheduleFlush = () => {
  if (flushTimer !== null) clearTimeout(flushTimer);
  flushTimer = setTimeout(() => {
    flush(1);
    flush(2);
  }, 10);
};

self.onmessage = async function (e) {
  const { type, dot, json } = e.data;

  function status(msg) {
    self.postMessage({ type: "status", status: msg });
  }

  if (type === "init") {
    status("Fetching and instantiating WebAssembly...");
    const response = await fetch("zig-out/bin/dotviz.wasm");
    const buffer = await response.arrayBuffer();

    const instance = await WebAssembly.instantiate(buffer, {
      env: {
        __indirect_function_table: new WebAssembly.Table({ initial: 0, element: "anyfunc" }),
        __stack_pointer: new WebAssembly.Global({ value: "i32", mutable: true }, 1024),
      },
      wasi_snapshot_preview1: {
        proc_exit() {
          return 52; // __WASI_ERRNO_NOSYS
        },
        args_sizes_get(argc_ptr, argv_buf_size_ptr) {
          return 52;
        },

        args_get(argv_ptr, argv_buf_ptr) {
          return 52;
        },

        environ_sizes_get(count_ptr, buf_size_ptr) {
          return 0;
        },

        environ_get(env_ptr, env_buf_ptr) {
          return 0;
        },

        clock_time_get(id, precision, time_ptr) {
          return 52;
        },

        fd_fdstat_get(fd, stat_ptr) {
          return 8; // __WASI_ERRNO_BADF
        },

        fd_close(fd) {
          return 8;
        },

        fd_fdstat_set_flags(fd, flags) {
          return 8;
        },

        fd_filestat_get(fd, stat_ptr) {
          return 8;
        },

        fd_prestat_get(fd, prestat_ptr) {
          return 8;
        },

        fd_prestat_dir_name(fd, path_ptr, path_len) {
          return 8;
        },

        fd_read(fd, iovs_ptr, iovs_len, nread_ptr) {
          return 8;
        },

        fd_seek(fd, offset_low, offset_high, whence, newOffsetPtr) {
          return 8;
        },

        path_filestat_get(fd, flags, path_ptr, path_len, stat_ptr) {
          return 44; // __WASI_ERRNO_NOENT
        },

        path_open(fd, dirflags, path_ptr, path_len, oflags, fs_rights_base, fs_rights_inheriting, fdflags, opened_fd_ptr) {
          return 52;
        },
        fd_write(fd, iovs_ptr, iovs_len, nwritten_ptr) {
          if (!memory) return 52; // WASI_ERRNO_NOTSUP
          const mem = new Uint8Array(memory.buffer);
          const view = new DataView(memory.buffer);
          const decoder = new TextDecoder("utf-8");

          let totalWritten = 0;

          for (let i = 0; i < iovs_len; i++) {
            const base = view.getUint32(iovs_ptr + i * 8, true);
            const len = view.getUint32(iovs_ptr + i * 8 + 4, true);
            const chunk = mem.subarray(base, base + len);
            const text = decoder.decode(chunk);
            totalWritten += len;

            if (fdBuffers[fd] !== undefined) {
              fdBuffers[fd] += text;
            } else {
              console.warn(`fd_write: unknown fd ${fd}`);
            }
          }

          view.setUint32(nwritten_ptr, totalWritten, true);
          scheduleFlush();

          return 0;
        },
        random_get(buf_ptr, buf_len) {
          return 52;
        },
      },
    });

    wasm = instance.instance.exports;
    memory = wasm.memory;

    if (wasm.viz_create_context) wasm.viz_create_context();

    self.postMessage({ type: "ready" });
    return;
  }

  if (!wasm || !memory) {
    self.postMessage({ type: "error", error: "WASM not ready" });
    return;
  }

  const ENCODER = new TextEncoder();
  const DECODER = new TextDecoder();

  async function handleGraphInput(encoded, parseFn) {
    status("Allocating input buffer...");
    const inputLen = encoded.length + 1;
    const inputPtr = wasm.viz_alloc(inputLen);
    if (!inputPtr) {
      self.postMessage({ type: "error", error: "Failed to allocate input buffer" });
      return;
    }

    const inputBuf = new Uint8Array(memory.buffer, inputPtr, inputLen);
    inputBuf.set(encoded);
    inputBuf[encoded.length] = 0;

    status("Parsing input...");
    const graph = parseFn(inputPtr);
    wasm.viz_free(inputPtr, inputLen);

    if (!graph) {
      self.postMessage({ type: "error", error: "Invalid input" });
      return;
    }

    const outLen = OUT_LEN;
    const outPtr = wasm.viz_alloc(outLen);
    if (!outPtr) {
      wasm.viz_free_graph(graph);
      self.postMessage({ type: "error", error: "Failed to allocate output buffer" });
      return;
    }

    status("Laying out graph...");
    const layout_res = wasm.viz_layout_graph(graph);
    if (layout_res != 0) {
      wasm.viz_free_graph(graph);
      wasm.viz_free(outPtr, outLen);
      self.postMessage({ type: "error", error: `Layout error: ${layout_res}` });
      return;
    }

    status("Rendering SVG...");
    const written = wasm.viz_graph_to_svg(graph, outPtr, outLen);
    wasm.viz_free_graph(graph);

    if (written === 0) {
      wasm.viz_free(outPtr, outLen);
      self.postMessage({ type: "error", error: "Failed to render SVG" });
      return;
    }

    status("Done");
    const svgBytes = new Uint8Array(memory.buffer, outPtr, written);
    const svg = DECODER.decode(svgBytes);
    wasm.viz_free(outPtr, outLen);

    self.postMessage({ type: "svg", svg });
  }

  if (type === "dot") {
    status("Encoding DOT string...");
    const encoded = ENCODER.encode(dot);
    handleGraphInput(encoded, wasm.viz_dot_to_graph);
    return;
  }

  if (type === "json") {
    status("Encoding JSON graph...");
    const encoded = ENCODER.encode(json);
    handleGraphInput(encoded, wasm.viz_json_to_graph);
    return;
  }
};
