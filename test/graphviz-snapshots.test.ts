import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import * as VizPackage from '@viz-js/viz';
import { describe, expect, it, type TestContext } from 'vitest';

import * as DotVizPackage from '../src/index.ts';
import { expectString } from './util/raw-string-serializer.ts';

const useVizJS = process.env.USE_VIZ_JS != null;
const renderFormats = useVizJS
  ? async (input: string, formats: string[]) => {
      const instance = await VizPackage.instance();
      return instance.renderFormats(input, formats);
    }
  : async (input: string, formats: string[]) => {
      const instance = await DotVizPackage.instance();
      return instance.renderFormats(input, formats);
    };

// FIXME: many files are modified with replaced fonts and removed unicode symbols
// update after we fully support native fonts
describe('GraphViz Gallery', () => {
  /* spell-checker: disable */
  describe('directed', () => {
    it('gallery/directed/bazel.gv', snapshotGvFile);
    it('gallery/directed/cluster.gv', snapshotGvFile);
    it('gallery/directed/crazy.gv', snapshotGvFile);
    it('gallery/directed/datastruct.gv', snapshotGvFile);
    it('gallery/directed/fsm.gv', snapshotGvFile);
    it('gallery/directed/Genetic_Programming.gv', snapshotGvFile);
    it('gallery/directed/git.gv', snapshotGvFile);
    it('gallery/directed/go-package.gv', snapshotGvFile);
    it('gallery/directed/hello.gv', snapshotGvFile);
    it.skip('gallery/directed/kennedyanc.gv', snapshotGvFile);
    it('gallery/directed/Linux_kernel_diagram.gv', snapshotGvFile);
    it('gallery/directed/lion_share.gv', snapshotGvFile);
    it('gallery/directed/neural-network.gv', snapshotGvFile);
    it('gallery/directed/ninja.gv', snapshotGvFile);
    it('gallery/directed/pprof.gv', snapshotGvFile);
    it('gallery/directed/profile.gv', snapshotGvFile);
    it('gallery/directed/psg.gv', snapshotGvFile);
    it('gallery/directed/sdh.gv', snapshotGvFile);

    // FIXME: viz-js also crashes on bellow graph
    // digraph {graph[label="\nKappa Kappa Psi/Tau Beta Sigma\nSan Diego State University"]}
    it.skip('gallery/directed/siblings.gv', snapshotGvFile);

    it('gallery/directed/switch.gv', snapshotGvFile);
    it('gallery/directed/UML_Class_diagram.gv', snapshotGvFile);
    it('gallery/directed/unix.gv', snapshotGvFile);
    it('gallery/directed/world.gv', snapshotGvFile);
  });
  describe.skip('fdp', () => {
    it('gallery/fdp/fdpclust.gv', snapshotGvFile);
  });
  describe('gradient', () => {
    it('gallery/gradient/angles.gv', snapshotGvFile);
    it('gallery/gradient/cluster.gv', snapshotGvFile);
    it('gallery/gradient/colors.gv', snapshotGvFile);
    it('gallery/gradient/datastruct.gv', snapshotGvFile);
    it('gallery/gradient/g_c_n.gv', snapshotGvFile);
    it('gallery/gradient/linear_angle.gv', snapshotGvFile);
    it('gallery/gradient/radial_angle.gv', snapshotGvFile);
    it('gallery/gradient/table.gv', snapshotGvFile);
  });
  describe.only('neato', () => {
    it('gallery/neato/color_wheel.gv', snapshotGvFile);
    it('gallery/neato/colors.gv', snapshotGvFile);
    it('gallery/neato/ER.gv', snapshotGvFile);
    it('gallery/neato/philo.gv', snapshotGvFile);
    it('gallery/neato/process.gv', snapshotGvFile);
    it('gallery/neato/softmaint.gv', snapshotGvFile);
    it('gallery/neato/traffic_lights.gv', snapshotGvFile);
    it('gallery/neato/transparency.gv', snapshotGvFile);
  });
  describe.skip('sfdp', () => {
    it('gallery/sfdp/root.gv', snapshotGvFile);
  });
  describe.skip('twopi', () => {
    it('gallery/twopi/happiness.gv', snapshotGvFile);
    it('gallery/twopi/networkmap_twopi.gv', snapshotGvFile);
    it('gallery/twopi/twopi2.gv', snapshotGvFile);
  });
  describe('undirected', () => {
    it('gallery/undirected/gd_1994_2007.gv', snapshotGvFile);
    it('gallery/undirected/grid.gv', snapshotGvFile);
    it('gallery/undirected/inet.gv', snapshotGvFile);
  });
  /* spell-checker: enable */
});

