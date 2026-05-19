import { instance } from 'dotviz';

const viz = await instance();
console.log(viz.renderString('digraph { a -> b }'));
