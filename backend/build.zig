const std = @import("std");
const zcc = @import("compile_commands");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{
        .default_target = .{
            .cpu_arch = .wasm32,
            .os_tag = .wasi,
        },
    });

    const optimize = b.standardOptimizeOption(.{});
    const root_module = b.createModule(.{
        .root_source_file = b.path("src/wasm_module.zig"),
        .target = target,
        .optimize = optimize,
    });
    root_module.export_symbol_names = &.{
        "wasm_alloc",
        "wasm_free",
        "render",
    };

    if (target.result.os.tag == .wasi) {
        root_module.addCMacro("_WASI_EMULATED_SIGNAL", "");
        root_module.linkSystemLibrary("wasi-emulated-signal", .{});
        root_module.addCMacro("_WASI_EMULATED_PROCESS_CLOCKS", "");
        root_module.linkSystemLibrary("wasi-emulated-process-clocks", .{});
        root_module.addCMacro("_WASI_EMULATED_MMAN", "");
        root_module.linkSystemLibrary("wasi-emulated-mman", .{});
        root_module.addCMacro("_WASI_EMULATED_GETPID", "");
        root_module.linkSystemLibrary("wasi-emulated-getpid", .{});
    }

    const expat_dep = b.dependency("libexpat", .{
        .target = target,
        .optimize = optimize,
    });
    root_module.linkLibrary(expat_dep.artifact("expat"));
    root_module.addIncludePath(expat_dep.path("lib"));

    root_module.addIncludePath(b.path("src/graphviz_build/"));
    root_module.addCSourceFiles(.{
        .files = &.{
            "src/graphviz_build/drand48.c",
            "src/graphviz_build/qsort.c",
            "src/graphviz_build/common/htmlparse.c",
        },
    });

    root_module.addIncludePath(b.path("graphviz-fork/lib/"));
    root_module.addIncludePath(b.path("graphviz-fork/lib/common"));
    root_module.addIncludePath(b.path("graphviz-fork/lib/util"));
    root_module.addIncludePath(b.path("graphviz-fork/lib/cgraph"));
    root_module.addIncludePath(b.path("graphviz-fork/lib/cdt"));
    root_module.addIncludePath(b.path("graphviz-fork/lib/pathplan"));
    root_module.addIncludePath(b.path("graphviz-fork/lib/gvc"));

    root_module.addCSourceFiles(.{
        .root = b.path("graphviz-fork/lib/"),
        .files = &graphviz_lib_files,
    });

    root_module.addIncludePath(b.path("src"));
    root_module.addCSourceFiles(.{
        .files = &.{
            "src/cgraph_wrapper.c",
            "src/layout_inline.c",
            "src/context_inline.c",
            "src/output_string.c",
            "src/inline_render_svg/gvdevice.c",
            "src/inline_render_svg/init_bb.c",
            "src/inline_render_svg/render_svg.c",
            "src/inline_render_svg/emit_svg.c",
            "src/inline_render_svg/core_svg.c",
            "src/inline_render_svg/htmltable.c",
            "src/inline_render_svg/shapes.c",
            "src/inline_render_svg/arrows.c",
            "src/inline_render_svg/labels.c",
            "src/inline_render_dot/render_inline_dot.c",
            "src/inline_render_dot/output_dot.c",
            "src/inline_render_dot/write_c_inline.c",
            "src/gvusershape_size.c",
            "src/graphviz_deps.c",
        },
        .flags = &.{ "-Wall", "-Werror", "-Wextra" },
    });

    var exe = b.addExecutable(.{
        .name = "dotviz",
        .root_module = root_module,
    });
    exe.entry = .disabled;
    exe.lto = .full;
    exe.stack_size = 16 * 1024 * 1024;
    b.installArtifact(exe);

    var targets: std.ArrayList(*std.Build.Step.Compile) = .empty;
    targets.append(b.allocator, exe) catch @panic("OOM");
    _ = zcc.createStep(b, "cdb", targets.toOwnedSlice(b.allocator) catch @panic("OOM"));
}

