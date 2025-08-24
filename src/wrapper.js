import { parseAgerrMessages, parseStderrMessages } from './errors.js';

/**
 * @param {WebAssembly.WebAssemblyInstantiatedSource} module 
 */
export function renderInput(module, input, formats, options) {
  const { exports: wasm } = module.instance;
  let graphPointer, contextPointer, resultPointer;

  try {
    // this.stderrMessages = [];

    if (typeof input === 'string') {
      graphPointer = readStringInput(module, input);
    } else if (typeof input === 'object') {
      graphPointer = readObjectInput(module, input);
    } else {
      throw new TypeError('input must be a string or object');
    }

    if (graphPointer === 0) {
      return {
        status: 'failure',
        output: undefined,
        errors: parseErrorMessages(module),
      };
    }

    setDefaultAttributes(module, graphPointer, options);
    wasm.viz_set_y_invert(options.yInvert); //FIXME: test
    wasm.viz_set_reduce(options.reduce); //FIXME: test

    contextPointer = wasm.viz_create_context();

    wasm.viz_reset_errors();

    let layoutError = wasm.viz_layout(contextPointer, graphPointer);

    if (layoutError !== 0) {
      return {
        status: 'failure',
        output: undefined,
        errors: parseErrorMessages(module),
      };
    }

    const resultPointer = wasm.viz_render(contextPointer, graphPointer);
    if (resultPointer === 0) {
      return {
        status: 'failure',
        output: undefined,
        errors: parseErrorMessages(module),
      };
    }

    const output = readCString(resultPointer);
    wasm.viz_free_svg(resultPointer);
    resultPointer = 0;

    return {
      status: 'success',
      output: output,
      errors: parseErrorMessages(module),
    };
  } catch (error) {
    if (/^exit\(\d+\)/.test(error)) { // FIXME: check if needed
      return {
        status: 'failure',
        output: undefined,
        errors: parseErrorMessages(module),
      };
    } else {
      throw error;
    }
  } finally {
    if (contextPointer && graphPointer) {
      wasm.viz_free_layout(contextPointer, graphPointer);
    }

    if (graphPointer) {
      wasm.viz_free_graph(graphPointer);
    }

    if (contextPointer) {
      wasm.viz_free_context(contextPointer);
    }
  }
}

/**
 * @param {WebAssembly.WebAssemblyInstantiatedSource} module 
 */
function parseErrorMessages(module) {
  return [
    ...parseAgerrMessages(module['agerrMessages']),
    ...parseStderrMessages(module['stderrMessages']),
  ];
}

/**
 * @param {WebAssembly.WebAssemblyInstantiatedSource} module 
 */
function readStringInput(module, src) {
  let srcPointer;

  try {
    const cString = writeCString(src);
    //////////// Finished here!!!!! 
    /// FIXME: can we just put cString into WASM without copy
    const inputPtr = wasm.wasm_alloc(cString);
    const srcLength = module.lengthBytesUTF8(src);

    srcPointer = module.ccall('malloc', 'number', ['number'], [srcLength + 1]);
    module.stringToUTF8(src, srcPointer, srcLength + 1);

    return module.ccall(
      'viz_read_one_graph',
      'number',
      ['number'],
      [srcPointer],
    );
  } finally {
    if (srcPointer) {
      module.ccall('free', 'number', ['number'], [srcPointer]);
    }
  }
}

/**
 * @param {WebAssembly.WebAssemblyInstantiatedSource} module 
 */
function readObjectInput(module, object) {
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
function setDefaultAttributes(module, graphPointer, data) {
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

function readCString(ptr) {
  const buf = new Uint8Array(memory.buffer);
  let end = ptr;
  while (buf[end] !== 0) {
    end++;
  }
  return new TextDecoder("utf-8").decode(buf.subarray(ptr, end));
}

function writeCString(string) {
  return new TextEncoder("utf-8").encode(string + '\0');
}