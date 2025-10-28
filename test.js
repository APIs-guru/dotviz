import fs from 'node:fs';

import * as DotVizPackage from '@viz-js/viz';

const dotviz = await DotVizPackage.instance();

const graph = fs.readFileSync('./test/gallery/directed/siblings.gv', 'utf8');
const result = dotviz.render(graph, { format: 'dot' });

console.log(result);
