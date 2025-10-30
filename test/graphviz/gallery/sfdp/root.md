---
aliases:
  - /_pages/Gallery/undirected/root.html
title: Undirected Large Graph Layout Using sfdp
svg: root.svg
copyright: Copyright © 1996 AT&T.  All rights reserved.
gv_file: root.gv.txt
img_src: root.png
url: /Gallery/undirected/root.html
---

Undirected graphs of moderate to very large size can be drawn using `sfdp`, a scalable multilevel
force directed algorithm. This graph, from the graphviz `tests/graphs` directory, has 1054 nodes,
and takes a few seconds using `sfdp`. Overlap of nodes are removed with `-Goverlap=prism` option.
More example of large graph layout can be found
<a href="http://www.yifanhu.net/GALLERY/GRAPHS/"> here</a>.

Note that this requires Graphviz to have been built with the
[GNU Triangulated Surface Library](https://gts.sourceforge.net/). Graphviz on
Windows does not have this support. See
https://gitlab.com/graphviz/graphviz/-/issues/1269 for more details.
