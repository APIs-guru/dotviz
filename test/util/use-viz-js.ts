import { instance, type Viz } from '@viz-js/viz';

export async function useVizJSInstance(): Promise<Viz | undefined> {
  /* v8 ignore start */
  return await (process.env.USE_VIZ_JS === undefined ? undefined : instance());
}
