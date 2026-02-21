import * as VizJSPackage from '@viz-js/viz';
import { describe, expect, it } from 'vitest';

import * as DotVizPackage from '../src/index.ts';

const vizJS = await VizJSPackage.instance();
const dotviz = await DotVizPackage.instance();

function renderString(dot: string): string {
  const dotvizResult = dotviz.renderString(dot);
  const vizJSResult = vizJS.renderString(dot);
  expect(dotvizResult).toStrictEqual(vizJSResult);
  return dotvizResult;
}

describe('Dot language support', () => {
  // don't wrap result string inside snapshots
  expect.addSnapshotSerializer({
    serialize: (val) => val as string,
    test: (val: unknown) => typeof val == 'string',
  });

  it('empty graph', () => {
    const result = renderString('graph {}');
    expect(result).toMatchInlineSnapshot(`
      graph {
      	graph [bb="0,0,0,0"];
      	node [label="\\N"];
      }
    `);

    const directedResult = renderString('digraph {}');
    expect(directedResult).toMatchInlineSnapshot(`
      digraph {
      	graph [bb="0,0,0,0"];
      	node [label="\\N"];
      }
    `);
  });

  it('strict empty graph', () => {
    const result = renderString('strict graph {}');
    expect(result).toMatchInlineSnapshot(`
      strict graph {
      	graph [bb="0,0,0,0"];
      	node [label="\\N"];
      }
    `);
  });

  it('named graph', () => {
    const result = renderString('graph test {}');
    expect(result).toMatchInlineSnapshot(`
      graph test {
      	graph [bb="0,0,0,0"];
      	node [label="\\N"];
      }
    `);

    const stringResult = renderString('graph "test" {}');
    expect(stringResult).toStrictEqual(result);

    const keywordResult = renderString('graph "graph" {}');
    expect(keywordResult).toMatchInlineSnapshot(`
      graph "graph" {
      	graph [bb="0,0,0,0"];
      	node [label="\\N"];
      }
    `);
  });

  it('empty attributes', () => {
    const result = renderString(`
      graph {
        graph []
        node []
        edge []
      }
    `);
    expect(result).toMatchInlineSnapshot(`
      graph {
      	graph [bb="0,0,0,0"];
      	node [label="\\N"];
      }
    `);

    const multipleResult = renderString(`
      graph {
        graph [][]
        node [][]
        edge [][]
      }
    `);
    expect(multipleResult).toStrictEqual(result);
  });

  it('global attributes', () => {
    const result = renderString(`
      graph {
        graph [a=valueA]
        node [b=valueB]
        edge [c=valueC]
      }
    `);

    expect(result).toMatchInlineSnapshot(`
      graph {
      	graph [a=valueA,
      		bb="0,0,0,0"
      	];
      	node [b=valueB,
      		label="\\N"
      	];
      	edge [c=valueC];
      }
    `);

    const mergeResult = renderString(`
      graph {
        graph [a=badA a=valueA]
        node [b=badB b=valueB]
        edge [c=badC c=valueC]
      }
    `);
    expect(mergeResult).toStrictEqual(result);
    const mergeListsResult = renderString(`
      graph {
        graph [a=badA][a=valueA]
        node [b=badB][b=valueB]
        edge [c=badC][c=valueC]
      }
    `);
    expect(mergeListsResult).toStrictEqual(result);
  });

  it('global graph attributes shorthand', () => {
    const result = renderString(`
      graph {
        a=valueA
      }
    `);

    expect(result).toMatchInlineSnapshot(`
      graph {
      	graph [a=valueA,
      		bb="0,0,0,0"
      	];
      	node [label="\\N"];
      }
    `);

    const mergeResult = renderString(`
      graph {
        a=badA
        a=valueA
      }
    `);
    expect(mergeResult).toStrictEqual(result);
  });

  it('single edge', () => {
    const result = renderString(`
      graph {
        b -- a
      }
    `);

    expect(result).toMatchInlineSnapshot(`
      graph {
      	graph [bb="0,0,54,108"];
      	node [label="\\N"];
      	b	[height=0.5,
      		pos="27,90",
      		width=0.75];
      	a	[height=0.5,
      		pos="27,18",
      		width=0.75];
      	b -- a	[pos="27,71.697 27,60.846 27,46.917 27,36.104"];
      }
    `);

    const directedResult = renderString(`
      digraph {
        b -> a
      }
    `);

    expect(directedResult).toMatchInlineSnapshot(`
      digraph {
      	graph [bb="0,0,54,108"];
      	node [label="\\N"];
      	b	[height=0.5,
      		pos="27,90",
      		width=0.75];
      	a	[height=0.5,
      		pos="27,18",
      		width=0.75];
      	b -> a	[pos="e,27,36.104 27,71.697 27,64.407 27,55.726 27,47.536"];
      }
    `);
  });

  it('two edges', () => {
    const result = renderString(`
      graph {
        b -- a
        a -- b
      }
    `);

    expect(result).toMatchInlineSnapshot(`
      graph {
      	graph [bb="0,0,54,108"];
      	node [label="\\N"];
      	b	[height=0.5,
      		pos="27,90",
      		width=0.75];
      	a	[height=0.5,
      		pos="27,18",
      		width=0.75];
      	b -- a	[pos="21.122,72.055 19.954,61.049 19.959,46.764 21.138,35.789"];
      	a -- b	[pos="32.862,35.789 34.041,46.764 34.046,61.049 32.878,72.055"];
      }
    `);

    const directedResult = renderString(`
      digraph {
        b -> a
        a -> b
      }
    `);

    expect(directedResult).toMatchInlineSnapshot(`
      digraph {
      	graph [bb="0,0,54,108"];
      	node [label="\\N"];
      	b	[height=0.5,
      		pos="27,90",
      		width=0.75];
      	a	[height=0.5,
      		pos="27,18",
      		width=0.75];
      	b -> a	[pos="e,21.138,35.789 21.122,72.055 20.328,64.574 20.076,55.579 20.367,47.137"];
      	a -> b	[pos="e,32.878,72.055 32.862,35.789 33.663,43.248 33.922,52.237 33.639,60.686"];
      }
    `);
  });
});
