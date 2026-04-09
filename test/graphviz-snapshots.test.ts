import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import * as VizPackage from '@viz-js/viz';
import { describe, expect, it, type TestContext } from 'vitest';

import * as DotVizPackage from '../src/index.ts';
import { expectString } from './util/raw-string-serializer.ts';

const useVizJS = process.env.USE_VIZ_JS != null;
const renderFormats = useVizJS
  ? async (input: string, formats: string[], options: { engine: string }) => {
      const instance = await VizPackage.instance();
      return instance.renderFormats(input, formats, options);
    }
  : async (input: string, formats: string[], options: { engine: string }) => {
      const instance = await DotVizPackage.instance();
      return instance.renderFormats(input, formats, options);
    };

// FIXME: many files are modified with replaced fonts and removed unicode symbols
// update after we fully support native fonts
describe('GraphViz Gallery', () => {
  /* spell-checker: disable */
  describe('directed', () => {
    it('dot: gallery/directed/bazel.gv', snapshotGvFile);
    it('dot: gallery/directed/cluster.gv', snapshotGvFile);
    it('dot: gallery/directed/crazy.gv', snapshotGvFile);
    it('dot: gallery/directed/datastruct.gv', snapshotGvFile);
    it('dot: gallery/directed/fsm.gv', snapshotGvFile);
    it('dot: gallery/directed/Genetic_Programming.gv', snapshotGvFile);
    it('dot: gallery/directed/git.gv', snapshotGvFile);
    it('dot: gallery/directed/go-package.gv', snapshotGvFile);
    it('dot: gallery/directed/hello.gv', snapshotGvFile);
    it.skip('dot: gallery/directed/kennedyanc.gv', snapshotGvFile);
    it('dot: gallery/directed/Linux_kernel_diagram.gv', snapshotGvFile);
    it('dot: gallery/directed/lion_share.gv', snapshotGvFile);
    it('dot: gallery/directed/neural-network.gv', snapshotGvFile);
    it('dot: gallery/directed/ninja.gv', snapshotGvFile);
    it('dot: gallery/directed/pprof.gv', snapshotGvFile);
    it('dot: gallery/directed/profile.gv', snapshotGvFile);
    it('dot: gallery/directed/psg.gv', snapshotGvFile);
    it('dot: gallery/directed/sdh.gv', snapshotGvFile);

    // FIXME: viz-js also crashes on bellow graph
    // digraph {graph[label="\nKappa Kappa Psi/Tau Beta Sigma\nSan Diego State University"]}
    it.skip('dot: gallery/directed/siblings.gv', snapshotGvFile);

    it('dot: gallery/directed/switch.gv', snapshotGvFile);
    it('dot: gallery/directed/UML_Class_diagram.gv', snapshotGvFile);
    it('dot: gallery/directed/unix.gv', snapshotGvFile);
    it('dot: gallery/directed/world.gv', snapshotGvFile);
  });
  describe.skip('fdp', () => {
    it('fdp: gallery/fdp/fdpclust.gv', snapshotGvFile);
  });
  describe('gradient', () => {
    it('dot: gallery/gradient/angles.gv', snapshotGvFile);
    it('dot: gallery/gradient/cluster.gv', snapshotGvFile);
    it('dot: gallery/gradient/colors.gv', snapshotGvFile);
    it('dot: gallery/gradient/datastruct.gv', snapshotGvFile);
    it('dot: gallery/gradient/g_c_n.gv', snapshotGvFile);
    it('dot: gallery/gradient/linear_angle.gv', snapshotGvFile);
    it('dot: gallery/gradient/radial_angle.gv', snapshotGvFile);
    it('dot: gallery/gradient/table.gv', snapshotGvFile);
  });
  describe('neato', () => {
    it('neato: gallery/neato/color_wheel.gv', snapshotGvFile);
    it('neato: gallery/neato/colors.gv', snapshotGvFile);
    it('neato: gallery/neato/ER.gv', snapshotGvFile);
    it('neato: gallery/neato/philo.gv', snapshotGvFile);
    it('neato: gallery/neato/process.gv', snapshotGvFile);
    it('neato: gallery/neato/softmaint.gv', snapshotGvFile);
    it('neato: gallery/neato/traffic_lights.gv', snapshotGvFile);
    it('neato: gallery/neato/transparency.gv', snapshotGvFile);
  });
  describe.skip('sfdp', () => {
    it('sfdp: gallery/sfdp/root.gv', snapshotGvFile);
  });
  describe.skip('twopi', () => {
    it('twopi: gallery/twopi/happiness.gv', snapshotGvFile);
    it('twopi: gallery/twopi/networkmap_twopi.gv', snapshotGvFile);
    it('twopi: gallery/twopi/twopi2.gv', snapshotGvFile);
  });
  describe('undirected', () => {
    it('dot: gallery/undirected/gd_1994_2007.gv', snapshotGvFile);
    it('dot: gallery/undirected/grid.gv', snapshotGvFile);
    it('dot: gallery/undirected/inet.gv', snapshotGvFile);
  });
  /* spell-checker: enable */
});

