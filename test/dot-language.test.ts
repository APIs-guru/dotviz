import { describe, expect, it } from 'vitest';

import { dotvizInstance, type RenderResult } from '../src/index.ts';
import { dedent } from './util/dedent.ts';
import {
  expectDot,
  expectDotWithWarnings,
  expectFailureResult,
} from './util/render-result.ts';
import { useVizJSInstance } from './util/use-viz-js.ts';

const vizJS = await useVizJSInstance();
const dotviz = await dotvizInstance();

function renderDotAndCompareWithVizJS(dot: string): RenderResult {
  const dotvizResult = dotviz.renderDot(dot);
  const vizJSResult = vizJS.render(dot);
  expect({
    status: dotvizResult.status,
    output: dotvizResult.output?.dot,
    errors: dotvizResult.diagnostics.map((err) => ({
      level: err.level,
      message: err.message,
    })),
  }).toStrictEqual(vizJSResult);
  return dotvizResult;
}

function checkAttributeValue(
  input: string,
  expected: string,
): { inputDot: string; expectedDot: string } {
  const inputDot = `graph { test = ${input} } `;
  const expectedDot = dedent`
    graph {
    	graph [bb="0,0,0,0",
    		test=${expected}
    	];
    	node [label="\\N"];
    }
  `;
  return { inputDot, expectedDot };
}

