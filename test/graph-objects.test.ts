import { describe, it } from 'vitest';

import * as VizPackage from '../src/index.ts';
import { expectSuccessResult } from './util/render-result.ts';

describe('Viz', () => {
  describe('rendering graph objects', () => {
    it('empty graph', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render({});

      expectSuccessResult(result).toMatchInlineSnapshot(`
        digraph {
        	graph [bb="0,0,0,0"];
        	node [label="\\N"];
        }
      `);
    });

    it('attributes in options override options in input', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render(
        {
          nodeAttributes: {
            shape: 'rectangle',
          },
        },
        {
          nodeAttributes: {
            shape: 'circle',
          },
        },
      );

      expectSuccessResult(result).toMatchInlineSnapshot(`
        digraph {
        	graph [bb="0,0,0,0"];
        	node [label="\\N",
        		shape=circle
        	];
        }
      `);
    });

    it('just edges', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render({
        edges: [{ tail: 'a', head: 'b' }],
      });

      expectSuccessResult(result).toMatchInlineSnapshot(`
        digraph {
        	graph [bb="0,0,54,108"];
        	node [label="\\N"];
        	a	[height=0.5,
        		pos="27,90",
        		width=0.75];
        	b	[height=0.5,
        		pos="27,18",
        		width=0.75];
        	a -> b	[pos="e,27,36.104 27,71.697 27,64.407 27,55.726 27,47.536"];
        }
      `);
    });

    it('undirected graph', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render({
        directed: false,
        edges: [{ tail: 'a', head: 'b' }],
      });

      expectSuccessResult(result).toMatchInlineSnapshot(`
        graph {
        	graph [bb="0,0,54,108"];
        	node [label="\\N"];
        	a	[height=0.5,
        		pos="27,90",
        		width=0.75];
        	b	[height=0.5,
        		pos="27,18",
        		width=0.75];
        	a -- b	[pos="27,71.697 27,60.846 27,46.917 27,36.104"];
        }
      `);
    });

    it('html attributes', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render({
        nodes: [
          {
            name: 'a',
            attributes: { label: { html: '<b>A</b>' } },
          },
        ],
      });

      expectSuccessResult(result).toMatchInlineSnapshot(`
        digraph {
        	graph [bb="0,0,54,36"];
        	a	[height=0.5,
        		label=<<b>A</b>>,
        		pos="27,18",
        		width=0.75];
        }
      `);
    });

    it('default attributes, nodes, edges, and nested subgraphs', async () => {
      const viz = await VizPackage.instance();
      const result = viz.render({
        graphAttributes: { rankdir: 'LR' },
        nodeAttributes: { shape: 'circle' },
        nodes: [
          { name: 'a', attributes: { label: 'A', color: 'red' } },
          { name: 'b', attributes: { label: 'B', color: 'green' } },
        ],
        edges: [
          { tail: 'a', head: 'b', attributes: { label: '1' } },
          { tail: 'b', head: 'c', attributes: { label: '2' } },
        ],
        subgraphs: [
          {
            name: 'cluster_1',
            nodes: [{ name: 'c', attributes: { label: 'C', color: 'blue' } }],
            edges: [{ tail: 'c', head: 'd', attributes: { label: '3' } }],
            subgraphs: [
              {
                name: 'cluster_2',
                nodes: [
                  { name: 'd', attributes: { label: 'D', color: 'orange' } },
                ],
              },
            ],
          },
        ],
      });

      expectSuccessResult(result).toMatchInlineSnapshot(`
        digraph {
        	graph [bb="0,0,297.04,84",
        		rankdir=LR
        	];
        	node [shape=circle];
        	subgraph cluster_1 {
        		graph [bb="150.02,8,289.04,76"];
        		subgraph cluster_2 {
        			graph [bb="229.02,16,281.04,68"];
        			d	[color=orange,
        				height=0.50029,
        				label=D,
        				pos="255.03,42",
        				width=0.50029];
        		}
        		c	[color=blue,
        			height=0.5,
        			label=C,
        			pos="176.02,42",
        			width=0.5];
        		c -> d	[label=3,
        			lp="215.52,50.4",
        			pos="e,236.63,42 194.5,42 203.55,42 214.85,42 225.17,42"];
        	}
        	a	[color=red,
        		height=0.50029,
        		label=A,
        		pos="18.01,42",
        		width=0.50029];
        	b	[color=green,
        		height=0.5,
        		label=B,
        		pos="97.021,42",
        		width=0.5];
        	a -> b	[label=1,
        		lp="57.521,50.4",
        		pos="e,78.615,42 36.485,42 45.544,42 56.842,42 67.155,42"];
        	b -> c	[label=2,
        		lp="136.52,50.4",
        		pos="e,157.62,42 115.49,42 124.55,42 135.85,42 146.16,42"];
        }
      `);
    });
  });
  it('html attributes with ports', async () => {
    const viz = await VizPackage.instance();
    const result = viz.render({
      name: 'structs',
      nodeAttributes: { shape: 'plaintext' },
      nodes: [
        {
          name: 'struct1',
          attributes: {
            label: {
              html: `
<TABLE BORDER="0" CELLBORDER="1" CELLSPACING="0">
  <TR><TD>left</TD><TD PORT="f1">mid dle</TD><TD PORT="f2">right</TD></TR>
</TABLE>`,
            },
          },
        },
        {
          name: 'struct2',
          attributes: {
            label: {
              html: `
<TABLE BORDER="0" CELLBORDER="1" CELLSPACING="0">
  <TR><TD PORT="f0">one</TD><TD>two</TD></TR>
</TABLE>`,
            },
          },
        },
        {
          name: 'struct3',
          attributes: {
            label: {
              html: `
<TABLE BORDER="0" CELLBORDER="1" CELLSPACING="0" CELLPADDING="4">
  <TR>
    <TD ROWSPAN="3">hello<BR/>world</TD>
    <TD COLSPAN="3">b</TD>
    <TD ROWSPAN="3">g</TD>
    <TD ROWSPAN="3">h</TD>
  </TR>
  <TR>
    <TD>c</TD><TD PORT="here">d</TD><TD>e</TD>
  </TR>
  <TR>
    <TD COLSPAN="3">f</TD>
  </TR>
</TABLE>`,
            },
          },
        },
      ],
      edges: [
        {
          head: 'struct2',
          tail: 'struct1',
          attributes: { headport: 'f0', tailport: 'f1' },
        },
        {
          head: 'struct3',
          tail: 'struct1',
          attributes: { headport: 'here', tailport: 'f2' },
        },
      ],
    });
    expectSuccessResult(result).toMatchInlineSnapshot(`
      digraph structs {
      	graph [bb="0,0,229.65,160.4"];
      	node [shape=plaintext];
      	struct1	[height=0.5,
      		label=<
      <TABLE BORDER="0" CELLBORDER="1" CELLSPACING="0">
        <TR><TD>left</TD><TD PORT="f1">mid dle</TD><TD PORT="f2">right</TD></TR>
      </TABLE>>,
      		pos="75.607,142.4",
      		width=1.6872];
      	struct2	[height=0.5,
      		label=<
      <TABLE BORDER="0" CELLBORDER="1" CELLSPACING="0">
        <TR><TD PORT="f0">one</TD><TD>two</TD></TR>
      </TABLE>>,
      		pos="34.607,44.2",
      		width=0.9613];
      	struct1:f1 -> struct2:f0	[pos="e,21.107,56.6 71.714,130 71.714,94.555 31.219,95.497 22.677,67.727"];
      	struct3	[height=1.2278,
      		label=<
      <TABLE BORDER="0" CELLBORDER="1" CELLSPACING="0" CELLPADDING="4">
        <TR>
          <TD ROWSPAN="3">hello<BR/>world</TD>
          <TD COLSPAN="3">b</TD>
          <TD ROWSPAN="3">g</TD>
          <TD ROWSPAN="3">h</TD>
        </TR>
        <TR>
          <TD>c</TD><TD PORT="here">d</TD><TD>e</TD>
        </TR>
        <TR>
          <TD COLSPAN="3">f</TD>
        </TR>
      </TABLE>>,
      		pos="158.61,44.2",
      		width=1.9735];
      	struct1:f2 -> struct3:here	[pos="e,154.55,52.701 112.13,130 112.13,102.74 131.68,76.664 146.53,60.796"];
      }
    `);
  });
  it('override default attributes', async () => {
    const viz = await VizPackage.instance();
    const result = viz.render({
      nodeAttributes: {
        color: 'blue',
      },
      nodes: [{ name: 'a', attributes: { color: 'red' } }, { name: 'b' }],
    });

    expectSuccessResult(result).toMatchInlineSnapshot(`
      digraph {
      	graph [bb="0,0,126,36"];
      	node [color=blue,
      		label="\\N"
      	];
      	a	[color=red,
      		height=0.5,
      		pos="27,18",
      		width=0.75];
      	b	[height=0.5,
      		pos="99,18",
      		width=0.75];
      }
    `);
  });
});
