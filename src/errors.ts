import type { RenderError } from './viz.d.ts';

const errorPatterns = [
  [/^Error: (.*)/, 'error'],
  [/^Warning: (.*)/, 'warning'],
] as const;

export function parseStderrMessages(
  messages: readonly string[],
): RenderError[] {
  return messages.map((message) => {
    for (const [pattern, level] of errorPatterns) {
      let match;

      if ((match = pattern.exec(message)) !== null) {
        return { message: match[1].trimEnd(), level };
      }
    }

    return { message: message.trimEnd(), level: undefined };
  });
}

export function parseAgerrMessages(messages: readonly string[]): RenderError[] {
  const result: RenderError[] = [];
  let level: 'error' | 'warning' | undefined;

  for (let i = 0; i < messages.length; i++) {
    if (messages[i] == 'Error' && messages[i + 1] == ': ') {
      level = 'error';
      i += 1;
    } else if (messages[i] == 'Warning' && messages[i + 1] == ': ') {
      level = 'warning';
      i += 1;
    } else {
      result.push({
        message: messages[i].trimEnd(),
        level: level,
      });
    }
  }

  return result;
}
