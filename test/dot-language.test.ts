import * as VizJSPackage from '@viz-js/viz';
import { describe, expect, it } from 'vitest';

import * as DotVizPackage from '../src/index.ts';
import { expectSuccessResult } from './util/render-result.ts';

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
});
