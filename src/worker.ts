import type { Graph } from './graph.d.ts';
import { instance } from './index.ts';
import type { RenderOptions, RenderResult } from './viz.ts';

export interface RenderRequest {
  id: number;
  input: string | Graph;
  options: RenderOptions | undefined;
}

export interface RenderResponse {
  id: number;
  result: RenderResult;
}

const viz = await instance();

addEventListener(
  'message',
  function onmessageCallBack(event: MessageEvent<RenderRequest>) {
    const { id, input, options } = event.data;
    const response: RenderResponse = {
      id,
      result: viz.render(input, options),
    };
    postMessage(response);
  },
);
