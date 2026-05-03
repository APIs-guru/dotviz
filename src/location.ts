export interface Location {
  index: number;
  line: number;
  column: number;
}

export function printLocation(
  message: string,
  body: string,
  location: Location,
): string {
  if (body === '') {
    return message;
  }
  return message + '\n\n' + printCodeFrame(body, location);
}

function printCodeFrame(body: string, location: Location): string {
  const lineIndex = location.line - 1;
  const columnIndex = location.column - 1;

  // FIXME print `${source.name}:${lineNum}:${columnNum}\n`;
  const lines = body.split(/\r\n|[\n\r]/g);
  const locationLine = lines[lineIndex];

  return printPrefixedLines([
    [lineIndex, lines[lineIndex - 1]],
    [lineIndex + 1, locationLine],
    [null, ' '.repeat(columnIndex) + '^'],
    [lineIndex + 2, lines[lineIndex + 1]],
  ]);
}

function printPrefixedLines(
  lines: [number | null, string | undefined][],
): string {
  let padLen = 0;
  for (const [lineNum, line] of lines) {
    if (line !== undefined && lineNum !== null) {
      padLen = Math.max(padLen, lineNum.toString().length);
    }
  }

  const result = [];
  for (const [lineNum, line] of lines) {
    if (line !== undefined) {
      const lineColumn = lineNum?.toString() ?? '';
      result.push(lineColumn.padStart(padLen) + ' | ' + line);
    }
  }

  return result.join('\n');
}
