# Spec Guidelines

---

## Specs describe behaviour, not implementation

Spec files must not reference dotviz internals — source file paths,
function names, module names, or backend architecture details. Specs
describe observable behaviour: what Graphviz does, what dotviz does
differently, and why. Implementation details belong in code comments.

---

## Graphviz source may be referenced

Since Graphviz changes rarely, referencing its source files (e.g.
`lib/xdot/xdot.c`, `lib/common/emit.c`) is acceptable when explaining
complexity or rationale. Graphviz source is stable enough to treat as
reference material.

---

## README entries describe scope, not contents

Entries in `README.md` should describe what a file covers at a high level.
Do not enumerate specific attributes, section numbers, or other details —
that creates maintenance burden every time the file changes.
