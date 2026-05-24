import { instance, type Viz } from '@viz-js/viz';

export async function useVizJSInstance(): Promise<Viz> {
  return await instance();
}
