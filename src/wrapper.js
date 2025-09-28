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

/**
 * @param {WebAssembly.WebAssemblyInstantiatedSource} module
 */
export function readObjectInput(module, object) {
  const graphPointer = module.ccall(
    'viz_create_graph',
    'number',
    ['string', 'number', 'number'],
    [object.name, object.directed ?? true, object.strict ?? false],
  );

  readGraph(module, graphPointer, object);

  return graphPointer;
}

/**
 * @param {WebAssembly.WebAssemblyInstantiatedSource} module
 */
function readGraph(module, graphPointer, graphData) {
  setDefaultAttributes(module, graphPointer, graphData);

  if (graphData.nodes) {
    for (const nodeData of graphData.nodes) {
      const nodePointer = module.ccall(
        'viz_add_node',
        'number',
        ['number', 'string'],
        [graphPointer, String(nodeData.name)],
      );

      if (nodeData.attributes) {
        setAttributes(module, graphPointer, nodePointer, nodeData.attributes);
      }
    }
  }

  if (graphData.edges) {
    for (const edgeData of graphData.edges) {
      const edgePointer = module.ccall(
        'viz_add_edge',
        'number',
        ['number', 'string', 'string'],
        [graphPointer, String(edgeData.tail), String(edgeData.head)],
      );

      if (edgeData.attributes) {
        setAttributes(module, graphPointer, edgePointer, edgeData.attributes);
      }
    }
  }

  if (graphData.subgraphs) {
    for (const subgraphData of graphData.subgraphs) {
      const subgraphPointer = module.ccall(
        'viz_add_subgraph',
        'number',
        ['number', 'string'],
        [graphPointer, String(subgraphData.name)],
      );

      readGraph(module, subgraphPointer, subgraphData);
    }
  }
}

/**
 * @param {WebAssembly.WebAssemblyInstantiatedSource} module
 */
export function setDefaultAttributes(module, graphPointer, data) {
  if (data.graphAttributes) {
    for (const [name, value] of Object.entries(data.graphAttributes)) {
      withStringPointer(module, graphPointer, value, (stringPointer) => {
        module.ccall(
          'viz_set_default_graph_attribute',
          'number',
          ['number', 'string', 'number'],
          [graphPointer, name, stringPointer],
        );
      });
    }
  }

  if (data.nodeAttributes) {
    for (const [name, value] of Object.entries(data.nodeAttributes)) {
      withStringPointer(module, graphPointer, value, (stringPointer) => {
        module.ccall(
          'viz_set_default_node_attribute',
          'number',
          ['number', 'string', 'number'],
          [graphPointer, name, stringPointer],
        );
      });
    }
  }

  if (data.edgeAttributes) {
    for (const [name, value] of Object.entries(data.edgeAttributes)) {
      withStringPointer(module, graphPointer, value, (stringPointer) => {
        module.ccall(
          'viz_set_default_edge_attribute',
          'number',
          ['number', 'string', 'number'],
          [graphPointer, name, stringPointer],
        );
      });
    }
  }
}

/**
 * @param {WebAssembly.WebAssemblyInstantiatedSource} module
 */
function setAttributes(module, graphPointer, objectPointer, attributes) {
  for (const [key, value] of Object.entries(attributes)) {
    withStringPointer(module, graphPointer, value, (stringPointer) => {
      module.ccall(
        'viz_set_attribute',
        'number',
        ['number', 'string', 'number'],
        [objectPointer, key, stringPointer],
      );
    });
  }
}

/**
 * @param {WebAssembly.WebAssemblyInstantiatedSource} module
 */
function withStringPointer(module, graphPointer, value, callbackFn) {
  const isHTML = typeof value === 'object' && 'html' in value;
  const stringPointer = module.ccall(
    isHTML ? 'viz_string_dup_html' : 'viz_string_dup',
    'number',
    ['number', 'string'],
    [graphPointer, isHTML ? String(value.html) : String(value)],
  );

  if (stringPointer == 0) {
    throw new Error("couldn't dup string");
  }

  callbackFn(stringPointer);

  module.ccall(
    isHTML ? 'viz_string_free_html' : 'viz_string_free',
    'number',
    ['number', 'number'],
    [graphPointer, stringPointer],
  );
}

function writeCString(string) {
  return new TextEncoder('utf8').encode(string + '\0');
}