const graphviz_lib_files = [_][]const u8{
    "cdt/dtview.c",
    "cdt/dtwalk.c",
    "cdt/dtrestore.c",
    "cdt/dttree.c",
    "cdt/dtclose.c",
    "cdt/dtrenew.c",
    "cdt/dtstrhash.c",
    "cdt/dtflatten.c",
    "cdt/dtextract.c",
    "cdt/dtsize.c",
    "cdt/dthash.c",
    "cdt/dtopen.c",
    "cdt/dtmethod.c",
    "cdt/dtstat.c",
    "cdt/dtdisc.c",

    "cgraph/imap.c",
    "cgraph/rec.c",
    "cgraph/subg.c",
    "cgraph/ingraphs.c",
    "cgraph/apply.c",
    "cgraph/agerror.c",
    "cgraph/graph.c",
    "cgraph/id.c",
    "cgraph/edge.c",
    "cgraph/utils.c",
    "cgraph/obj.c",
    "cgraph/unflatten.c",
    "cgraph/acyclic.c",
    "cgraph/refstr.c",
    "cgraph/tred.c",
    "cgraph/node.c",
    "cgraph/node_induce.c",
    "cgraph/attr.c",
    "cgraph/io.c",

    "common/splines.c",
    "common/htmllex.c",
    "common/colxlate.c",
    "common/textspan_lut.c",
    "common/postproc.c",
    "common/taper.c",
    "common/globals.c",
    "common/timing.c",
    "common/psusershape.c",
    "common/textspan.c",
    "common/utils.c",
    "common/args.c",
    "common/routespl.c",
    "common/pointset.c",
    "common/ns.c",
    "common/ellipse.c",
    "common/geom.c",
    "common/input.c",

    "util/gv_fopen.c",
    "util/gv_find_me.c",
    "util/random.c",
    "util/base64.c",

    "gvc/gvtextlayout.c",
    "gvc/gvjobs.c",
    "gvc/gvlayout.c",
    "gvc/gvplugin.c",
    "gvc/gvc.c",
    "gvc/gvconfig.c",
    "gvc/gvcontext.c",
    "gvc/gvtool_tred.c",
    "gvc/gvloadimage.c",

    "xdot/xdot.c",

    "label/index.c",
    "label/split.q.c",
    "label/xlabels.c",
    "label/rectangle.c",
    "label/node.c",

    "pathplan/triang.c",
    "pathplan/util.c",
    "pathplan/inpoly.c",
    "pathplan/visibility.c",
    "pathplan/shortest.c",
    "pathplan/cvt.c",
    "pathplan/route.c",
    "pathplan/solvers.c",
    "pathplan/shortestpth.c",

    "dotgen/dotinit.c",
    "dotgen/class1.c",
    "dotgen/fastgr.c",
    "dotgen/cluster.c",
    "dotgen/aspect.c",
    "dotgen/mincross.c",
    "dotgen/acyclic.c",
    "dotgen/decomp.c",
    "dotgen/dotsplines.c",
    "dotgen/compound.c",
    "dotgen/rank.c",
    "dotgen/class2.c",
    "dotgen/flat.c",
    "dotgen/sameport.c",
    "dotgen/conc.c",
    "dotgen/position.c",

    "circogen/block.c",
    "circogen/blockpath.c",
    "circogen/blocktree.c",
    "circogen/circpos.c",
    "circogen/circular.c",
    "circogen/circularinit.c",
    "circogen/edgelist.c",
    "circogen/nodelist.c",

    "neatogen/neatoinit.c",
    "neatogen/adjust.c",
    "neatogen/neatosplines.c",
    "neatogen/constraint.c",
    "neatogen/geometry.c",
    "neatogen/poly.c",
    "neatogen/voronoi.c",
    "neatogen/edges.c",
    "neatogen/info.c",
    "neatogen/hedges.c",
    "neatogen/heap.c",
    "neatogen/site.c",
    "neatogen/memory.c",
    "neatogen/legal.c",
    "neatogen/stuff.c",
    "neatogen/solve.c",
    "neatogen/stress.c",
    "neatogen/matrix_ops.c",
    "neatogen/circuit.c",
    "neatogen/matinv.c",
    "neatogen/lu.c",
    "neatogen/dijkstra.c",
    "neatogen/bfs.c",
    "neatogen/kkutils.c",
    "neatogen/embed_graph.c",
    "neatogen/pca.c",
    "neatogen/closest.c",
    "neatogen/conjgrad.c",
    "neatogen/delaunay.c",
    "neatogen/sgd.c",
    "neatogen/randomkit.c",
    "neatogen/call_tri.c",
    "neatogen/overlap.c",

    "fdpgen/layout.c",
    "fdpgen/tlayout.c",
    "fdpgen/grid.c",
    "fdpgen/fdpinit.c",
    "fdpgen/clusteredges.c",
    "fdpgen/comp.c",
    "fdpgen/xlayout.c",

    "sfdpgen/sfdpinit.c",
    "sfdpgen/spring_electrical.c",
    "sfdpgen/Multilevel.c",
    "sfdpgen/sparse_solve.c",
    "sfdpgen/post_process.c",

    "sparse/SparseMatrix.c",
    "sparse/QuadTree.c",
    "sparse/general.c",

    "twopigen/twopiinit.c",
    "twopigen/circle.c",

    "patchwork/patchworkinit.c",
    "patchwork/patchwork.c",
    "patchwork/tree_map.c",

    "pack/ccomps.c",
    "pack/pack.c",

    "osage/osageinit.c",
};
