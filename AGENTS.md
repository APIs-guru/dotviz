# Agent Guidelines

Conventions and lessons for LLM agents working on this codebase, distilled from
code-review feedback.

---

## Project goal

This project aims to match the behaviour of the reference Graphviz binary as
closely as possible, with two deliberate divergences:

1. **Stricter validation.** Inputs that Graphviz accepts silently (e.g. an
   out-of-range `linelength`) are rejected with a clear error.
2. **Removed legacy features.** Severely outdated features are not supported,
   but they are never silently ignored — they are validated and reported as
   errors. For example, non-UTF-8 charsets (`charset=latin1`) produce an
   error rather than being passed through.

When in doubt: match Graphviz output exactly, then ask whether the case
requires stricter validation or falls under a removed feature.

---

## Match existing style before writing new code

Before writing any new code, search the codebase for similar existing code and
match its style exactly. This applies to tests, source code, and comments.

For tests specifically:

- Read **all** test files (`test/*.test.ts`) before writing a new test.
- Find the closest existing test to what you are adding and follow its
  structure, naming, and assertion style. Do not invent a new pattern when one
  already exists.

---

## Write tests that are readable at a glance

Tests should be optimised for visual inspection. A reader must be able to
understand what is being tested and where something goes wrong without counting
characters or computing positions mentally.

- **Test end-to-end.** In the vast majority of cases tests should supply a
  full DOT input and assert the full output. Small utility helpers such as
  `expectDot` exist precisely to keep this readable without sacrificing
  completeness.
- **Mark positions visually.** When a test involves a location in a string
  (e.g. a parse error), the expected output should point to it with `^` under
  the relevant token — not just state an offset number.
- **Make input/output self-evident.** Choose test data whose structure is
  immediately obvious: a reader should see at a glance what value triggers the
  behaviour and why.

---

## Verify Graphviz behaviour with the binary before asserting it in tests

Do not assume how Graphviz behaves — run the `dot` binary directly and inspect
the output. Use the file creation tool to write the input to a temporary file
inside the project, giving it a descriptive and unique name such as
`tmp/xk72-test-charset-utf8.dot` to avoid collisions when multiple agents run
in parallel, then run:

```sh
dot -Tdot tmp/xk72-test-charset-utf8.dot
```

---

## Reference documentation

Check `spec/README.md` to find reference documents relevant to the area you are working in.

Keep these documents up to date when behaviour changes.

---

## Workflow

1. Before writing any code, search the codebase for similar patterns and match
   their style (see first section).
2. Verify any assumption about Graphviz output by running the `dot` binary
   directly (see above).
3. Run `npx vitest run test/render.test.ts` (or the relevant file) after every
   change to confirm tests pass.
4. After editing any file run `npx prettier --write <file>` to fix formatting.
5. When the user applies manual edits or corrections, inspect what changed,
   derive the generalised lesson behind each correction, and present them
   concisely. Then ask: "Should any of these be added to AGENTS.md?"
6. After completing a task, reflect on any parts of the code that were
   confusing or required extra effort to understand. Suggest concrete
   improvements to make those areas clearer for both humans and future agents
   (e.g. better variable or function names, comments on non-trivial logic,
   clearer data structure shapes). Present these as a short list and ask
   whether the user wants any of them applied.
