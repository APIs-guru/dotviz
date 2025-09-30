/**
 * @param {WebAssembly.WebAssemblyInstantiatedSource} module
 */
export function readStringInput(module, src) {
  const { exports: wasm } = module.instance;
  let inputBuf;

  try {
    const cString = writeCString(src);
    /// FIXME: can we just put cString into WASM without copy
    const inputPtr = wasm.wasm_alloc(cString.length);
    inputBuf = new Uint8Array(wasm.memory.buffer, inputPtr, cString.length);
    inputBuf.set(cString);

    return wasm.viz_read_one_graph_from_dot(inputBuf.byteOffset);
  } finally {
    if (inputBuf) {
      wasm.wasm_free(inputBuf.byteOffset, inputBuf.length);
    }
  }
}

function writeCString(string) {
  return new TextEncoder('utf8').encode(string + '\0');
}
