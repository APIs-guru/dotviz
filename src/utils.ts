import {
  NormalizedAttributes,
  type NormalizedAttributeValue,
} from './normalize-graph.ts';
import { isNumberToken } from './parser.ts';

export function formatValueForDiagnostics(value: string) {
  const truncated = value.length > 20 ? value.slice(0, 17) + '...' : value;
  return JSON.stringify(truncated)
    .replaceAll(String.raw`\"`, '"')
    .replaceAll(String.raw`\\`, '\\')
    .slice(1, -1);
}

export function parseDotNumber(value: NormalizedAttributeValue): number {
  return NormalizedAttributes.isText(value) && isNumberToken(value.text)
    ? Number.parseFloat(value.text)
    : Number.NaN;
}