describe('fonts', () => {
  /* spell-checker: disable */
  it('dot: fonts/AvantGarde.gv', snapshotGvFile);
  it('dot: fonts/Bookman.gv', snapshotGvFile);
  it('dot: fonts/Helvetica.gv', snapshotGvFile);
  it('dot: fonts/NewCenturySchlbk.gv', snapshotGvFile);
  it('dot: fonts/Palatino.gv', snapshotGvFile);
  it('dot: fonts/Symbol.gv', snapshotGvFile);
  it('dot: fonts/Times.gv', snapshotGvFile);
  it('dot: fonts/ZapfChancery.gv', snapshotGvFile);
  it('dot: fonts/ZapfDingbats.gv', snapshotGvFile);
  /* spell-checker: enable */
});
describe('languages', () => {
  /* spell-checker: disable */
  it('dot: languages/japanese.gv', snapshotGvFile);
  it('dot: languages/russian.gv', snapshotGvFile);
  /* spell-checker: enable */
});

describe('miscellaneous', () => {
  /* spell-checker: disable */
  it('dot: graphs/a.gv', snapshotGvFile);
  it('dot: graphs/abstract.gv', snapshotGvFile);
  it('dot: graphs/alf.gv', snapshotGvFile);
  it('dot: graphs/arrows.gv', snapshotGvFile);
  it('dot: graphs/arrowsize.gv', snapshotGvFile);
  it('dot: graphs/awilliams.gv', snapshotGvFile);
  it('dot: graphs/b.gv', snapshotGvFile);
  it('dot: graphs/b3.gv', snapshotGvFile);
  it('dot: graphs/b7.gv', snapshotGvFile);
  // it('dot: graphs/b15.gv', snapshotGvFile);
  it('dot: graphs/b22.gv', snapshotGvFile);
  it('dot: graphs/b29.gv', snapshotGvFile);
  it('dot: graphs/b33.gv', snapshotGvFile);
  // FIXME it('dot: graphs/b34.gv', snapshotGvFile);
  it('dot: graphs/b36.gv', snapshotGvFile);
  it('dot: graphs/b51.gv', snapshotGvFile);
  it('dot: graphs/b53.gv', snapshotGvFile);
  // FIXME it.only('dot: graphs/b56.gv', snapshotGvFile);
  it('dot: graphs/b57.gv', snapshotGvFile);
  it('dot: graphs/b58.gv', snapshotGvFile);
  // FIXME it.only('dot: graphs/b60.gv', snapshotGvFile);
  it('dot: graphs/b62.gv', snapshotGvFile);
  it('dot: graphs/b68.gv', snapshotGvFile);
  it('dot: graphs/b69.gv', snapshotGvFile);
  it('dot: graphs/b71.gv', snapshotGvFile);
  it('dot: graphs/b73.gv', snapshotGvFile);
  it('dot: graphs/b73a.gv', snapshotGvFile);
  it('dot: graphs/b76.gv', snapshotGvFile);
  it('dot: graphs/b77.gv', snapshotGvFile);
  it('dot: graphs/b79.gv', snapshotGvFile);
  it('dot: graphs/b80.gv', snapshotGvFile);
  it('dot: graphs/b80a.gv', snapshotGvFile);
  it('dot: graphs/b81.gv', snapshotGvFile);
  it('dot: graphs/b85.gv', snapshotGvFile);
  it('dot: graphs/b94.gv', snapshotGvFile);
  // FIXME runs for 16sec it.only('dot: graphs/b100.gv', snapshotGvFile);
  it('dot: graphs/b102.gv', snapshotGvFile);
  it('dot: graphs/b103.gv', snapshotGvFile);
  // FIXME runs for 15sec it.only('dot: graphs/b104.gv', snapshotGvFile);
  it('dot: graphs/b106.gv', snapshotGvFile);
  it('dot: graphs/b117.gv', snapshotGvFile);
  it('dot: graphs/b123.gv', snapshotGvFile);
  it('dot: graphs/b124.gv', snapshotGvFile);
  it('dot: graphs/b135.gv', snapshotGvFile);
  it('dot: graphs/b143.gv', snapshotGvFile);
  // it('dot: graphs/b145.gv', snapshotGvFile);
  it('dot: graphs/b146.gv', snapshotGvFile);
  it('dot: graphs/b155.gv', snapshotGvFile);
  it('dot: graphs/b491.gv', snapshotGvFile);
  it('dot: graphs/b545.gv', snapshotGvFile);
  it('dot: graphs/b786.gv', snapshotGvFile);
  it('dot: graphs/b993.gv', snapshotGvFile);
  it('dot: graphs/bad.gv', snapshotGvFile);
  it('dot: graphs/badvoro.gv', snapshotGvFile);
  it('dot: graphs/big.gv', snapshotGvFile);
  it('dot: graphs/biglabel.gv', snapshotGvFile);
  it('dot: graphs/center.gv', snapshotGvFile);
  it('dot: graphs/clover.gv', snapshotGvFile);
  // it.only('dot: graphs/clust.gv', snapshotGvFile);
  it('dot: graphs/clust1.gv', snapshotGvFile);
  it('dot: graphs/clust2.gv', snapshotGvFile);
  it('dot: graphs/clust3.gv', snapshotGvFile);
  it('dot: graphs/clust4.gv', snapshotGvFile);
  it('dot: graphs/clust5.gv', snapshotGvFile);
  it('dot: graphs/clusters.gv', snapshotGvFile);
  it('dot: graphs/clustlabel.gv', snapshotGvFile);
  // Case(
  //     "clustlabel",
  //     Path("clustlabel.gv"),
  //     "dot",
  //     "ps",
  //     ["-Glabelloc=t", "-Glabeljust=r"],
  // ),
  // Case(
  //     "clustlabel",
  //     Path("clustlabel.gv"),
  //     "dot",
  //     "ps",
  //     ["-Glabelloc=b", "-Glabeljust=r"],
  //     1,
  // ),
  // Case(
  //     "clustlabel",
  //     Path("clustlabel.gv"),
  //     "dot",
  //     "ps",
  //     ["-Glabelloc=t", "-Glabeljust=l"],
  //     2,
  // ),
  // Case(
  //     "clustlabel",
  //     Path("clustlabel.gv"),
  //     "dot",
  //     "ps",
  //     ["-Glabelloc=b", "-Glabeljust=l"],
  //     3,
  // ),
  // Case(
  //     "clustlabel",
  //     Path("clustlabel.gv"),
  //     "dot",
  //     "ps",
  //     ["-Glabelloc=t", "-Glabeljust=c"],
  //     4,
  // ),
  // Case(
  //     "clustlabel",
  //     Path("clustlabel.gv"),
  //     "dot",
  //     "ps",
  //     ["-Glabelloc=b", "-Glabeljust=c"],
  //     5,
  // ),
  // Case("clustlabel", Path("clustlabel.gv"), "dot", "ps", ["-Glabelloc=t"], 6),
  // Case("clustlabel", Path("clustlabel.gv"), "dot", "ps", ["-Glabelloc=b"], 7),

  it('dot: graphs/color.gv', snapshotGvFile);
  // # color encodings
  // # multiple edge colors
  // Case("color", Path("color.gv"), "dot", "png", []),
  // Case("color", Path("color.gv"), "dot", "png", ["-Gbgcolor=lightblue"]),

  // pencolor, fontcolor, fillcolor
  it('dot: graphs/colors.gv', snapshotGvFile);
  it('dot: graphs/colorscheme.gv', snapshotGvFile);
  it('dot: graphs/compound.gv', snapshotGvFile);
  it('dot: graphs/crazy.gv', snapshotGvFile);
  // Case("rotate", Path("crazy.gv"), "dot", "ps", ["-Glandscape"]),
  // Case("rotate", Path("crazy.gv"), "dot", "ps", ["-Grotate=90"], 1),
  // Case("rankdir", Path("crazy.gv"), "dot", "gv", ["-Grankdir=LR"]),
  // Case("rankdir", Path("crazy.gv"), "dot", "gv", ["-Grankdir=BT"], 1),
  // Case("rankdir", Path("crazy.gv"), "dot", "gv", ["-Grankdir=RL"], 2),

  it('dot: graphs/ctext.gv', snapshotGvFile);
  it('dot: graphs/d.gv', snapshotGvFile);
  it('dot: graphs/dd.gv', snapshotGvFile);
  it('dot: graphs/decorate.gv', snapshotGvFile);
  it('dot: graphs/dfa.gv', snapshotGvFile);
  it('dot: graphs/dir.gv', snapshotGvFile);
  it('dot: graphs/dpd.gv', snapshotGvFile);
  it('dot: graphs/edgeclip.gv', snapshotGvFile);
  it('dot: graphs/ER.gv', snapshotGvFile);
  // FDP it('dot: graphs/fdp.gv', snapshotGvFile);
  it('dot: graphs/fig6.gv', snapshotGvFile);
  it('dot: graphs/flatedge.gv', snapshotGvFile);
  it('dot: graphs/fsm.gv', snapshotGvFile);
  it('dot: graphs/grammar.gv', snapshotGvFile);
  it('dot: graphs/grdangles.gv', snapshotGvFile);
  it('dot: graphs/grdcluster.gv', snapshotGvFile);
  it('dot: graphs/grdcolors.gv', snapshotGvFile);
  it('dot: graphs/grdfillcolor.gv', snapshotGvFile);
  it('dot: graphs/grdlinear_angle.gv', snapshotGvFile);
  it('dot: graphs/grdlinear_node.gv', snapshotGvFile);
  it('dot: graphs/grdlinear.gv', snapshotGvFile);
  it('dot: graphs/grdradial_angle.gv', snapshotGvFile);
  it('dot: graphs/grdradial_node.gv', snapshotGvFile);
  it('dot: graphs/grdradial.gv', snapshotGvFile);
  it('dot: graphs/grdshapes.gv', snapshotGvFile);
  it('dot: graphs/hashtable.gv', snapshotGvFile);
  it('dot: graphs/Heawood.gv', snapshotGvFile);
  it('dot: graphs/honda-tokoro.gv', snapshotGvFile);
  it('dot: graphs/html.gv', snapshotGvFile);
  // FIXME it('dot: graphs/html2.gv', snapshotGvFile);
  it('dot: graphs/in.gv', snapshotGvFile);
  it('dot: graphs/jcctree.gv', snapshotGvFile);
  it('dot: graphs/jsort.gv', snapshotGvFile);
  it('dot: graphs/KW91.gv', snapshotGvFile);

  // FIXME use parametrise testing
  it('dot: graphs/labelclust-fbc.gv', snapshotGvFile);
  it('dot: graphs/labelclust-fbd.gv', snapshotGvFile);
  it('dot: graphs/labelclust-fbl.gv', snapshotGvFile);
  it('dot: graphs/labelclust-fbr.gv', snapshotGvFile);
  it('dot: graphs/labelclust-fdc.gv', snapshotGvFile);
  it('dot: graphs/labelclust-fdd.gv', snapshotGvFile);
  it('dot: graphs/labelclust-fdl.gv', snapshotGvFile);
  it('dot: graphs/labelclust-fdr.gv', snapshotGvFile);
  it('dot: graphs/labelclust-ftc.gv', snapshotGvFile);
  it('dot: graphs/labelclust-ftd.gv', snapshotGvFile);
  it('dot: graphs/labelclust-ftl.gv', snapshotGvFile);
  it('dot: graphs/labelclust-ftr.gv', snapshotGvFile);
  it('dot: graphs/labelclust-nbc.gv', snapshotGvFile);
  it('dot: graphs/labelclust-nbd.gv', snapshotGvFile);
  it('dot: graphs/labelclust-nbl.gv', snapshotGvFile);
  it('dot: graphs/labelclust-nbr.gv', snapshotGvFile);
  it('dot: graphs/labelclust-ndc.gv', snapshotGvFile);
  it('dot: graphs/labelclust-ndd.gv', snapshotGvFile);
  it('dot: graphs/labelclust-ndl.gv', snapshotGvFile);
  it('dot: graphs/labelclust-ndr.gv', snapshotGvFile);
  it('dot: graphs/labelclust-ntc.gv', snapshotGvFile);
  it('dot: graphs/labelclust-ntd.gv', snapshotGvFile);
  it('dot: graphs/labelclust-ntl.gv', snapshotGvFile);
  it('dot: graphs/labelclust-ntr.gv', snapshotGvFile);

  // FIXME use parametrise testing
  it('dot: graphs/labelroot-fbc.gv', snapshotGvFile);
  it('dot: graphs/labelroot-fbd.gv', snapshotGvFile);
  it('dot: graphs/labelroot-fbl.gv', snapshotGvFile);
  it('dot: graphs/labelroot-fbr.gv', snapshotGvFile);
  it('dot: graphs/labelroot-fdc.gv', snapshotGvFile);
  it('dot: graphs/labelroot-fdd.gv', snapshotGvFile);
  it('dot: graphs/labelroot-fdl.gv', snapshotGvFile);
  it('dot: graphs/labelroot-fdr.gv', snapshotGvFile);
  it('dot: graphs/labelroot-ftc.gv', snapshotGvFile);
  it('dot: graphs/labelroot-ftd.gv', snapshotGvFile);
  it('dot: graphs/labelroot-ftl.gv', snapshotGvFile);
  it('dot: graphs/labelroot-ftr.gv', snapshotGvFile);
  it('dot: graphs/labelroot-nbc.gv', snapshotGvFile);
  it('dot: graphs/labelroot-nbd.gv', snapshotGvFile);
  it('dot: graphs/labelroot-nbl.gv', snapshotGvFile);
  it('dot: graphs/labelroot-nbr.gv', snapshotGvFile);
  it('dot: graphs/labelroot-ndc.gv', snapshotGvFile);
  it('dot: graphs/labelroot-ndd.gv', snapshotGvFile);
  it('dot: graphs/labelroot-ndl.gv', snapshotGvFile);
  it('dot: graphs/labelroot-ndr.gv', snapshotGvFile);
  it('dot: graphs/labelroot-ntc.gv', snapshotGvFile);
  it('dot: graphs/labelroot-ntd.gv', snapshotGvFile);
  it('dot: graphs/labelroot-ntl.gv', snapshotGvFile);
  it('dot: graphs/labelroot-ntr.gv', snapshotGvFile);

  // it('dot: graphs/Latin1.gv', snapshotGvFile);
  // it('dot: graphs/layer.gv', snapshotGvFile);
  // it('dot: graphs/layer2.gv', snapshotGvFile);
  // it('dot: graphs/layers.gv', snapshotGvFile);
  it('dot: graphs/ldbxtried.gv', snapshotGvFile);
  it('dot: graphs/longflat.gv', snapshotGvFile);

  it('dot: graphs/lsunix1.gv', snapshotGvFile);
  it('dot: graphs/lsunix2.gv', snapshotGvFile);
  it('dot: graphs/lsunix3.gv', snapshotGvFile);

  it('dot: graphs/mode.gv', snapshotGvFile);
  // # check mode=hier
  // Case("mode", Path("mode.gv"), "neato", "ps", ["-Gmode=KK"]),
  // Case("mode", Path("mode.gv"), "neato", "ps", ["-Gmode=hier"], 1),
  // Case("mode", Path("mode.gv"), "neato", "ps", ["-Gmode=hier", "-Glevelsgap=1"], 2),
  // Case("model", Path("mode.gv"), "neato", "ps", ["-Gmodel=circuit"]),
  // Case(
  //     "model",
  //     Path("mode.gv"),
  //     "neato",
  //     "ps",
  //     ["-Goverlap=false", "-Gmodel=subset"],
  //     1,
  // Case("page", Path("mode.gv"), "neato", "ps", ["-Gpage=8.5,11"]),
  // Case("page", Path("mode.gv"), "neato", "ps", ["-Gpage=8.5,11", "-Gpagedir=TL"], 1),
  // Case("page", Path("mode.gv"), "neato", "ps", ["-Gpage=8.5,11", "-Gpagedir=TR"], 2),
  // Case("size", Path("mode.gv"), "neato", "ps", ["-Gsize=5,5"]),
  // Case("size", Path("mode.gv"), "neato", "png", ["-Gsize=5,5"]),

  it('dot: graphs/multi.gv', snapshotGvFile);
  it('dot: graphs/NaN.gv', snapshotGvFile);
  it('dot: graphs/nestedclust.gv', snapshotGvFile);
  it('dot: graphs/newarrows.gv', snapshotGvFile);

  it('dot: graphs/ngk10_4.gv', snapshotGvFile);
  it('dot: graphs/nhg.gv', snapshotGvFile);
  it('dot: graphs/nojustify.gv', snapshotGvFile);
  it('dot: graphs/ordering.gv', snapshotGvFile);
  // Case("ordering", Path("ordering.gv"), "dot", "gv", ["-Gordering=in"]),
  // Case("ordering", Path("ordering.gv"), "dot", "gv", ["-Gordering=out"], 1),

  it('dot: graphs/overlap.gv', snapshotGvFile);
  // Case("overlap", Path("overlap.gv"), "neato", "gv", ["-Goverlap=false"]),
  // Case("overlap", Path("overlap.gv"), "neato", "gv", ["-Goverlap=scale"], 1),
  // Case(
  //     "neatosplines",
  //     Path("overlap.gv"),
  //     "neato",
  //     "gv",
  //     ["-Goverlap=false", "-Gsplines"],
  // ),
  // Case(
  //     "neatosplines",
  //     Path("overlap.gv"),
  //     "neato",
  //     "gv",
  //     ["-Goverlap=false", "-Gsplines=polyline"],
  //     1,
  // ),

  it('dot: graphs/p.gv', snapshotGvFile);
  it('dot: graphs/p2.gv', snapshotGvFile);
  it('dot: graphs/p3.gv', snapshotGvFile);
  it('dot: graphs/p4.gv', snapshotGvFile);

  it('dot: graphs/pack.gv', snapshotGvFile);
  // Case("pack", Path("pack.gv"), "neato", "gv", []),
  // Case("pack", Path("pack.gv"), "neato", "gv", ["-Gpack=20"], 1),
  // Case("pack", Path("pack.gv"), "neato", "gv", ["-Gpackmode=graph"], 2),

  it('dot: graphs/Petersen.gv', snapshotGvFile);
  it('dot: graphs/pgram.gv', snapshotGvFile);
  it('dot: graphs/pm2way.gv', snapshotGvFile);
  it('dot: graphs/pmpipe.gv', snapshotGvFile);
  it('dot: graphs/polypoly.gv', snapshotGvFile);
  it('dot: graphs/ports.gv', snapshotGvFile);
  it('dot: graphs/proc3d.gv', snapshotGvFile);
  it('dot: graphs/process.gv', snapshotGvFile);

  it('dot: graphs/rd_rules.gv', snapshotGvFile);
  it('dot: graphs/record.gv', snapshotGvFile);
  it('dot: graphs/record2.gv', snapshotGvFile);
  it('dot: graphs/records.gv', snapshotGvFile);

  it('dot: graphs/root.gv', snapshotGvFile);
  // Case("size_ex", Path("root.gv"), "dot", "ps", ["-Gsize=6,6!"]),
  // Case("size_ex", Path("root.gv"), "dot", "png", ["-Gsize=6,6!"]),
  // Case("root", Path("root.gv"), "twopi", "gv", []),

  it('dot: graphs/rootlabel.gv', snapshotGvFile);
  // Case(
  //     "rootlabel",
  //     Path("rootlabel.gv"),
  //     "dot",
  //     "ps",
  //     ["-Glabelloc=t", "-Glabeljust=r"],
  // ),
  // Case(
  //     "rootlabel",
  //     Path("rootlabel.gv"),
  //     "dot",
  //     "ps",
  //     ["-Glabelloc=b", "-Glabeljust=r"],
  //     1,
  // ),
  // Case(
  //     "rootlabel",
  //     Path("rootlabel.gv"),
  //     "dot",
  //     "ps",
  //     ["-Glabelloc=t", "-Glabeljust=l"],
  //     2,
  // ),
  // Case(
  //     "rootlabel",
  //     Path("rootlabel.gv"),
  //     "dot",
  //     "ps",
  //     ["-Glabelloc=b", "-Glabeljust=l"],
  //     3,
  // ),
  // Case(
  //     "rootlabel",
  //     Path("rootlabel.gv"),
  //     "dot",
  //     "ps",
  //     ["-Glabelloc=t", "-Glabeljust=c"],
  //     4,
  // ),
  // Case(
  //     "rootlabel",
  //     Path("rootlabel.gv"),
  //     "dot",
  //     "ps",
  //     ["-Glabelloc=b", "-Glabeljust=c"],
  //     5,
  // ),
  // Case("rootlabel", Path("rootlabel.gv"), "dot", "ps", ["-Glabelloc=t"], 6),
  // Case("rootlabel", Path("rootlabel.gv"), "dot", "ps", ["-Glabelloc=b"], 7),

  it('dot: graphs/rowcolsep.gv', snapshotGvFile);
  // Case("rowcolsep", Path("rowcolsep.gv"), "dot", "gv", ["-Gnodesep=0.5"]),
  // Case("rowcolsep", Path("rowcolsep.gv"), "dot", "gv", ["-Granksep=1.5"], 1),

  it('dot: graphs/rowe.gv', snapshotGvFile);

  it('dot: graphs/sb_box.gv', snapshotGvFile);
  it('dot: graphs/sb_box_dbl.gv', snapshotGvFile);
  it('dot: graphs/sr_box.gv', snapshotGvFile);
  it('dot: graphs/sr_box_dbl.gv', snapshotGvFile);
  it('dot: graphs/st_box.gv', snapshotGvFile);
  it('dot: graphs/st_box_dbl.gv', snapshotGvFile);
  it('dot: graphs/sl_box.gv', snapshotGvFile);
  it('dot: graphs/sl_box_dbl.gv', snapshotGvFile);
  it('dot: graphs/sb_circle.gv', snapshotGvFile);
  it('dot: graphs/sb_circle_dbl.gv', snapshotGvFile);
  it('dot: graphs/sl_circle.gv', snapshotGvFile);
  it('dot: graphs/sl_circle_dbl.gv', snapshotGvFile);
  it('dot: graphs/sr_circle.gv', snapshotGvFile);
  it('dot: graphs/sr_circle_dbl.gv', snapshotGvFile);
  it('dot: graphs/st_circle.gv', snapshotGvFile);
  it('dot: graphs/st_circle_dbl.gv', snapshotGvFile);

  it('dot: graphs/shapes.gv', snapshotGvFile);
  it('dot: graphs/shells.gv', snapshotGvFile);
  it('dot: graphs/sides.gv', snapshotGvFile);
  it('dot: graphs/size.gv', snapshotGvFile);
  // Case("dotsplines", Path("size.gv"), "dot", "gv", ["-Gsplines=line"]),
  // Case("dotsplines", Path("size.gv"), "dot", "gv", ["-Gsplines=polyline"], 1),

  it('dot: graphs/sq_rules.gv', snapshotGvFile);
  it('dot: graphs/states.gv', snapshotGvFile);
  it('dot: graphs/structs.gv', snapshotGvFile);
  it('dot: graphs/style.gv', snapshotGvFile);

  it('dot: graphs/train11.gv', snapshotGvFile);
  it('dot: graphs/trapeziumlr.gv', snapshotGvFile);
  it('dot: graphs/tree.gv', snapshotGvFile);
  it('dot: graphs/triedds.gv', snapshotGvFile);
  it('dot: graphs/try.gv', snapshotGvFile);
  it('dot: graphs/unix.gv', snapshotGvFile);

  // FIXME: it('dot: graphs/url.gv', snapshotGvFile);
  // Case("url", Path("url.gv"), "dot", "svg", ["-Gstylesheet=stylesheet"]),

  // it('dot: graphs/user_shapes.gv', snapshotGvFile); use 'dot: graphs/jcr.gif'
  // Case("user_shapes", Path("user_shapes.gv"), "dot", "ps", []),

  it('dot: graphs/viewfile.gv', snapshotGvFile);

  it('dot: graphs/viewport.gv', snapshotGvFile);
  // Case(
  //     "viewport", Path("viewport.gv"), "neato", "png", ["-Gviewport=300,300", "-n2"]
  // ),
  // Case("viewport", Path("viewport.gv"), "neato", "ps", ["-Gviewport=300,300", "-n2"]),
  // Case(
  //     "viewport",
  //     Path("viewport.gv"),
  //     "neato",
  //     "png",
  //     ["-Gviewport=300,300,1,200,620", "-n2"],
  //     1,
  // ),
  // Case(
  //     "viewport",
  //     Path("viewport.gv"),
  //     "neato",
  //     "ps",
  //     ["-Gviewport=300,300,1,200,620", "-n2"],
  //     1,
  // ),
  // Case(
  //     "viewport",
  //     Path("viewport.gv"),
  //     "neato",
  //     "png",
  //     ["-Gviewport=300,300,2,200,620", "-n2"],
  //     2,
  // ),
  // Case(
  //     "viewport",
  //     Path("viewport.gv"),
  //     "neato",
  //     "ps",
  //     ["-Gviewport=300,300,2,200,620", "-n2"],
  //     2,
  // ),

  it('dot: graphs/weight.gv', snapshotGvFile);
  it('dot: graphs/world.gv', snapshotGvFile);

  it('dot: graphs/xlabels.gv', snapshotGvFile);
  // Case("xlabels", Path("xlabels.gv"), "dot", "png", []),
  // Case("xlabels", Path("xlabels.gv"), "neato", "png", []),

  it('dot: graphs/xx.gv', snapshotGvFile);

  // #Since this test relies on absolute pathnames, it is necessary to run a
  // #shell script to generate the test graphs.
  // #Run imagepath_test.sh from within the imagepath_test directory before running
  // #rtest on this test suite. This script creates the input graphs and output png
  // #files and stores them in the graphs and imagepath_test/nshare directories.
  // #The png files may be copied into the nshare directory by running
  // #imagepath_test/save_png_files.sh when it is appropriate to update the expected
  // #test output.
  //
  // #The graphs in this test suite should also be tested using the mac os user
  // #interface.  Copy rtest/imagepath_test/image.jpg to the directory graphviz is run
  // #from. Select each of the test graphs from the graphviz application and visually
  // #determine whether the tests complete successfully.  The image that is displayed
  // #during the test should correspond to the expected result message that appears
  // #below it.

  // it('dot: graphs/val_inv.gv', snapshotGvFile);
  // it('dot: graphs/val_nul.gv', snapshotGvFile);
  // it('dot: graphs/val_val.gv', snapshotGvFile);

  // it('dot: graphs/inv_inv.gv', snapshotGvFile);
  // it('dot: graphs/inv_nul.gv', snapshotGvFile);
  // it('dot: graphs/inv_val.gv', snapshotGvFile);

  // it('dot: graphs/nul_inv.gv', snapshotGvFile);
  // it('dot: graphs/nul_nul.gv', snapshotGvFile);
  // it('dot: graphs/nul_val.gv', snapshotGvFile);
  /* spell-checker: enable */
});