describe('fonts', () => {
  /* spell-checker: disable */
  it('fonts/AvantGarde.gv', snapshotGvFile);
  it('fonts/Bookman.gv', snapshotGvFile);
  it('fonts/Helvetica.gv', snapshotGvFile);
  it('fonts/NewCenturySchlbk.gv', snapshotGvFile);
  it('fonts/Palatino.gv', snapshotGvFile);
  it('fonts/Symbol.gv', snapshotGvFile);
  it('fonts/Times.gv', snapshotGvFile);
  it('fonts/ZapfChancery.gv', snapshotGvFile);
  it('fonts/ZapfDingbats.gv', snapshotGvFile);
  /* spell-checker: enable */
});
describe('languages', () => {
  /* spell-checker: disable */
  it('languages/japanese.gv', snapshotGvFile);
  it('languages/russian.gv', snapshotGvFile);
  /* spell-checker: enable */
});

describe('fonts', () => {
  /* spell-checker: disable */
  it('graphs/a.gv', snapshotGvFile);
  it('graphs/abstract.gv', snapshotGvFile);
  it('graphs/alf.gv', snapshotGvFile);
  it('graphs/arrows.gv', snapshotGvFile);
  it('graphs/arrowsize.gv', snapshotGvFile);
  it('graphs/awilliams.gv', snapshotGvFile);
  it('graphs/b.gv', snapshotGvFile);
  it('graphs/b3.gv', snapshotGvFile);
  it('graphs/b7.gv', snapshotGvFile);
  // it('graphs/b15.gv', snapshotGvFile);
  it('graphs/b22.gv', snapshotGvFile);
  it('graphs/b29.gv', snapshotGvFile);
  it('graphs/b33.gv', snapshotGvFile);
  // FIXME it('graphs/b34.gv', snapshotGvFile);
  it('graphs/b36.gv', snapshotGvFile);
  it('graphs/b51.gv', snapshotGvFile);
  it('graphs/b53.gv', snapshotGvFile);
  // FIXME it.only('graphs/b56.gv', snapshotGvFile);
  it('graphs/b57.gv', snapshotGvFile);
  it('graphs/b58.gv', snapshotGvFile);
  // FIXME it.only('graphs/b60.gv', snapshotGvFile);
  it('graphs/b62.gv', snapshotGvFile);
  it('graphs/b68.gv', snapshotGvFile);
  it('graphs/b69.gv', snapshotGvFile);
  it('graphs/b71.gv', snapshotGvFile);
  it('graphs/b73.gv', snapshotGvFile);
  it('graphs/b73a.gv', snapshotGvFile);
  it('graphs/b76.gv', snapshotGvFile);
  it('graphs/b77.gv', snapshotGvFile);
  it('graphs/b79.gv', snapshotGvFile);
  it('graphs/b80.gv', snapshotGvFile);
  it('graphs/b80a.gv', snapshotGvFile);
  it('graphs/b81.gv', snapshotGvFile);
  it('graphs/b85.gv', snapshotGvFile);
  it('graphs/b94.gv', snapshotGvFile);
  // FIXME runs for 16sec it.only('graphs/b100.gv', snapshotGvFile);
  it('graphs/b102.gv', snapshotGvFile);
  it('graphs/b103.gv', snapshotGvFile);
  // FIXME runs for 15sec it.only('graphs/b104.gv', snapshotGvFile);
  it('graphs/b106.gv', snapshotGvFile);
  it('graphs/b117.gv', snapshotGvFile);
  it('graphs/b123.gv', snapshotGvFile);
  it('graphs/b124.gv', snapshotGvFile);
  it('graphs/b135.gv', snapshotGvFile);
  it('graphs/b143.gv', snapshotGvFile);
  // it('graphs/b145.gv', snapshotGvFile);
  it('graphs/b146.gv', snapshotGvFile);
  it('graphs/b155.gv', snapshotGvFile);
  it('graphs/b491.gv', snapshotGvFile);
  it('graphs/b545.gv', snapshotGvFile);
  it('graphs/b786.gv', snapshotGvFile);
  it('graphs/b993.gv', snapshotGvFile);
  it('graphs/bad.gv', snapshotGvFile);
  it('graphs/badvoro.gv', snapshotGvFile);
  it('graphs/big.gv', snapshotGvFile);
  it('graphs/biglabel.gv', snapshotGvFile);
  it('graphs/center.gv', snapshotGvFile);
  it('graphs/clover.gv', snapshotGvFile);
  // it.only('graphs/clust.gv', snapshotGvFile);
  it('graphs/clust1.gv', snapshotGvFile);
  it('graphs/clust2.gv', snapshotGvFile);
  it('graphs/clust3.gv', snapshotGvFile);
  it('graphs/clust4.gv', snapshotGvFile);
  it('graphs/clust5.gv', snapshotGvFile);
  it('graphs/clusters.gv', snapshotGvFile);
  it('graphs/clustlabel.gv', snapshotGvFile);
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

  it('graphs/color.gv', snapshotGvFile);
  // # color encodings
  // # multiple edge colors
  // Case("color", Path("color.gv"), "dot", "png", []),
  // Case("color", Path("color.gv"), "dot", "png", ["-Gbgcolor=lightblue"]),

  // pencolor, fontcolor, fillcolor
  it('graphs/colors.gv', snapshotGvFile);
  it('graphs/colorscheme.gv', snapshotGvFile);
  it('graphs/compound.gv', snapshotGvFile);
  it('graphs/crazy.gv', snapshotGvFile);
  // Case("rotate", Path("crazy.gv"), "dot", "ps", ["-Glandscape"]),
  // Case("rotate", Path("crazy.gv"), "dot", "ps", ["-Grotate=90"], 1),
  // Case("rankdir", Path("crazy.gv"), "dot", "gv", ["-Grankdir=LR"]),
  // Case("rankdir", Path("crazy.gv"), "dot", "gv", ["-Grankdir=BT"], 1),
  // Case("rankdir", Path("crazy.gv"), "dot", "gv", ["-Grankdir=RL"], 2),

  it('graphs/ctext.gv', snapshotGvFile);
  it('graphs/d.gv', snapshotGvFile);
  it('graphs/dd.gv', snapshotGvFile);
  it('graphs/decorate.gv', snapshotGvFile);
  it('graphs/dfa.gv', snapshotGvFile);
  it('graphs/dir.gv', snapshotGvFile);
  it('graphs/dpd.gv', snapshotGvFile);
  it('graphs/edgeclip.gv', snapshotGvFile);
  it('graphs/ER.gv', snapshotGvFile);
  // FDP it('graphs/fdp.gv', snapshotGvFile);
  it('graphs/fig6.gv', snapshotGvFile);
  it('graphs/flatedge.gv', snapshotGvFile);
  it('graphs/fsm.gv', snapshotGvFile);
  it('graphs/grammar.gv', snapshotGvFile);
  it('graphs/grdangles.gv', snapshotGvFile);
  it('graphs/grdcluster.gv', snapshotGvFile);
  it('graphs/grdcolors.gv', snapshotGvFile);
  it('graphs/grdfillcolor.gv', snapshotGvFile);
  it('graphs/grdlinear_angle.gv', snapshotGvFile);
  it('graphs/grdlinear_node.gv', snapshotGvFile);
  it('graphs/grdlinear.gv', snapshotGvFile);
  it('graphs/grdradial_angle.gv', snapshotGvFile);
  it('graphs/grdradial_node.gv', snapshotGvFile);
  it('graphs/grdradial.gv', snapshotGvFile);
  it('graphs/grdshapes.gv', snapshotGvFile);
  it('graphs/hashtable.gv', snapshotGvFile);
  it('graphs/Heawood.gv', snapshotGvFile);
  it('graphs/honda-tokoro.gv', snapshotGvFile);
  it('graphs/html.gv', snapshotGvFile);
  // FIXME it('graphs/html2.gv', snapshotGvFile);
  it('graphs/in.gv', snapshotGvFile);
  it('graphs/jcctree.gv', snapshotGvFile);
  it('graphs/jsort.gv', snapshotGvFile);
  it('graphs/KW91.gv', snapshotGvFile);

  // FIXME use parametrise testing
  it('graphs/labelclust-fbc.gv', snapshotGvFile);
  it('graphs/labelclust-fbd.gv', snapshotGvFile);
  it('graphs/labelclust-fbl.gv', snapshotGvFile);
  it('graphs/labelclust-fbr.gv', snapshotGvFile);
  it('graphs/labelclust-fdc.gv', snapshotGvFile);
  it('graphs/labelclust-fdd.gv', snapshotGvFile);
  it('graphs/labelclust-fdl.gv', snapshotGvFile);
  it('graphs/labelclust-fdr.gv', snapshotGvFile);
  it('graphs/labelclust-ftc.gv', snapshotGvFile);
  it('graphs/labelclust-ftd.gv', snapshotGvFile);
  it('graphs/labelclust-ftl.gv', snapshotGvFile);
  it('graphs/labelclust-ftr.gv', snapshotGvFile);
  it('graphs/labelclust-nbc.gv', snapshotGvFile);
  it('graphs/labelclust-nbd.gv', snapshotGvFile);
  it('graphs/labelclust-nbl.gv', snapshotGvFile);
  it('graphs/labelclust-nbr.gv', snapshotGvFile);
  it('graphs/labelclust-ndc.gv', snapshotGvFile);
  it('graphs/labelclust-ndd.gv', snapshotGvFile);
  it('graphs/labelclust-ndl.gv', snapshotGvFile);
  it('graphs/labelclust-ndr.gv', snapshotGvFile);
  it('graphs/labelclust-ntc.gv', snapshotGvFile);
  it('graphs/labelclust-ntd.gv', snapshotGvFile);
  it('graphs/labelclust-ntl.gv', snapshotGvFile);
  it('graphs/labelclust-ntr.gv', snapshotGvFile);

  // FIXME use parametrise testing
  it('graphs/labelroot-fbc.gv', snapshotGvFile);
  it('graphs/labelroot-fbd.gv', snapshotGvFile);
  it('graphs/labelroot-fbl.gv', snapshotGvFile);
  it('graphs/labelroot-fbr.gv', snapshotGvFile);
  it('graphs/labelroot-fdc.gv', snapshotGvFile);
  it('graphs/labelroot-fdd.gv', snapshotGvFile);
  it('graphs/labelroot-fdl.gv', snapshotGvFile);
  it('graphs/labelroot-fdr.gv', snapshotGvFile);
  it('graphs/labelroot-ftc.gv', snapshotGvFile);
  it('graphs/labelroot-ftd.gv', snapshotGvFile);
  it('graphs/labelroot-ftl.gv', snapshotGvFile);
  it('graphs/labelroot-ftr.gv', snapshotGvFile);
  it('graphs/labelroot-nbc.gv', snapshotGvFile);
  it('graphs/labelroot-nbd.gv', snapshotGvFile);
  it('graphs/labelroot-nbl.gv', snapshotGvFile);
  it('graphs/labelroot-nbr.gv', snapshotGvFile);
  it('graphs/labelroot-ndc.gv', snapshotGvFile);
  it('graphs/labelroot-ndd.gv', snapshotGvFile);
  it('graphs/labelroot-ndl.gv', snapshotGvFile);
  it('graphs/labelroot-ndr.gv', snapshotGvFile);
  it('graphs/labelroot-ntc.gv', snapshotGvFile);
  it('graphs/labelroot-ntd.gv', snapshotGvFile);
  it('graphs/labelroot-ntl.gv', snapshotGvFile);
  it('graphs/labelroot-ntr.gv', snapshotGvFile);

  // it('graphs/Latin1.gv', snapshotGvFile);
  // it('graphs/layer.gv', snapshotGvFile);
  // it('graphs/layer2.gv', snapshotGvFile);
  // it('graphs/layers.gv', snapshotGvFile);
  it('graphs/ldbxtried.gv', snapshotGvFile);
  it('graphs/longflat.gv', snapshotGvFile);

  it('graphs/lsunix1.gv', snapshotGvFile);
  it('graphs/lsunix2.gv', snapshotGvFile);
  it('graphs/lsunix3.gv', snapshotGvFile);

  it('graphs/mode.gv', snapshotGvFile);
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

  it('graphs/multi.gv', snapshotGvFile);
  it('graphs/NaN.gv', snapshotGvFile);
  it('graphs/nestedclust.gv', snapshotGvFile);
  it('graphs/newarrows.gv', snapshotGvFile);

  it('graphs/ngk10_4.gv', snapshotGvFile);
  it('graphs/nhg.gv', snapshotGvFile);
  it('graphs/nojustify.gv', snapshotGvFile);
  it('graphs/ordering.gv', snapshotGvFile);
  // Case("ordering", Path("ordering.gv"), "dot", "gv", ["-Gordering=in"]),
  // Case("ordering", Path("ordering.gv"), "dot", "gv", ["-Gordering=out"], 1),

  it('graphs/overlap.gv', snapshotGvFile);
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

  it('graphs/p.gv', snapshotGvFile);
  it('graphs/p2.gv', snapshotGvFile);
  it('graphs/p3.gv', snapshotGvFile);
  it('graphs/p4.gv', snapshotGvFile);

  it('graphs/pack.gv', snapshotGvFile);
  // Case("pack", Path("pack.gv"), "neato", "gv", []),
  // Case("pack", Path("pack.gv"), "neato", "gv", ["-Gpack=20"], 1),
  // Case("pack", Path("pack.gv"), "neato", "gv", ["-Gpackmode=graph"], 2),

  it('graphs/Petersen.gv', snapshotGvFile);
  it('graphs/pgram.gv', snapshotGvFile);
  it('graphs/pm2way.gv', snapshotGvFile);
  it('graphs/pmpipe.gv', snapshotGvFile);
  it('graphs/polypoly.gv', snapshotGvFile);
  it('graphs/ports.gv', snapshotGvFile);
  it('graphs/proc3d.gv', snapshotGvFile);
  it('graphs/process.gv', snapshotGvFile);

  it('graphs/rd_rules.gv', snapshotGvFile);
  it('graphs/record.gv', snapshotGvFile);
  it('graphs/record2.gv', snapshotGvFile);
  it('graphs/records.gv', snapshotGvFile);

  it('graphs/root.gv', snapshotGvFile);
  // Case("size_ex", Path("root.gv"), "dot", "ps", ["-Gsize=6,6!"]),
  // Case("size_ex", Path("root.gv"), "dot", "png", ["-Gsize=6,6!"]),
  // Case("root", Path("root.gv"), "twopi", "gv", []),

  it('graphs/rootlabel.gv', snapshotGvFile);
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

  it('graphs/rowcolsep.gv', snapshotGvFile);
  // Case("rowcolsep", Path("rowcolsep.gv"), "dot", "gv", ["-Gnodesep=0.5"]),
  // Case("rowcolsep", Path("rowcolsep.gv"), "dot", "gv", ["-Granksep=1.5"], 1),

  it('graphs/rowe.gv', snapshotGvFile);

  it('graphs/sb_box.gv', snapshotGvFile);
  it('graphs/sb_box_dbl.gv', snapshotGvFile);
  it('graphs/sr_box.gv', snapshotGvFile);
  it('graphs/sr_box_dbl.gv', snapshotGvFile);
  it('graphs/st_box.gv', snapshotGvFile);
  it('graphs/st_box_dbl.gv', snapshotGvFile);
  it('graphs/sl_box.gv', snapshotGvFile);
  it('graphs/sl_box_dbl.gv', snapshotGvFile);
  it('graphs/sb_circle.gv', snapshotGvFile);
  it('graphs/sb_circle_dbl.gv', snapshotGvFile);
  it('graphs/sl_circle.gv', snapshotGvFile);
  it('graphs/sl_circle_dbl.gv', snapshotGvFile);
  it('graphs/sr_circle.gv', snapshotGvFile);
  it('graphs/sr_circle_dbl.gv', snapshotGvFile);
  it('graphs/st_circle.gv', snapshotGvFile);
  it('graphs/st_circle_dbl.gv', snapshotGvFile);

  it('graphs/shapes.gv', snapshotGvFile);
  it('graphs/shells.gv', snapshotGvFile);
  it('graphs/sides.gv', snapshotGvFile);
  it('graphs/size.gv', snapshotGvFile);
  // Case("dotsplines", Path("size.gv"), "dot", "gv", ["-Gsplines=line"]),
  // Case("dotsplines", Path("size.gv"), "dot", "gv", ["-Gsplines=polyline"], 1),

  it('graphs/sq_rules.gv', snapshotGvFile);
  it('graphs/states.gv', snapshotGvFile);
  it('graphs/structs.gv', snapshotGvFile);
  it('graphs/style.gv', snapshotGvFile);

  it('graphs/train11.gv', snapshotGvFile);
  it('graphs/trapeziumlr.gv', snapshotGvFile);
  it('graphs/tree.gv', snapshotGvFile);
  it('graphs/triedds.gv', snapshotGvFile);
  it('graphs/try.gv', snapshotGvFile);
  it('graphs/unix.gv', snapshotGvFile);

  // FIXME: it('graphs/url.gv', snapshotGvFile);
  // Case("url", Path("url.gv"), "dot", "svg", ["-Gstylesheet=stylesheet"]),

  // it('graphs/user_shapes.gv', snapshotGvFile); use 'graphs/jcr.gif'
  // Case("user_shapes", Path("user_shapes.gv"), "dot", "ps", []),

  it('graphs/viewfile.gv', snapshotGvFile);

  it('graphs/viewport.gv', snapshotGvFile);
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

  it('graphs/weight.gv', snapshotGvFile);
  it('graphs/world.gv', snapshotGvFile);

  it('graphs/xlabels.gv', snapshotGvFile);
  // Case("xlabels", Path("xlabels.gv"), "dot", "png", []),
  // Case("xlabels", Path("xlabels.gv"), "neato", "png", []),

  it('graphs/xx.gv', snapshotGvFile);

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

  // it('graphs/val_inv.gv', snapshotGvFile);
  // it('graphs/val_nul.gv', snapshotGvFile);
  // it('graphs/val_val.gv', snapshotGvFile);

  // it('graphs/inv_inv.gv', snapshotGvFile);
  // it('graphs/inv_nul.gv', snapshotGvFile);
  // it('graphs/inv_val.gv', snapshotGvFile);

  // it('graphs/nul_inv.gv', snapshotGvFile);
  // it('graphs/nul_nul.gv', snapshotGvFile);
  // it('graphs/nul_val.gv', snapshotGvFile);
  /* spell-checker: enable */
});

const graphvizSnapshotDir = fileURLToPath(import.meta.resolve('./graphviz'));
const fontWarningRegExp =
  /^Warning: no hard-coded metrics for '[^']+'. {2}Falling back to 'Times' metrics$/;
const asciiWarningRegExp =
  /^Warning: no value for width of non-ASCII character [0-9]+. Falling back to width of space character$/;
async function snapshotGvFile({ task }: TestContext) {
  const gvPath = path.join(graphvizSnapshotDir, task.name);
  const gvFile = fs.readFileSync(gvPath, 'utf8');
  const result = await renderFormats(gvFile, ['dot', 'svg']);
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

  const basePath = gvPath.replace(/\.gv$/, '');
  await expectString(dot).toMatchFileSnapshot(basePath + '.dot');
  await expectString(svg).toMatchFileSnapshot(basePath + '.svg');
}
