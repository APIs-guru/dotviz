import assert from 'node:assert/strict';
import { describe, it } from 'node:test';

import * as VizJSPackage from '@viz-js/viz';

import type { Graph } from '../src/index.ts';
import * as DotVizPackage from '../src/index.ts';

const vizJS = await VizJSPackage.instance();
const dotviz = await DotVizPackage.instance();

function renderString(dot: string): string {
  const vizJSResult = vizJS.renderString(dot);
  const dotvizResult = dotviz.renderString(dot);
  assert.deepStrictEqual(dotvizResult, vizJSResult);
  return dotvizResult;
}

function emptyGraphResult(graph: Graph = {}) {
  const strict = graph.strict ? 'strict ' : '';
  return zigDedent`
    \\${strict}graph {
    \\\tgraph [bb="0,0,0,0"];
    \\\tnode [label="\\N"];
    \\}
    \\`;
}

describe('Dot language support', () => {
  void it('empty graph', () => {
    const result = renderString('graph {}');
    assert.deepStrictEqual(result, emptyGraphResult());
  });

  void it('strict empty graph', () => {
    const result = renderString('strict graph {}');
    assert.deepStrictEqual(result, emptyGraphResult({ strict: true }));
  });

  void it('empty attributes', () => {
    const result = renderString(zigDedent`
      \\graph {
      \\  graph []
      \\  node []
      \\  edge []
      \\}
    `);
    assert.deepStrictEqual(result, emptyGraphResult());

    const multipleResult = renderString(zigDedent`
      \\graph {
      \\  graph [][]
      \\  node [][]
      \\  edge [][]
      \\}
    `);
    assert.deepStrictEqual(multipleResult, emptyGraphResult());
  });

  void it('multiple empty attributes', () => {
    const result = renderString(zigDedent`
      \\graph {
      \\  graph [a=valueA]
      \\  node [b=valueB]
      \\  edge [c=valueC]
      \\}
    `);

    assert.deepStrictEqual(
      result,
      zigDedent`
        \\graph {
        \\\tgraph [a=valueA,
        \\\t\tbb="0,0,0,0"
        \\\t];
        \\\tnode [b=valueB,
        \\\t\tlabel="\\N"
        \\\t];
        \\\tedge [c=valueC];
        \\}
        \\`,
    );

    const mergeResult = renderString(zigDedent`
      \\graph {
      \\  graph [a=badA a=valueA]
      \\  node [b=badB b=valueB]
      \\  edge [c=badC c=valueC]
      \\}
    `);
    assert.deepStrictEqual(mergeResult, result);
    const mergeListsResult = renderString(zigDedent`
      \\graph {
      \\  graph [a=badA][a=valueA]
      \\  node [b=badB][b=valueB]
      \\  edge [c=badC][c=valueC]
      \\}
    `);
    assert.deepStrictEqual(mergeListsResult, result);
  });
});

function zigDedent(
  strings: readonly string[],
  ...values: readonly string[]
): string {
  let str = strings[0];

  for (let i = 1; i < strings.length; ++i) {
    str += values[i - 1] + strings[i]; // interpolation
  }

  const result: string[] = [];
  for (const line of str.split('\n')) {
    const idx = line.indexOf('\\');
    if (idx !== -1) {
      result.push(line.slice(idx + 1));
    }
  }
  return result.join('\n');
}
