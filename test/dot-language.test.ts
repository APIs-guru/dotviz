import * as VizJSPackage from '@viz-js/viz';
import { describe, expect, it } from 'vitest';

import * as DotVizPackage from '../src/index.ts';
import { dedent } from './util/dedent.ts';
import {
  expectErrors,
  expectFailureResult,
  expectSuccessResult,
  stringifyErrors,
} from './util/render-result.ts';

const vizJS = await VizJSPackage.instance();
const dotviz = await DotVizPackage.instance();

function renderString(dot: string): DotVizPackage.RenderResult {
  const dotvizResult = dotviz.render(dot);
  const vizJSResult = vizJS.render(dot);
  expect(dotvizResult).toStrictEqual(vizJSResult);
  return dotvizResult;
}

describe('Dot language support', () => {
  it('empty graph', () => {
    const result = renderString('graph {}');
    expectSuccessResult(result).toMatchInlineSnapshot(`
      graph {
      	graph [bb="0,0,0,0"];
      	node [label="\\N"];
      }
    `);

    const directedResult = renderString('digraph {}');
    expectSuccessResult(directedResult).toMatchInlineSnapshot(`
      digraph {
      	graph [bb="0,0,0,0"];
      	node [label="\\N"];
      }
    `);
  });

  it('strict empty graph', () => {
    const result = renderString('strict graph {}');
    expectSuccessResult(result).toMatchInlineSnapshot(`
      strict graph {
      	graph [bb="0,0,0,0"];
      	node [label="\\N"];
      }
    `);
  });

  it('named graph', () => {
    const result = renderString('graph test {}');
    expectSuccessResult(result).toMatchInlineSnapshot(`
      graph test {
      	graph [bb="0,0,0,0"];
      	node [label="\\N"];
      }
    `);

    const stringResult = renderString('graph "test" {}');
    expect(stringResult).toStrictEqual(result);

    const keywordResult = renderString('graph "graph" {}');
    expectSuccessResult(keywordResult).toMatchInlineSnapshot(`
      graph "graph" {
      	graph [bb="0,0,0,0"];
      	node [label="\\N"];
      }
    `);
  });

  it('ignores whitespace and comments', () => {
    const result = renderString(`
      graph
      \n\r\t\uFEFF
      # comment with # in the middle
      // another one with / and // in the middle
      /* start comment
         /* and * in the middle
         end comment */
      {}
    `);
    expectSuccessResult(result).toMatchInlineSnapshot(`
      graph {
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
    expectSuccessResult(result).toMatchInlineSnapshot(`
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

    expectSuccessResult(result).toMatchInlineSnapshot(`
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

  it('empty strings as global attributes', () => {
    const result = renderString(`
      graph {
        graph [a=""]
        node [b=""]
        edge [c=""]
      }
    `);
    expectSuccessResult(result).toMatchInlineSnapshot(`
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

  it('various values as attributes', () => {
    const result = renderString(String.raw`
      graph {
        graph [
          str1="",
          str2="\"",
          str3="\"a",
          str4="\\\"",
          str5="\\a\\",
          str6="a",
          str7="\
          a",
          str8="\na",
        ]
      }
    `);
    expectSuccessResult(result).toMatchInlineSnapshot(String.raw`
      graph {
      	graph [bb="0,0,0,0",
      		str1="",
      		str2="\"",
      		str3="\"a",
      		str4="\\\"",
      		str5="\\a\\",
      		str6=a,
      		str7="          a",
      		str8="\na"
      	];
      	node [label="\N"];
      }
    `);
  });

  it('global graph attributes shorthand', () => {
    const result = renderString(`
      graph {
        a=valueA
      }
    `);

    expectSuccessResult(result).toMatchInlineSnapshot(`
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

  it('apply default attributes values', () => {
    const result = renderString(`
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

    expectSuccessResult(result).toMatchInlineSnapshot(`
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
      },
    `;
    const options = {
      graphAttributes: { testGraph: 'valueGraph' },
      nodeAttributes: { testNode: 'valueNode' },
      edgeAttributes: { shape: 'valueEdge' },
    };
    const result = dotviz.render(dot, options);

    expectSuccessResult(result).toMatchInlineSnapshot(`
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
    const result = renderString(`
      graph {
        b -- a
      }
    `);

    expectSuccessResult(result).toMatchInlineSnapshot(`
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

    expectSuccessResult(directedResult).toMatchInlineSnapshot(`
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

    expectSuccessResult(result).toMatchInlineSnapshot(`
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

    expectSuccessResult(directedResult).toMatchInlineSnapshot(`
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
    const result = renderString(`
      graph {
        c -- b -- a [valueA=a]
      }
    `);

    expectSuccessResult(result).toMatchInlineSnapshot(`
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

    const directedResult = renderString(`
      digraph {
        c -> b -> a [valueA=a]
      }
    `);

    expectSuccessResult(directedResult).toMatchInlineSnapshot(`
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
    const result = renderString(`
      graph {
        a,b -- c,d -- e,f [valueA=a]
      }
    `);

    expectSuccessResult(result).toMatchInlineSnapshot(`
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
    const result = renderString(`
        graph {
         	node [a=""];
         	{ node [a=""] }
        }
      `);
    expectSuccessResult(result).toMatchInlineSnapshot(`
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
    const result = renderString(`
      graph {
        subgraph a { a1 }
        subgraph b { b1 }
        subgraph a { a2 }
      }
    `);

    expectSuccessResult(result).toMatchInlineSnapshot(`
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
    const result = renderString(`
      graph {
        {
          subgraph a { a1 }
          subgraph b { b1 }
          subgraph a { a2 }
        }
      }
    `);

    expectSuccessResult(result).toMatchInlineSnapshot(`
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
    const result = renderString(`
        digraph {
          {
            a->b
            edge [color=red]
            b->a
          }
        }
      `);
    expectSuccessResult(result).toMatchInlineSnapshot(`
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
    const result = renderString(`
      digraph {
        subgraph tails { a b } -> subgraph heads { c d }
      }
    `);
    expectSuccessResult(result).toMatchInlineSnapshot(`
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

  it('strict graph deduplication keeps ports from first edge declaration', () => {
    const result = renderString(`
      strict digraph {
        a -> a:n
        a -> a
      }
    `);
    expectSuccessResult(result).toMatchInlineSnapshot(`
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
    const result = renderString(`
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
    expectSuccessResult(result).toMatchInlineSnapshot(`
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
    const result = renderString(`
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
    expectSuccessResult(result).toMatchInlineSnapshot(`
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
    const result = renderString(`
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
    expectSuccessResult(result).toMatchInlineSnapshot(`
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
    const result = renderString(`
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
    expectSuccessResult(result).toMatchInlineSnapshot(`
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

  it('error on empty string', () => {
    const result = dotviz.render('');
    expectFailureResult(result).toMatchInlineSnapshot(
      `ParserError: Missing graph definition. Start your file with 'graph {}' or 'digraph {}'.`,
    );
  });

  it('error on missing graph at the beginning of file', () => {
    const result = dotviz.render('test');
    expectFailureResult(result).toMatchInlineSnapshot(`
      ParserError: Unexpected identifier 'test', expected keyword 'strict', 'graph' or 'digraph' at the beginning of the file.

      1 | test
        | ^
    `);
  });

  it('error on graph without statements', () => {
    const result = dotviz.render('graph // missing body');
    expectFailureResult(result).toMatchInlineSnapshot(`
      ParserError: Unexpected end of file, expected '{'.

      1 | graph // missing body
        |                      ^
    `);
  });

  it('error on using square brackets for graph definition', () => {
    const result = dotviz.render('graph []');
    expectFailureResult(result).toMatchInlineSnapshot(`
      ParserError: Unexpected '[', expected '{'.

      1 | graph []
        |       ^
    `);
  });

  it('warns on ambiguous token sequences', () => {
    const result = dotviz.render(dedent`
      digraph-1 {
        version=2.0.0
        hex_number=0x5f
      }
    `);
    expectErrors(result).toMatchInlineSnapshot(`
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
    `);
  });

  it('error on using keyword as graph name', () => {
    const result = dotviz.render('graph subgraph {}');
    expectFailureResult(result).toMatchInlineSnapshot(`
      ParserError: Unexpected reserved keyword 'subgraph' where graph name was expected. If you want to use it as an identifier, enclose it in quotes: "subgraph".

      1 | graph subgraph {}
        |       ^
    `);
  });

  it('error on using HTML string as a graph name', () => {
    const result = dotviz.render('graph <SomeHTML> {}');
    expectFailureResult(result).toMatchInlineSnapshot(`
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
      const result = dotviz.render('graph name ' + token);
      expect(stringifyErrors(result.errors)).toStrictEqual(dedent`
        ParserError: Unexpected ${tokenDebugMessage}, expected '{'.

        1 | graph name ${token}
          |            ^
      `);
    });
  });

  it('error on invalid syntax in graph statement list', () => {
    const result = dotviz.render('graph { -- }');
    expectFailureResult(result).toMatchInlineSnapshot(`
      ParserError: Unexpected '--', expected node, edge, subgraph or attribute statement. If this is meant to be part of a label or name, enclose it in quotes ("...").

      1 | graph { -- }
        |         ^
    `);
  });

  it('error on invalid attributes syntax', () => {
    const result = dotviz.render(dedent`
      graph {
        node {}
      }
    `);
    expectFailureResult(result).toMatchInlineSnapshot(`
      ParserError: Unexpected '{', expected '['.

      1 | graph {
      2 |   node {}
        |        ^
      3 | }
    `);
  });

  it('error on invalid syntax inside attribute list', () => {
    const result = dotviz.render(dedent`
      graph {
        node [ -> ]
      }
    `);
    expectFailureResult(result).toMatchInlineSnapshot(`
      ParserError: Unexpected '->', expected attribute name. If this is meant to be part of a label or name, enclose it in quotes ("...").

      1 | graph {
      2 |   node [ -> ]
        |          ^
      3 | }
    `);
  });

  it('error on unterminated block comment', () => {
    const result = dotviz.render(dedent`
      graph {
        test=/* never finishes
      }
    `);
    expectFailureResult(result).toMatchInlineSnapshot(`
      ParserError: Unexpected unterminated block comment '/* never finishes\\n}', add a closing '*/' to the comment.

      1 | graph {
      2 |   test=/* never finishes
        |        ^
      3 | }
    `);
  });

  it('error on unterminated string', () => {
    const result = dotviz.render(dedent`
      graph {
        test="never finishes
      }
    `);
    expectFailureResult(result).toMatchInlineSnapshot(`
      ParserError: Unterminated string '"never finishes\\n}', add a closing '"' to the string.

      1 | graph {
      2 |   test="never finishes
        |        ^
      3 | }
    `);
  });

  it('error on unterminated html', () => {
    const result = dotviz.render(dedent`
      graph {
        test=<never finishes
      }
    `);
    expectFailureResult(result).toMatchInlineSnapshot(`
      ParserError: Unterminated HTML string '<never finishes\\n}', add a closing '>' to the HTML string.

      1 | graph {
      2 |   test=<never finishes
        |        ^
      3 | }
    `);
  });

  it('error on unexpected port in node statement', () => {
    const result = dotviz.render(dedent`
      graph {
        a:bad_port
      }
    `);
    expectFailureResult(result).toMatchInlineSnapshot(`
      ParserError: Unexpected 'bad_port' port in node statement

      1 | graph {
      2 |   a:bad_port
        |     ^
      3 | }
    `);
  });

  it('error on invalid compass point', () => {
    const result = dotviz.render(dedent`
      graph {
        a:port:bad_point
      }
    `);
    expectFailureResult(result).toMatchInlineSnapshot(`
      ParserError: Invalid compass point identifier 'bad_point'. Allowed values: n, ne, e, se, s, sw, w, nw, c, _.

      1 | graph {
      2 |   a:port:bad_point
        |          ^
      3 | }
    `);
  });

  it('error on using directed edges in an undirected graph', () => {
    const result = dotviz.render(dedent`
      graph {
        a -> a
      }
    `);
    expectFailureResult(result).toMatchInlineSnapshot(`
      ParserError: Unexpected '->' in an undirected graph. Use '--' for undirected edges in a 'graph'.

      1 | graph {
      2 |   a -> a
        |     ^
      3 | }
    `);
  });

  it('error on using undirected edges in a directed graph', () => {
    const result = dotviz.render(dedent`
      digraph {
        a -- a
      }
    `);
    expectFailureResult(result).toMatchInlineSnapshot(`
      ParserError: Unexpected '--' in a directed graph. Use '->' for directed edges in a 'digraph'.

      1 | digraph {
      2 |   a -- a
        |     ^
      3 | }
    `);
  });

  describe('error on invalid syntax inside subgraph', () => {
    it.for(['&', '/', '-', '.'])('character $0', (badChar) => {
      const result = dotviz.render(dedent`
        digraph {
          { ${badChar} }
        }
      `);
      expect(stringifyErrors(result.errors)).toStrictEqual(dedent`
        ParserError: Unexpected character '${badChar}', expected node, edge, subgraph or attribute statement. If this is meant to be part of a label or name, enclose it in quotes ("...").

        1 | digraph {
        2 |   { ${badChar} }
          |     ^
        3 | }
      `);
    });
  });

  it('error on invalid syntax in named subgraph definition', () => {
    const result = dotviz.render(`
      graph {
        subgraph name <bad>
      }
    `);
    expectFailureResult(result).toMatchInlineSnapshot(`
      ParserError: Unexpected HTML string <bad>, expected '{'.

      2 |       graph {
      3 |         subgraph name <bad>
        |                       ^
      4 |       }
    `);
  });
});