const graphvizSnapshotDir = fileURLToPath(import.meta.resolve('./graphviz'));
const fontWarningRegExp =
  /^Warning: no hard-coded metrics for '[^']+'. {2}Falling back to 'Times' metrics$/;
const asciiWarningRegExp =
  /^Warning: no value for width of non-ASCII character [0-9]+. Falling back to width of space character$/;

async function snapshotGvFile({ task }: TestContext) {
  const [engine, gvFile] = task.name.split(': ');
  if (!gvFile) {
    throw new Error(`Incorrectly formatted test name: ${task.name}`);
  }

  const gvPath = path.join(graphvizSnapshotDir, gvFile);
  const gvString = fs.readFileSync(gvPath, 'utf8');
  const result = await renderFormats(gvString, ['dot', 'svg'], { engine });
  const errors = result.errors.filter(
    (e) =>
      fontWarningRegExp.test(e.message) || asciiWarningRegExp.test(e.message),
  );

  const output = result.output;
  expect(result).toStrictEqual({ status: 'success', output, errors });

  const dot = output?.dot;
  const svg = useVizJS
    ? output?.svg.replaceAll(
        'Generated by graphviz version 13.1.2 (20250808.2320)',
        'Generated by graphviz version a (a)',
      )
    : output?.svg;

  const basePath = gvPath.replace(/\.gv$/, `-snapshots/${engine}_engine`);
  await expectString(dot).toMatchFileSnapshot(basePath + '.dot');
  await expectString(svg).toMatchFileSnapshot(basePath + '.svg');
}
