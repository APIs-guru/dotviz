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

  it('dedublicate edges in strict graph', () => {
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
  it('dedublicate edges in strict directed graph', () => {
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
});
