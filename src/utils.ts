export function formatValueForDiagnostics(value: string) {
  const truncated = value.length > 20 ? value.slice(0, 17) + '...' : value;
  return JSON.stringify(truncated)
    .replaceAll(String.raw`\"`, '"')
    .replaceAll(String.raw`\\`, '\\')
    .slice(1, -1);
}

const DOT_NUMBER_RE = /^-?\d+(\.\d+)?$/;
export function parseDotNumber(value: string): number {
  return DOT_NUMBER_RE.test(value) ? Number(value) : Number.NaN;
}
