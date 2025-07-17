let wasm;
let memory;
const OUT_LEN = 64 * 1024;

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
