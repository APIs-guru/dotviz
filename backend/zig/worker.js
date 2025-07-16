let wasm;
let memory;

self.onmessage = async function (e) {
  const { type, dot, json } = e.data;

  if (type === "init") {
    const response = await fetch("zig-out/bin/dotviz.wasm");
    const buffer = await response.arrayBuffer();
    const instance = await WebAssembly.instantiate(buffer, {
      env: {
        __indirect_function_table: new WebAssembly.Table({ initial: 0, element: "anyfunc" }),
        __stack_pointer: new WebAssembly.Global({ value: "i32", mutable: true }, 1024),
      },
      wasi_snapshot_preview1: {
        proc_exit() {},
        args_sizes_get() {},
        environ_get() {},
        environ_sizes_get() {},
        clock_time_get() {},
        fd_fdstat_get() {},
        fd_close() {},
        args_get() {},
        fd_fdstat_set_flags() {},
        fd_filestat_get() {},
        fd_prestat_get() {},
        fd_prestat_dir_name() {},
        fd_read() {},
        fd_seek() {},
        path_filestat_get() {},
        path_open() {},
        fd_write() {},
        random_get() {},
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

  if (type === "dot") {
    const encoded = new TextEncoder().encode(dot);
    const ptr = 1024;
    new Uint8Array(memory.buffer).set(encoded, ptr);
    const svgPtr = wasm.viz_dot_to_svg(ptr, encoded.length);
    const len = wasm.viz_svg_len();
    const svgBytes = new Uint8Array(memory.buffer).slice(svgPtr, svgPtr + len);
    const svg = new TextDecoder().decode(svgBytes);
    self.postMessage({ type: "svg", svg });
    return;
  }

  if (type === "json") {
    const encoded = new TextEncoder().encode(json);
    const ptr = 4096;
    new Uint8Array(memory.buffer).set(encoded, ptr);

    const graph = wasm.viz_json_to_graph(ptr);
    if (!graph) {
      self.postMessage({ type: "error", error: "Invalid JSON" });
      return;
    }

    const svgPtr = wasm.viz_graph_to_svg(graph);
    const len = wasm.viz_svg_len();
    const svgBytes = new Uint8Array(memory.buffer).slice(svgPtr, svgPtr + len);
    const svg = new TextDecoder().decode(svgBytes);
    self.postMessage({ type: "svg", svg });
    wasm.viz_free_graph(graph);
    return;
  }
};