describe('dot language support', () => {
  it('empty graph', () => {
    const result = renderDotAndCompareWithVizJS('graph {}');
    expectDot(result).toMatchRawStringInlineSnapshot(`
      graph {
      	graph [bb="0,0,0,0"];
      	node [label="\\N"];
      }
    `);

    const directedResult = renderDotAndCompareWithVizJS('digraph {}');
    expectDot(directedResult).toMatchRawStringInlineSnapshot(`
      digraph {
      	graph [bb="0,0,0,0"];
      	node [label="\\N"];
      }
    `);
  });

  it('strict empty graph', () => {
    const result = renderDotAndCompareWithVizJS('strict graph {}');
    expectDot(result).toMatchRawStringInlineSnapshot(`
      strict graph {
      	graph [bb="0,0,0,0"];
      	node [label="\\N"];
      }
    `);
  });

  it('named graph', () => {
    const result = renderDotAndCompareWithVizJS('graph test {}');
    expectDot(result).toMatchRawStringInlineSnapshot(`
      graph test {
      	graph [bb="0,0,0,0"];
      	node [label="\\N"];
      }
    `);

    const stringResult = renderDotAndCompareWithVizJS('graph "test" {}');
    expect(stringResult).toStrictEqual(result);

    const keywordResult = renderDotAndCompareWithVizJS('graph "graph" {}');
    expectDot(keywordResult).toMatchRawStringInlineSnapshot(`
      graph "graph" {
      	graph [bb="0,0,0,0"];
      	node [label="\\N"];
      }
    `);
  });

  it('ignores whitespace and comments', () => {
    const result = renderDotAndCompareWithVizJS(`
      graph
      \n\r\t\uFEFF
      # comment with # in the middle
      // another one with / and // in the middle
      /* start comment
         /* and * in the middle
         end comment */
      {}
    `);
    expectDot(result).toMatchRawStringInlineSnapshot(`
      graph {
      	graph [bb="0,0,0,0"];
      	node [label="\\N"];
      }
    `);
  });

  it('empty attributes', () => {
    const result = renderDotAndCompareWithVizJS(`
      graph {
        graph []
        node []
        edge []
      }
    `);
    expectDot(result).toMatchRawStringInlineSnapshot(`
      graph {
      	graph [bb="0,0,0,0"];
      	node [label="\\N"];
      }
    `);

    const multipleResult = renderDotAndCompareWithVizJS(`
      graph {
        graph [][]
        node [][]
        edge [][]
      }
    `);
    expect(multipleResult).toStrictEqual(result);
  });

  it('global attributes', () => {
    const result = renderDotAndCompareWithVizJS(`
      graph {
        graph [a=valueA]
        node [b=valueB]
        edge [c=valueC]
      }
    `);

    expectDot(result).toMatchRawStringInlineSnapshot(`
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

    const mergeResult = renderDotAndCompareWithVizJS(`
      graph {
        graph [a=badA a=valueA]
        node [b=badB b=valueB]
        edge [c=badC c=valueC]
      }
    `);
    expect(mergeResult).toStrictEqual(result);
    const mergeListsResult = renderDotAndCompareWithVizJS(`
      graph {
        graph [a=badA][a=valueA]
        node [b=badB][b=valueB]
        edge [c=badC][c=valueC]
      }
    `);
    expect(mergeListsResult).toStrictEqual(result);
  });

  it('empty strings as global attributes', () => {
    const result = renderDotAndCompareWithVizJS(`
      graph {
        graph [a=""]
        node [b=""]
        edge [c=""]
      }
    `);
    expectDot(result).toMatchRawStringInlineSnapshot(`
      graph {
      	graph [a="",
      		bb="0,0,0,0"
      	];
      	node [b="",
      		label="\\N"
      	];
      	edge [c=""];
      }
    `);
  });

  describe('various values as attributes', () => {
    it.for([
      `<>`,
      `<<>>`,
      `<ab>`,
      `a`,
      `0`,
      `-0`,
      `.0`,
      `""`,
      `"\n"`,
      // `\` + `"` → `"` (backslash consumed)
      String.raw`"\""`,
      String.raw`"\"a"`,
      String.raw`"\\\""`,
      String.raw`"\\a\\"`,
      String.raw`"\\\\"`, // `\\` → both backslashes kept (not reduced to one)
      String.raw`"\n\t\r"`, // `\` + letter → both stored verbatim (not C-style escapes)
      `"\\\\\na"`, // `\\<LF>` → `\\` + literal LF (NOT a continuation)
      `"\\\\\\\\\na"`, // `\\\\<LF>` → all four backslashes + literal LF
    ])('value $0 stays that same', (input) => {
      const { inputDot, expectedDot } = checkAttributeValue(input, input);
      const result = renderDotAndCompareWithVizJS(inputDot);
      expect(result.output?.dot?.trimEnd()).toStrictEqual(expectedDot);
    });

    it.for([
      [`"a"`, `a`],
      [`"a" + /* empty string */ "" + "b"`, `ab`],
      [`"\\\n"`, `""`], // `\<LF>` → nothing (line continuation)
      [`"\\\\\\\na"`, String.raw`"\\a"`], // `\\\<LF>` → `\\` stored, continuation on 3rd backslash
    ])('value $0 is correctly transformed into $1', ([input, expected]) => {
      const { inputDot, expectedDot } = checkAttributeValue(input, expected);
      const result = renderDotAndCompareWithVizJS(inputDot);
      expect(result.output?.dot?.trimEnd()).toStrictEqual(expectedDot);
    });
  });

  describe('handle Windows-style line endings in quoted strings (dotviz only)', () => {
    it.for([
      `"a\\\rb"`, // `\<CR>` alone (no LF) → verbatim `\`+CR pair, not a continuation
      `"a\\\r"`, // `\<CR>` alone before closing quote → verbatim `\`+CR, quote still closes string
    ])('value $0 stays that same', (input) => {
      const { inputDot, expectedDot } = checkAttributeValue(input, input);
      const result = dotviz.renderDot(inputDot);
      expect(result.output?.dot?.trimEnd()).toStrictEqual(expectedDot);
    });

    it.for([
      [`"\\\r\n"`, `""`], // `\<CR><LF>` → continuation (same as `\<LF>`)
      [`"a\\\r\nb"`, `ab`], // `\<CR><LF>` → continuation, text on both sides kept
      [`"\\\\\\\r\n"`, String.raw`"\\"`], // `\\\<CR><LF>` → `\\` stored, continuation on 3rd backslash
      [`"\\\\\\\r\na"`, String.raw`"\\a"`], // `\\\<CR><LF>` with trailing content → same as above
    ])('value $0 is correctly transformed into $1', ([input, expected]) => {
      const { inputDot, expectedDot } = checkAttributeValue(input, expected);
      const result = dotviz.renderDot(inputDot);
      expect(result.output?.dot?.trimEnd()).toStrictEqual(expectedDot);
    });
  });

  it('global graph attributes shorthand', () => {
    const result = renderDotAndCompareWithVizJS(`
      graph {
        a=valueA
      }
    `);

    expectDot(result).toMatchRawStringInlineSnapshot(`
      graph {
      	graph [a=valueA,
      		bb="0,0,0,0"
      	];
      	node [label="\\N"];
      }
    `);

    const mergeResult = renderDotAndCompareWithVizJS(`
      graph {
        a=badA
        a=valueA
      }
    `);
    expect(mergeResult).toStrictEqual(result);
  });

  it('apply default attributes values', () => {
    const result = renderDotAndCompareWithVizJS(`
      digraph {
        a
        a -> a
        {
          b
          b -> b
          {}
          node [nodeAttr1=1]
          edge [edgeAttr1=1]
          graph[graphAttr1=1]
        }
        node [nodeAttr2=2]
        edge [edgeAttr2=2]
        graph [graphAttr2=2]
      }
    `);

    expectDot(result).toMatchRawStringInlineSnapshot(`
      digraph {
      	graph [bb="0,0,162,36",
      		graphAttr2=2
      	];
      	node [label="\\N",
      		nodeAttr2=2
      	];
      	edge [edgeAttr2=2];
      	{
      		graph [graphAttr1=1,
      			graphAttr2=""
      		];
      		node [nodeAttr1=1];
      		edge [edgeAttr1=1];
      		{
      			graph [graphAttr1=""];
      		}
      		b	[height=0.5,
      			nodeAttr1="",
      			nodeAttr2="",
      			pos="117,18",
      			width=0.75];
      		b -> b	[edgeAttr1="",
      			edgeAttr2="",
      			pos="e,142.44,11.309 142.44,24.691 153.03,25.152 162,22.922 162,18 162,15.001 158.67,13.001 153.67,12.001"];
      	}
      	a	[height=0.5,
      		nodeAttr2="",
      		pos="27,18",
      		width=0.75];
      	a -> a	[edgeAttr2="",
      		pos="e,52.443,11.309 52.443,24.691 63.028,25.152 72,22.922 72,18 72,15.001 68.668,13.001 63.67,12.001"];
      }
    `);
  });

  it('attributes in options override attributes in dot', () => {
    const dot = `
      digraph {
        graph [ testGraph=valueGraphBad ]
        node [ testNode=valueNodeBad ]
        edge [ shape=valueEdgeBad]

        // check what attributes are applied to:
        {}
        a
        a -> a
      }
    `;
    const overrideAttributes = {
      graphAttributes: { testGraph: 'valueGraph' },
      nodeAttributes: { testNode: 'valueNode' },
      edgeAttributes: { shape: 'valueEdge' },
    };
    const result = dotviz.renderDot(dot, { overrideAttributes });

    expectDot(result).toMatchRawStringInlineSnapshot(`
      digraph {
      	graph [bb="0,0,72,36",
      		testGraph=valueGraph
      	];
      	node [label="\\N",
      		testNode=valueNode
      	];
      	edge [shape=valueEdge];
      	{
      	}
      	a	[height=0.5,
      		pos="27,18",
      		width=0.75];
      	a -> a	[pos="e,52.443,11.309 52.443,24.691 63.028,25.152 72,22.922 72,18 72,15.001 68.668,13.001 63.67,12.001"];
      }

    `);
  });

  it('single edge', () => {
    const result = renderDotAndCompareWithVizJS(`
      graph {
        b -- a
      }
    `);

    expectDot(result).toMatchRawStringInlineSnapshot(`
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

    const directedResult = renderDotAndCompareWithVizJS(`
      digraph {
        b -> a
      }
    `);

    expectDot(directedResult).toMatchRawStringInlineSnapshot(`
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
    const result = renderDotAndCompareWithVizJS(`
      graph {
        b -- a
        a -- b
      }
    `);

    expectDot(result).toMatchRawStringInlineSnapshot(`
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

    const directedResult = renderDotAndCompareWithVizJS(`
      digraph {
        b -> a
        a -> b
      }
    `);

    expectDot(directedResult).toMatchRawStringInlineSnapshot(`
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

  it('chain of edges', () => {
    const result = renderDotAndCompareWithVizJS(`
      graph {
        c -- b -- a [valueA=a]
      }
    `);

    expectDot(result).toMatchRawStringInlineSnapshot(`
      graph {
      	graph [bb="0,0,54,180"];
      	node [label="\\N"];
      	c	[height=0.5,
      		pos="27,162",
      		width=0.75];
      	b	[height=0.5,
      		pos="27,90",
      		width=0.75];
      	c -- b	[pos="27,143.7 27,132.85 27,118.92 27,108.1",
      		valueA=a];
      	a	[height=0.5,
      		pos="27,18",
      		width=0.75];
      	b -- a	[pos="27,71.697 27,60.846 27,46.917 27,36.104",
      		valueA=a];
      }
    `);

    const directedResult = renderDotAndCompareWithVizJS(`
      digraph {
        c -> b -> a [valueA=a]
      }
    `);

    expectDot(directedResult).toMatchRawStringInlineSnapshot(`
      digraph {
      	graph [bb="0,0,54,180"];
      	node [label="\\N"];
      	c	[height=0.5,
      		pos="27,162",
      		width=0.75];
      	b	[height=0.5,
      		pos="27,90",
      		width=0.75];
      	c -> b	[pos="e,27,108.1 27,143.7 27,136.41 27,127.73 27,119.54",
      		valueA=a];
      	a	[height=0.5,
      		pos="27,18",
      		width=0.75];
      	b -> a	[pos="e,27,36.104 27,71.697 27,64.407 27,55.726 27,47.536",
      		valueA=a];
      }
    `);
  });

  it('chain of edges with node lists', () => {
    const result = renderDotAndCompareWithVizJS(`
      graph {
        a,b -- c,d -- e,f [valueA=a]
      }
    `);

    expectDot(result).toMatchRawStringInlineSnapshot(`
      graph {
      	graph [bb="0,0,126,180"];
      	node [label="\\N"];
      	a	[height=0.5,
      		pos="27,162",
      		width=0.75];
      	c	[height=0.5,
      		pos="27,90",
      		width=0.75];
      	a -- c	[pos="27,143.7 27,132.85 27,118.92 27,108.1",
      		valueA=a];
      	d	[height=0.5,
      		pos="99,90",
      		width=0.75];
      	a -- d	[pos="41.918,146.5 54.275,134.48 71.749,117.49 84.101,105.49",
      		valueA=a];
      	b	[height=0.5,
      		pos="99,162",
      		width=0.75];
      	b -- c	[pos="84.082,146.5 71.725,134.48 54.251,117.49 41.899,105.49",
      		valueA=a];
      	b -- d	[pos="99,143.7 99,132.85 99,118.92 99,108.1",
      		valueA=a];
      	e	[height=0.5,
      		pos="27,18",
      		width=0.75];
      	c -- e	[pos="27,71.697 27,60.846 27,46.917 27,36.104",
      		valueA=a];
      	f	[height=0.5,
      		pos="99,18",
      		width=0.75];
      	c -- f	[pos="41.918,74.496 54.275,62.482 71.749,45.494 84.101,33.485",
      		valueA=a];
      	d -- e	[pos="84.082,74.496 71.725,62.482 54.251,45.494 41.899,33.485",
      		valueA=a];
      	d -- f	[pos="99,71.697 99,60.846 99,46.917 99,36.104",
      		valueA=a];
      }
    `);
  });

  it('empty strings as subgraph attributes', () => {
    const result = renderDotAndCompareWithVizJS(`
        graph {
         	node [a=""];
         	{ node [a=""] }
        }
      `);
    expectDot(result).toMatchRawStringInlineSnapshot(`
        graph {
        	graph [bb="0,0,0,0"];
        	node [a="",
        		label="\\N"
        	];
        	{
        	}
        }
      `);
  });

  it('merge top-level subgraphs with the same name', () => {
    const result = renderDotAndCompareWithVizJS(`
      graph {
        subgraph a { a1 }
        subgraph b { b1 }
        subgraph a { a2 }
      }
    `);

    expectDot(result).toMatchRawStringInlineSnapshot(`
      graph {
      	graph [bb="0,0,198,36"];
      	node [label="\\N"];
      	subgraph a {
      		a1	[height=0.5,
      			pos="27,18",
      			width=0.75];
      		a2	[height=0.5,
      			pos="171,18",
      			width=0.75];
      	}
      	subgraph b {
      		b1	[height=0.5,
      			pos="99,18",
      			width=0.75];
      	}
      }
    `);
  });

  it('merge nested subgraphs with the same name', () => {
    const result = renderDotAndCompareWithVizJS(`
      graph {
        {
          subgraph a { a1 }
          subgraph b { b1 }
          subgraph a { a2 }
        }
      }
    `);

    expectDot(result).toMatchRawStringInlineSnapshot(`
      graph {
      	graph [bb="0,0,198,36"];
      	node [label="\\N"];
      	{
      		subgraph a {
      			a1	[height=0.5,
      				pos="27,18",
      				width=0.75];
      			a2	[height=0.5,
      				pos="171,18",
      				width=0.75];
      		}
      		subgraph b {
      			b1	[height=0.5,
      				pos="99,18",
      				width=0.75];
      		}
      	}
      }
    `);
  });

  it('change edge attributes inside subgraph', () => {
    const result = renderDotAndCompareWithVizJS(`
        digraph {
          {
            a->b
            edge [color=red]
            b->a
          }
        }
      `);
    expectDot(result).toMatchRawStringInlineSnapshot(`
      digraph {
      	graph [bb="0,0,54,108"];
      	node [label="\\N"];
      	{
      		edge [color=red];
      		a	[height=0.5,
      			pos="27,90",
      			width=0.75];
      		b	[height=0.5,
      			pos="27,18",
      			width=0.75];
      		a -> b	[color="",
      			pos="e,21.138,35.789 21.122,72.055 20.328,64.574 20.076,55.579 20.367,47.137"];
      		b -> a	[pos="e,32.878,72.055 32.862,35.789 33.663,43.248 33.922,52.237 33.639,60.686"];
      	}
      }
    `);
  });

  it('connect nodes in subgraphs with edges', () => {
    const result = renderDotAndCompareWithVizJS(`
      digraph {
        subgraph tails { a b } -> subgraph heads { c d }
      }
    `);
    expectDot(result).toMatchRawStringInlineSnapshot(`
      digraph {
      	graph [bb="0,0,126,108"];
      	node [label="\\N"];
      	subgraph tails {
      		a	[height=0.5,
      			pos="27,90",
      			width=0.75];
      		b	[height=0.5,
      			pos="99,90",
      			width=0.75];
      	}
      	subgraph heads {
      		c	[height=0.5,
      			pos="27,18",
      			width=0.75];
      		d	[height=0.5,
      			pos="99,18",
      			width=0.75];
      	}
      	a -> c	[pos="e,27,36.104 27,71.697 27,64.407 27,55.726 27,47.536"];
      	a -> d	[pos="e,84.101,33.485 41.918,74.496 51.765,64.923 64.861,52.19 76.026,41.336"];
      	b -> c	[pos="e,41.899,33.485 84.082,74.496 74.235,64.923 61.139,52.19 49.974,41.336"];
      	b -> d	[pos="e,99,36.104 99,71.697 99,64.407 99,55.726 99,47.536"];
      }
    `);
  });

  it('edges with different combinations of port names and compass values', () => {
    const result = renderDotAndCompareWithVizJS(`
      digraph {
        a[shape=record label="<p1>|<p2>"]

        a:"" -> a:""       [key=1]
        a:"":_ -> a:"":_   [key=2]
        a:"":w -> a:"":s   [key=3]
        a:p1 -> a:p2       [key=4]
        a:p1:w -> a:p2:s   [key=5]
        a:p1:_ -> a:p2:_   [key=6]
      }
    `);

    const resultWithAttributes = renderDotAndCompareWithVizJS(`
      digraph {
        a[shape=record label="<p1>|<p2>"]

        a -> a [key=1 tailport="", headport=""]
        a -> a [key=2 tailport=":_", headport=":_"]
        a -> a [key=3 tailport=":w", headport=":s"]
        a -> a [key=4 tailport="p1", headport="p2"]
        a -> a [key=5 tailport="p1:w", headport="p2:s"]
        a -> a [key=6 tailport="p1:_", headport="p2:_"]
      }
    `);

    expectDot(result).toMatchRawStringInlineSnapshot(`
      digraph {
      	graph [bb="0,0,126,79.711"];
      	node [label="\\N"];
      	a	[height=0.51389,
      		label="<p1>|<p2>",
      		pos="45,24.211",
      		rects="18,6.211,44.5,42.211 44.5,6.211,72,42.211",
      		shape=record,
      		width=0.75];
      	a -> a	[key=1,
      		pos="e,72.241,21.901 72.241,26.521 82.024,26.571 90,25.801 90,24.211 90,23.317 87.476,22.682 83.527,22.306"];
      	a:"":_ -> a:"":_	[key=2,
      		pos="e,72.198,19.994 72.198,28.428 90.34,29.615 108,28.21 108,24.211 108,21.056 97.005,19.515 83.453,19.589"];
      	a:"":w -> a:"":s	[key=3,
      		pos="e,72.386,18.601 72.386,29.821 98.104,32.729 126,30.859 126,24.211 126,18.523 105.58,16.333 83.583,17.64"];
      	a:p1 -> a:p2	[key=4,
      		pos="e,60.632,42.487 28.868,42.487 29.608,52.285 33.922,61.211 44.75,61.211 51.179,61.211 55.312,58.064 57.764,53.456"];
      	a:p1:w -> a:p2:s	[key=5,
      		pos="e,58.25,6.211 18,24.211 12,33.461 0,33.461 0,15.211 0,0.31158 28.827,-2.4239 47.574,2.1073"];
      	a:p1:_ -> a:p2:_	[key=6,
      		pos="e,63.373,42.689 26.127,42.689 23.981,60.388 28.49,79.711 44.75,79.711 57.58,79.711 63.094,67.68 63.795,53.909"];
      }
    `);
    expect(result).toStrictEqual(resultWithAttributes);
  });

  it('normalize empty string as missing compass point', () => {
    const result = dotviz.renderDot(`
      digraph {
        a[shape=record label="<p1>|<p2>"]

        a:"":"" -> a:"":"" [key=1]
        a:p1:"" -> a:p2:"" [key=2]
      }
    `);

    const resultWithAttributes = dotviz.renderDot(`
      digraph {
        a[shape=record label="<p1>|<p2>"]

        a -> a [key=1 tailport=":", headport=":"]
        a -> a [key=2 tailport="p1:", headport="p2:"]
      }
    `);

    expectDot(result).toMatchRawStringInlineSnapshot(`
      digraph {
      	graph [bb="0,0,72,55.5"];
      	node [label="\\N"];
      	a	[height=0.51389,
      		label="<p1>|<p2>",
      		pos="27,18.5",
      		rects="0,0.5,26.5,36.5 26.5,0.5,54,36.5",
      		shape=record,
      		width=0.75];
      	a -> a	[key=1,
      		pos="e,54.241,11.569 54.241,25.431 64.024,25.58 72,23.27 72,18.5 72,15.817 69.476,13.912 65.527,12.786"];
      	a:p1 -> a:p2	[key=2,
      		pos="e,45.868,36.776 7.6324,36.776 7.8219,46.574 13.215,55.5 26.75,55.5 34.786,55.5 39.953,52.353 42.863,47.745"];
      }
    `);
    expect(result).toStrictEqual(resultWithAttributes);
  });

  it('empty headport and tailport attributes clear edge endpoint ports', () => {
    const result = renderDotAndCompareWithVizJS(`
      digraph {
        a:tail_port -> a:head_port [tailport="", headport=""]
      }
    `);
    expectDot(result).toMatchRawStringInlineSnapshot(`
      digraph {
      	graph [bb="0,0,72,36"];
      	node [label="\\N"];
      	a	[height=0.5,
      		pos="27,18",
      		width=0.75];
      	a -> a	[pos="e,52.443,11.309 52.443,24.691 63.028,25.152 72,22.922 72,18 72,15.001 68.668,13.001 63.67,12.001"];
      }
    `);
  });

  it('strict graph deduplication keeps ports from first edge declaration', () => {
    const result = renderDotAndCompareWithVizJS(`
      strict digraph {
        a -> a:n
        a -> a
      }
    `);
    expectDot(result).toMatchRawStringInlineSnapshot(`
      strict digraph {
      	graph [bb="0,0,72,42.271"];
      	node [label="\\N"];
      	a	[height=0.5,
      		pos="27,18",
      		width=0.75];
      	a -> a:n	[pos="e,27,36.321 52.896,12.188 63.282,12.858 72,17.371 72,27.16 72,41.632 52.95,44.571 37.62,40.614"];
      }
    `);
  });

  it('deduplicate edges with the same key', () => {
    const result = renderDotAndCompareWithVizJS(`
      graph {
        {
          edge [test=<no_key>]
          a -- b
          edge [test=key1]
          a -- b [key = 1]
          edge [test=bad1]
          b -- a [key = 1]
          edge [test=bad2]
          a -- b [key = 1]
          edge [test=key2]
          a -- b [key=2]
        }
      }
    `);
    expectDot(result).toMatchRawStringInlineSnapshot(`
      graph {
      	graph [bb="0,0,54,108"];
      	node [label="\\N"];
      	{
      		edge [test=key2];
      		a	[height=0.5,
      			pos="27,90",
      			width=0.75];
      		b	[height=0.5,
      			pos="27,18",
      			width=0.75];
      		a -- b	[pos="15.56,73.465 12.81,61.865 12.813,46.082 15.57,34.492",
      			test=<no_key>];
      		a -- b	[key=1,
      			pos="27,71.697 27,60.846 27,46.917 27,36.104",
      			test=key1];
      		a -- b	[key=2,
      			pos="38.44,73.465 41.19,61.865 41.187,46.082 38.43,34.492"];
      	}
      }
    `);
  });

  it('deduplicate edges with the same key in directed graph', () => {
    const result = renderDotAndCompareWithVizJS(`
      digraph {
        {
          edge [test=<no_key>]
          a -> b
          edge [test=key1]
          a -> b [key = 1]
          edge [test=key1]
          b -> a [key = 1]
          edge [test=bad1]
          a -> b [key = 1]
          edge [test=bad2]
          b -> a [key = 1]
          edge [test=key2]
          a -> b [key=2]
        }
      }
    `);
    expectDot(result).toMatchRawStringInlineSnapshot(`
      digraph {
      	graph [bb="0,0,54,108"];
      	node [label="\\N"];
      	{
      		edge [test=key2];
      		a	[height=0.5,
      			pos="27,90",
      			width=0.75];
      		b	[height=0.5,
      			pos="27,18",
      			width=0.75];
      		a -> b	[pos="e,10.643,32.455 10.626,75.503 6.8265,66.4 5.8204,54.129 7.6076,43.348",
      			test=<no_key>];
      		a -> b	[key=1,
      			pos="e,21.138,35.789 21.122,72.055 20.328,64.574 20.076,55.579 20.367,47.137",
      			test=key1];
      		a -> b	[key=2,
      			pos="e,43.357,32.455 43.374,75.503 47.173,66.4 48.18,54.129 46.392,43.348"];
      		b -> a	[key=1,
      			pos="e,32.878,72.055 32.862,35.789 33.663,43.248 33.922,52.237 33.639,60.686",
      			test=key1];
      	}
      }
    `);
  });

  it('deduplicate edges in strict graph', () => {
    const result = renderDotAndCompareWithVizJS(`
      strict graph {
        {
          edge [test=1]
          a -- b
          edge [test=2]
          b -- a
          edge [test=3]
          a -- b
        }
      }
    `);
    expectDot(result).toMatchRawStringInlineSnapshot(`
      strict graph {
      	graph [bb="0,0,54,108"];
      	node [label="\\N"];
      	{
      		edge [test=3];
      		a	[height=0.5,
      			pos="27,90",
      			width=0.75];
      		b	[height=0.5,
      			pos="27,18",
      			width=0.75];
      		a -- b	[pos="27,71.697 27,60.846 27,46.917 27,36.104",
      			test=1];
      	}
      }
    `);
  });

  it('deduplicate edges in strict directed graph', () => {
    const result = renderDotAndCompareWithVizJS(`
      strict digraph {
        {
          edge [test=1]
          a -> b
          edge [test=2]
          b -> a
          edge [test=3]
          a -> b
        }
      }
    `);
    expectDot(result).toMatchRawStringInlineSnapshot(`
      strict digraph {
      	graph [bb="0,0,54,108"];
      	node [label="\\N"];
      	{
      		edge [test=3];
      		a	[height=0.5,
      			pos="27,90",
      			width=0.75];
      		b	[height=0.5,
      			pos="27,18",
      			width=0.75];
      		a -> b	[pos="e,21.138,35.789 21.122,72.055 20.328,64.574 20.076,55.579 20.367,47.137",
      			test=1];
      		b -> a	[pos="e,32.878,72.055 32.862,35.789 33.663,43.248 33.922,52.237 33.639,60.686",
      			test=2];
      	}
      }
    `);
  });

  describe('error on missing graph', () => {
    it.for(['', '   \n  ', '// line comment', '/* block comment */'])(
      'value $0',
      (dot) => {
        const result = dotviz.renderDot(dot);
        expectFailureResult(result).toMatchRawStringInlineSnapshot(
          `RenderingBackendError: Missing graph definition. Start your file with 'graph {}' or 'digraph {}'.`,
        );
      },
    );
  });

  it('error on missing graph at the beginning of file', () => {
    const result = dotviz.renderDot('test');
    expectFailureResult(result).toMatchRawStringInlineSnapshot(`
      ParserError: Unexpected identifier 'test', expected keyword 'strict', 'graph' or 'digraph' at the beginning of the file.

      1 | test
        | ^
    `);
  });

  it('error on graph without statements', () => {
    const result = dotviz.renderDot('graph // missing body');
    expectFailureResult(result).toMatchRawStringInlineSnapshot(`
      ParserError: Unexpected end of file, expected '{'.

      1 | graph // missing body
        |                      ^
    `);
  });

  it('error on using square brackets for graph definition', () => {
    const result = dotviz.renderDot('graph []');
    expectFailureResult(result).toMatchRawStringInlineSnapshot(`
      ParserError: Unexpected '[', expected '{'.

      1 | graph []
        |       ^
    `);
  });

  it('warns on ambiguous token sequences', () => {
    const result = dotviz.renderDot(dedent`
      digraph-1 {
        version=2.0.0
        hex_number=0x5f
      }
    `);

    expectDotWithWarnings(result).toMatchRawStringInlineSnapshot(`
      ParserWarning: Ambiguous token sequence: 'digraph-1' will be split into keyword 'digraph' and number '-1'. If you want it interpreted as a single value, use quotes: "...". Otherwise, use whitespace or other delimiters to separate tokens.

      1 | digraph-1 {
        | ^
      2 |   version=2.0.0

      ParserWarning: Ambiguous token sequence: '2.0.0' will be split into number '2.0' and number '.0'. If you want it interpreted as a single value, use quotes: "...". Otherwise, use whitespace or other delimiters to separate tokens.

      1 | digraph-1 {
      2 |   version=2.0.0
        |           ^
      3 |   hex_number=0x5f

      ParserWarning: Ambiguous token sequence: '0x5f' will be split into number '0' and identifier 'x5f'. If you want it interpreted as a single value, use quotes: "...". Otherwise, use whitespace or other delimiters to separate tokens.

      2 |   version=2.0.0
      3 |   hex_number=0x5f
        |              ^
      4 | }

      digraph -1 {
      	graph [bb="0,0,126,36",
      		hex_number=0,
      		version=2.0
      	];
      	node [label="\\N"];
      	.0	[height=0.5,
      		pos="27,18",
      		width=0.75];
      	x5f	[height=0.5,
      		pos="99,18",
      		width=0.75];
      }
    `);
  });

  it('error on using keyword as graph name', () => {
    const result = dotviz.renderDot('graph subgraph {}');
    expectFailureResult(result).toMatchRawStringInlineSnapshot(`
      ParserError: Unexpected reserved keyword 'subgraph' where graph name was expected. If you want to use it as an identifier, enclose it in quotes: "subgraph".

      1 | graph subgraph {}
        |       ^
    `);
  });

  it('error on using HTML string as a graph name', () => {
    const result = dotviz.renderDot('graph <SomeHTML> {}');
    expectFailureResult(result).toMatchRawStringInlineSnapshot(`
      ParserError: HTML string as graph name is not supported. If you want to use it as an identifier, enclose it in quotes: "<SomeHTML>".

      1 | graph <SomeHTML> {}
        |       ^
    `);
  });

  describe('error on invalid graph definition with various tokens', () => {
    it.for([
      ['', 'end of file'],
      [',', `','`],
      [':', `':'`],
      [';', `';'`],
      ['=', `'='`],
      ['[', `'['`],
      [']', `']'`],
      ['}', `'}'`],
      ['->', `'->'`],
      ['--', `'--'`],
      ['node', `keyword 'node'`],
      ['edge', `keyword 'edge'`],
      ['graph', `keyword 'graph'`],
      ['digraph', `keyword 'digraph'`],
      ['subgraph', `keyword 'subgraph'`],
      ['strict', `keyword 'strict'`],
      ['"bad"', `string "bad"`],
      [
        '"very very very very very long string"',
        `string "very very very ve..."`,
      ],
      ['<bad>', `HTML string <bad>`],
      [
        '<very very very very long HTML string>',
        `HTML string <very very very ve...>`,
      ],
    ])('token $0', ([token, tokenDebugMessage]) => {
      const result = dotviz.renderDot('graph name ' + token);
      expectFailureResult(result).toStrictEqual(dedent`
        ParserError: Unexpected ${tokenDebugMessage}, expected '{'.

        1 | graph name ${token}
          |            ^
      `);
    });
  });

  it('error on invalid syntax in graph statement list', () => {
    const result = dotviz.renderDot('graph { -- }');
    expectFailureResult(result).toMatchRawStringInlineSnapshot(`
      ParserError: Unexpected '--', expected node, edge, subgraph or attribute statement. If this is meant to be part of a label or name, enclose it in quotes ("...").

      1 | graph { -- }
        |         ^
    `);
  });

  it('error on invalid attributes syntax', () => {
    const result = dotviz.renderDot(dedent`
      graph {
        node {}
      }
    `);
    expectFailureResult(result).toMatchRawStringInlineSnapshot(`
      ParserError: Unexpected '{', expected '['.

      1 | graph {
      2 |   node {}
        |        ^
      3 | }
    `);
  });

  it('error on invalid syntax inside attribute list', () => {
    const result = dotviz.renderDot(dedent`
      graph {
        node [ -> ]
      }
    `);
    expectFailureResult(result).toMatchRawStringInlineSnapshot(`
      ParserError: Unexpected '->', expected attribute name. If this is meant to be part of a label or name, enclose it in quotes ("...").

      1 | graph {
      2 |   node [ -> ]
        |          ^
      3 | }
    `);
  });

  it('error on unterminated block comment', () => {
    const result = dotviz.renderDot(dedent`
      graph {
        test=/* never finishes
      }
    `);
    expectFailureResult(result).toMatchRawStringInlineSnapshot(`
      ParserError: Unexpected unterminated block comment '/* never finishes\\n}', add a closing '*/' to the comment.

      1 | graph {
      2 |   test=/* never finishes
        |        ^
      3 | }
    `);
  });

  it('error on unterminated string', () => {
    const result = dotviz.renderDot(dedent`
      graph {
        test="never finishes
      }
    `);
    expectFailureResult(result).toMatchRawStringInlineSnapshot(`
      ParserError: Unterminated string '"never finishes\\n}', add a closing '"' to the string.

      1 | graph {
      2 |   test="never finishes
        |        ^
      3 | }
    `);
  });

  it('error on unterminated html', () => {
    const result = dotviz.renderDot(dedent`
      graph {
        test=<never finishes
      }
    `);
    expectFailureResult(result).toMatchRawStringInlineSnapshot(`
      ParserError: Unterminated HTML string '<never finishes\\n}', add a closing '>' to the HTML string.

      1 | graph {
      2 |   test=<never finishes
        |        ^
      3 | }
    `);
  });

  it('error on invalid string concatenation', () => {
    const result = dotviz.renderDot(dedent`
      graph {
        test="string" + "and" + id
      }
    `);

    expectFailureResult(result).toMatchRawStringInlineSnapshot(`
      ParserError: Unexpected identifier 'id', expected a string literal.

      1 | graph {
      2 |   test="string" + "and" + id
        |                           ^
      3 | }
    `);
  });

  it('error on unexpected port in node statement', () => {
    const result = dotviz.renderDot(dedent`
      graph {
        a:bad_port
      }
    `);
    expectFailureResult(result).toMatchRawStringInlineSnapshot(`
      ParserError: Unexpected 'bad_port' port in node statement

      1 | graph {
      2 |   a:bad_port
        |     ^
      3 | }
    `);
  });

  it('error on invalid compass point', () => {
    const result = dotviz.renderDot(dedent`
      graph {
        a:port:bad_point
      }
    `);
    expectFailureResult(result).toMatchRawStringInlineSnapshot(`
      ParserError: Invalid compass point identifier 'bad_point'. Allowed values: n, ne, e, se, s, sw, w, nw, c, _.

      1 | graph {
      2 |   a:port:bad_point
        |          ^
      3 | }
    `);
  });

  it('error on using directed edges in an undirected graph', () => {
    const result = dotviz.renderDot(dedent`
      graph {
        a -> a
      }
    `);
    expectFailureResult(result).toMatchRawStringInlineSnapshot(`
      ParserError: Unexpected '->' in an undirected graph. Use '--' for undirected edges in a 'graph'.

      1 | graph {
      2 |   a -> a
        |     ^
      3 | }
    `);
  });

  it('error on using undirected edges in a directed graph', () => {
    const result = dotviz.renderDot(dedent`
      digraph {
        a -- a
      }
    `);
    expectFailureResult(result).toMatchRawStringInlineSnapshot(`
      ParserError: Unexpected '--' in a directed graph. Use '->' for directed edges in a 'digraph'.

      1 | digraph {
      2 |   a -- a
        |     ^
      3 | }
    `);
  });

  describe('error on invalid syntax inside subgraph', () => {
    it.for(['&', '/', '-', '.'])('character $0', (badChar) => {
      const result = dotviz.renderDot(dedent`
        digraph {
          { ${badChar} }
        }
      `);
      expectFailureResult(result).toStrictEqual(dedent`
        ParserError: Unexpected character '${badChar}', expected node, edge, subgraph or attribute statement. If this is meant to be part of a label or name, enclose it in quotes ("...").

        1 | digraph {
        2 |   { ${badChar} }
          |     ^
        3 | }
      `);
    });
  });

  it('error on invalid syntax in named subgraph definition', () => {
    const result = dotviz.renderDot(`
      graph {
        subgraph name <bad>
      }
    `);
    expectFailureResult(result).toMatchRawStringInlineSnapshot(`
      ParserError: Unexpected HTML string <bad>, expected '{'.

      2 |       graph {
      3 |         subgraph name <bad>
        |                       ^
      4 |       }
    `);
  });

  it('error on macro name syntax', () => {
    // `dot` accepts the deprecated macro-name syntax with a warning and ignores the name.
    const result = dotviz.renderDot(
      'digraph { node my_template = [shape=box] }',
    );
    expectFailureResult(result).toMatchRawStringInlineSnapshot(`
      ParserError: Unexpected identifier 'my_template', expected '['.

      1 | digraph { node my_template = [shape=box] }
        |                ^
    `);
  });

  describe('error on HTML string as names', () => {
    it('node name', () => {
      const result = dotviz.renderDot('digraph { <foo> [x=1] }');
      expectFailureResult(result).toMatchRawStringInlineSnapshot(`
        ParserError: HTML string as node name is not supported. If you want to use it as an identifier, enclose it in quotes: "<foo>".

        1 | digraph { <foo> [x=1] }
          |           ^
      `);
    });

    it('attribute name', () => {
      const result = dotviz.renderDot('digraph { a [<color>=red] }');
      expectFailureResult(result).toMatchRawStringInlineSnapshot(`
        ParserError: HTML string as attribute name is not supported. If you want to use it as an identifier, enclose it in quotes: "<color>".

        1 | digraph { a [<color>=red] }
          |              ^
      `);
    });
  });

  it('error on HTML + HTML string concatenation', () => {
    // `dot` silently loses HTML tagging on concatenation: <a>+<b> → plain "ab". We simply reject it.
    const result = dotviz.renderDot('digraph { label = <a> + <b> }');
    expectFailureResult(result).toMatchRawStringInlineSnapshot(`
      ParserError: Unexpected '+', expected node, edge, subgraph or attribute statement. If this is meant to be part of a label or name, enclose it in quotes ("...").

      1 | digraph { label = <a> + <b> }
        |                       ^
    `);
  });

  describe('non-BMP (astral) Unicode character handling', () => {
    it('correctly handles astral Unicode characters in node names', () => {
      const result = renderDotAndCompareWithVizJS('graph { 😀 }');
      expectDotWithWarnings(result).toMatchRawStringInlineSnapshot(`
        RenderingBackendWarning: Warning: no value for width of non-ASCII character 240. Falling back to width of space character

        graph {
        	graph [bb="0,0,54,36"];
        	node [label="\\N"];
        	😀	[height=0.5,
        		pos="27,18",
        		width=0.75];
        }
      `);
    });

    it('correctly handles astral Unicode characters in string attributes', () => {
      const result = renderDotAndCompareWithVizJS('graph { label="😀" }');
      expectDot(result).toMatchRawStringInlineSnapshot(`
        graph {
        	graph [bb="0,0,30,24.8",
        		label=😀,
        		lheight=0.23,
        		lp="15,12.4",
        		lwidth=0.19
        	];
        	node [label="\\N"];
        }
      `);
    });

    it('correctly report error positions if dot contains unicode', () => {
      const result = dotviz.renderDot('graph { \u{1F600} [= }');
      expectFailureResult(result).toMatchRawStringInlineSnapshot(`
        ParserError: Unexpected '=', expected attribute name. If this is meant to be part of a label or name, enclose it in quotes ("...").

        1 | graph { 😀 [= }
          |             ^
      `);
    });
  });
});
