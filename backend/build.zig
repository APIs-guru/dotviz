const std = @import("std");

const flags = [_][]const u8{ "-Wall", "-Werror", "-Wextra" };

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{
        .default_target = .{
            .cpu_arch = .wasm32,
            .os_tag = .wasi,
        },
    });

    const optimize = b.standardOptimizeOption(.{});
    const graphviz_build_mode = optimize;

    const lib_mod = b.createModule(.{
        .root_source_file = b.path("src/wasm_module.zig"),
        .target = target,
        .optimize = optimize,
    });

    const exe_mod = b.createModule(.{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
    });
    exe_mod.addImport("dotviz_lib", lib_mod);

    var lib = b.addLibrary(.{
        .name = "dotviz",
        .root_module = lib_mod,
        .linkage = .static,
    });

    const graphviz_build = try buildGraphviz(
        b,
        target,
        graphviz_build_mode,
    );
    lib.root_module.linkLibrary(graphviz_build);

    lib.root_module.addIncludePath(b.path("src"));
    lib.root_module.addCSourceFiles(.{
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
        .flags = &flags,
    });

    lib.root_module.export_symbol_names = &.{
        "wasm_alloc",
        "wasm_free",
        "render",
    };
    applyWasiEmulation(lib);

    const exe = b.addExecutable(.{
        .name = "dotviz",
        .root_module = exe_mod,
    });
    exe.root_module.addIncludePath(b.path("src"));
    exe.lto = .full;
    applyWasiEmulation(exe);
    lib.stack_size = 16 * 1024 * 1024;
    exe.stack_size = 16 * 1024 * 1024;

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| run_cmd.addArgs(args);

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}

pub fn buildGraphviz(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) !*std.Build.Step.Compile {
    const graphviz_dep = b.dependency("graphviz", .{
        .target = target,
        .optimize = optimize,
    });

    var lib_mod = b.createModule(.{
        .root_source_file = null,
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });

    const lib = b.addLibrary(.{
        .name = "graphvizstatic",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_mod.addCSourceFile(.{ .file = b.path(
        "src/graphviz_build/src/dummy.c",
    ) });
    lib_mod.addCSourceFile(.{ .file = b.path(
        "src/graphviz_build/src/drand48.c",
    ) });
    lib_mod.addCSourceFile(.{ .file = b.path(
        "src/graphviz_build/src/qsort.c",
    ) });
    lib_mod.addIncludePath(graphviz_dep.path("lib/cdt"));
    lib_mod.addIncludePath(graphviz_dep.path("lib/cgraph"));
    const expat_dep = b.dependency("libexpat", .{
        .target = target,
        .optimize = optimize,
    });
    lib_mod.linkLibrary(expat_dep.artifact("expat"));

    const config_h = b.addConfigHeader(.{
        .style = .blank,
        .include_path = "config.h",
    }, .{
        .HAVE_TCL = 0,
        .DEFAULT_DPI = 96,
        .HAVE_EXPAT = 1,
        .HAVE_SYS_MMAN_H = 1,
        .HAVE_DRAND48 = 1,
        .HAVE_SRAND48 = 1,
    });
    lib.installConfigHeader(config_h);
    const builddate_h = b.addConfigHeader(.{
        .style = .blank,
        .include_path = "builddate.h",
    }, .{
        .PACKAGE_VERSION = "a",
        .BUILDDATE = "a",
    });
    lib.installConfigHeader(builddate_h);

    const lib_cdt = b.addLibrary(.{
        .name = "cdt",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_cdt.root_module.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/cdt"),
        .files = &src_cdt,
    });
    lib_cdt.root_module.addIncludePath(graphviz_dep.path("lib"));
    lib_cdt.root_module.addIncludePath(graphviz_dep.path("lib/cdt"));

    const lib_cgraph = b.addLibrary(.{
        .name = "cgraph",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_cgraph.root_module.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/cgraph"),
        .files = &src_cgraph,
    });
    lib_cgraph.root_module.addIncludePath(b.path("inc/cgraph"));
    lib.root_module.addConfigHeader(config_h);
    lib.root_module.addIncludePath(graphviz_dep.path("lib"));
    lib.root_module.addIncludePath(b.path("src/graphviz_build/inc/cgraph/"));
    lib_cgraph.root_module.addConfigHeader(config_h);
    lib_cgraph.root_module.addIncludePath(b.path("src/graphviz_build/inc/cgraph/"));
    lib_cgraph.root_module.addIncludePath(graphviz_dep.path("lib"));
    lib_cgraph.root_module.addIncludePath(graphviz_dep.path("lib/cdt"));
    lib_cgraph.root_module.addIncludePath(graphviz_dep.path("lib/cgraph"));

    const lib_common = b.addLibrary(.{
        .name = "common",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_common.root_module.addIncludePath(b.path("src/graphviz_build/inc/"));
    lib_common.root_module.addIncludePath(b.path("src/graphviz_build/inc/common/"));
    lib_common.root_module.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/common"),
        .files = &src_common,
    });
    lib.root_module.addCSourceFiles(.{
        .root = b.path("src/graphviz_build/src/common/"),
        .files = &.{"htmlparse.c"},
    });
    addInclude(lib, graphviz_dep);
    lib.root_module.addIncludePath(b.path("src/graphviz_build/inc/common/"));
    lib_common.root_module.addIncludePath(graphviz_dep.path("lib"));
    addInclude(lib_common, graphviz_dep);
    lib_common.root_module.linkLibrary(expat_dep.artifact("expat"));
    lib_common.root_module.addIncludePath(.{
        .dependency = .{
            .dependency = expat_dep,
            .sub_path = "lib",
        },
    });
    lib_common.root_module.addConfigHeader(config_h);
    lib_common.root_module.addConfigHeader(builddate_h);

    const lib_util = b.addLibrary(.{
        .name = "util",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_util.root_module.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/util"),
        .files = &src_util,
    });
    lib_util.root_module.addIncludePath(graphviz_dep.path("lib"));
    lib_util.root_module.addIncludePath(graphviz_dep.path("lib/util"));

    const lib_gvc = b.addLibrary(.{
        .name = "gvc",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_gvc.root_module.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/gvc"),
        .files = &src_gvc,
    });
    lib_gvc.root_module.addIncludePath(graphviz_dep.path("lib"));
    addInclude(lib_gvc, graphviz_dep);
    lib_gvc.root_module.addConfigHeader(config_h);
    lib_gvc.root_module.addConfigHeader(builddate_h);

    const lib_xdot = b.addLibrary(.{
        .name = "xdot",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_xdot.root_module.addCSourceFile(.{ .file = .{
        .dependency = .{
            .dependency = graphviz_dep,
            .sub_path = "lib/xdot/xdot.c",
        },
    } });
    lib_xdot.root_module.addIncludePath(graphviz_dep.path("lib"));

    const lib_pathplan = b.addLibrary(.{
        .name = "pathplan",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_pathplan.root_module.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/pathplan"),
        .files = &src_pathplan,
    });
    lib_pathplan.root_module.addIncludePath(graphviz_dep.path("lib"));
    lib_pathplan.root_module.addIncludePath(graphviz_dep.path("lib/pathplan"));

    const lib_dotgen = b.addLibrary(.{
        .name = "dotgen",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_dotgen.root_module.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/dotgen"),
        .files = &src_dotgen,
    });
    addInclude(lib_dotgen, graphviz_dep);
    lib_dotgen.root_module.addConfigHeader(config_h);

    const lib_circogen = b.addLibrary(.{
        .name = "circogen",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_circogen.root_module.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/circogen"),
        .files = &src_circogen,
    });
    addInclude(lib_circogen, graphviz_dep);
    lib_circogen.root_module.addConfigHeader(config_h);

    const lib_neatogen = b.addLibrary(.{
        .name = "neatogen",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_neatogen.root_module.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/neatogen"),
        .files = &src_neatogen,
    });
    addInclude(lib_neatogen, graphviz_dep);
    lib_neatogen.root_module.addConfigHeader(config_h);

    const lib_fdpgen = b.addLibrary(.{
        .name = "fdpgen",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_fdpgen.root_module.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/fdpgen"),
        .files = &src_fdpgen,
    });
    addInclude(lib_fdpgen, graphviz_dep);
    lib_fdpgen.root_module.addConfigHeader(config_h);

    const lib_sfdpgen = b.addLibrary(.{
        .name = "sfdpgen",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_sfdpgen.root_module.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/sfdpgen"),
        .files = &src_sfdpgen,
    });
    addInclude(lib_sfdpgen, graphviz_dep);
    lib_sfdpgen.root_module.addConfigHeader(config_h);

    const lib_sparse = b.addLibrary(.{
        .name = "sparse",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_sparse.root_module.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/sparse"),
        .files = &src_sparse,
    });
    addInclude(lib_sparse, graphviz_dep);
    lib_sparse.root_module.addConfigHeader(config_h);

    const lib_twopigen = b.addLibrary(.{
        .name = "twopigen",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_twopigen.root_module.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/twopigen"),
        .files = &src_twopigen,
    });
    addInclude(lib_twopigen, graphviz_dep);
    lib_twopigen.root_module.addConfigHeader(config_h);

    const lib_patchwork = b.addLibrary(.{
        .name = "patchwork",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_patchwork.root_module.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/patchwork"),
        .files = &src_patchwork,
    });
    addInclude(lib_patchwork, graphviz_dep);
    lib_patchwork.root_module.addConfigHeader(config_h);

    const lib_osage = b.addLibrary(.{
        .name = "osage",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_osage.root_module.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/osage"),
        .files = &src_osage,
    });
    addInclude(lib_osage, graphviz_dep);
    lib_osage.root_module.addConfigHeader(config_h);

    const lib_plugin_dot_layout = b.addLibrary(.{
        .name = "dot_layout",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_plugin_dot_layout.root_module.addCSourceFile(.{ .file = .{
        .dependency = .{
            .dependency = graphviz_dep,
            .sub_path = "plugin/dot_layout/gvlayout_dot_layout.c",
        },
    } });
    lib_plugin_dot_layout.root_module.addCSourceFile(.{ .file = .{
        .dependency = .{
            .dependency = graphviz_dep,
            .sub_path = "plugin/dot_layout/gvplugin_dot_layout.c",
        },
    } });
    addInclude(lib_plugin_dot_layout, graphviz_dep);
    lib_plugin_dot_layout.root_module.addConfigHeader(config_h);

    const lib_pack = b.addLibrary(.{
        .name = "pack",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_pack.root_module.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/pack"),
        .files = &.{
            "ccomps.c",
            "pack.c",
        },
    });
    addInclude(lib_pack, graphviz_dep);
    lib_pack.root_module.addConfigHeader(config_h);

    const lib_label = b.addLibrary(.{
        .name = "label",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_label.root_module.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/label"),
        .files = &src_label,
    });
    addInclude(lib_label, graphviz_dep);
    lib_label.root_module.addConfigHeader(config_h);

    inline for (&.{
        lib,                   lib_cdt,       lib_cgraph,   lib_common,
        lib_dotgen,            lib_circogen,  lib_neatogen, lib_fdpgen,
        lib_twopigen,          lib_patchwork, lib_osage,    lib_sfdpgen,
        lib_gvc,               lib_label,     lib_pack,     lib_pathplan,
        lib_plugin_dot_layout, lib_util,      lib_xdot,
    }) |library| {
        applyWasiEmulation(library);
    }

    const h = std.Build.Step.Compile.HeaderInstallation.Directory.Options{ .include_extensions = &.{".h"} };
    lib.installHeadersDirectory(graphviz_dep.path("lib"), "lib", h);
    lib.installHeadersDirectory(graphviz_dep.path("lib/common"), "", h);
    lib.installHeadersDirectory(graphviz_dep.path("lib/pathplan"), "", h);
    lib.installHeadersDirectory(graphviz_dep.path("lib/gvc"), "", h);
    lib.installHeadersDirectory(graphviz_dep.path("lib/cdt"), "", h);
    lib.installHeadersDirectory(graphviz_dep.path("lib/cgraph"), "", h);
    lib.installHeadersDirectory(graphviz_dep.path("lib/util"), "", h);
    lib.installHeadersDirectory(graphviz_dep.path("lib/xdot"), "", h);
    lib.installHeadersDirectory(graphviz_dep.path("lib/util"), "util/", h);
    lib.installHeader(graphviz_dep.path("lib/gvc/gvc.h"), "gvc.h");
    lib.installHeadersDirectory(b.path("src/graphviz_build/inc/common"), "", .{});

    b.installArtifact(lib);
    return lib;
}

fn addInclude(step: *std.Build.Step.Compile, graphviz_dep: *std.Build.Dependency) void {
    step.root_module.addIncludePath(graphviz_dep.path("lib"));
    step.root_module.addIncludePath(graphviz_dep.path("lib/common"));
    step.root_module.addIncludePath(graphviz_dep.path("lib/pathplan"));
    step.root_module.addIncludePath(graphviz_dep.path("lib/gvc"));
    step.root_module.addIncludePath(graphviz_dep.path("lib/cgraph"));
    step.root_module.addIncludePath(graphviz_dep.path("lib/cdt"));
}

fn applyWasiEmulation(step: *std.Build.Step.Compile) void {
    if (step.root_module.resolved_target.?.result.os.tag == .wasi) {
        step.root_module.addCMacro("_WASI_EMULATED_SIGNAL", "");
        step.root_module.linkSystemLibrary("wasi-emulated-signal", .{});
        step.root_module.addCMacro("_WASI_EMULATED_PROCESS_CLOCKS", "");
        step.root_module.linkSystemLibrary("wasi-emulated-process-clocks", .{});
        step.root_module.addCMacro("_WASI_EMULATED_MMAN", "");
        step.root_module.linkSystemLibrary("wasi-emulated-mman", .{});
        step.root_module.addCMacro("_WASI_EMULATED_GETPID", "");
        step.root_module.linkSystemLibrary("wasi-emulated-getpid", .{});
    }
}

const src_cdt = [_][]const u8{
    "dtview.c",  "dtwalk.c",    "dtrestore.c", "dttree.c",    "dtclose.c",
    "dtrenew.c", "dtstrhash.c", "dtflatten.c", "dtextract.c", "dtsize.c",
    "dthash.c",  "dtopen.c",    "dtmethod.c",  "dtstat.c",    "dtdisc.c",
};

const src_cgraph = [_][]const u8{
    "imap.c",    "rec.c",    "subg.c", "ingraphs.c", "apply.c",       "agerror.c",
    "graph.c",   "id.c",     "edge.c", "utils.c",    "obj.c",         "unflatten.c",
    "acyclic.c", "refstr.c", "tred.c", "node.c",     "node_induce.c", "attr.c",
    // "write.c",
    "io.c",
};

const src_gvc = [_][]const u8{
    "gvtextlayout.c", "gvjobs.c", "gvlayout.c", "gvplugin.c",
    // "gvrender.c",
    // "gvusershape.c",
    "gvc.c", // "gvdevice.c",
    "gvconfig.c",    "gvcontext.c", // "gvevent.c",
    "gvtool_tred.c", "gvloadimage.c",
};

const src_common = [_][]const u8{
    "splines.c", "htmllex.c", "colxlate.c", "textspan_lut.c", "postproc.c",
    "taper.c",    "globals.c", "timing.c", "psusershape.c", // "emit.c",
    "textspan.c", "utils.c",   "args.c",   "routespl.c",
    // "shapes.c",
    "pointset.c", "ns.c", "ellipse.c", //"arrows.c",
    "geom.c",
    "input.c", //"output.c",
    // "labels.c",
    // "htmltable.c",
};

const src_util = [_][]const u8{
    //"xml.c",
    "gv_fopen.c", "gv_find_me.c", "random.c", "base64.c",
};

const src_pathplan = [_][]const u8{
    "triang.c", "util.c",  "inpoly.c",  "visibility.c",  "shortest.c",
    "cvt.c",    "route.c", "solvers.c", "shortestpth.c",
};

const src_dotgen = [_][]const u8{
    "dotinit.c",  "class1.c",  "fastgr.c", "cluster.c",    "aspect.c",
    "mincross.c", "acyclic.c", "decomp.c", "dotsplines.c", "compound.c",
    "rank.c",     "class2.c",  "flat.c",   "sameport.c",   "conc.c",
    "position.c",
};

const src_circogen = [_][]const u8{
    "block.c",    "blockpath.c",    "blocktree.c", "circpos.c",
    "circular.c", "circularinit.c", "edgelist.c",  "nodelist.c",
};

const src_neatogen = [_][]const u8{
    "neatoinit.c",   "adjust.c",     "neatosplines.c", "constraint.c",
    "geometry.c",    "poly.c",       "voronoi.c",      "edges.c",
    "info.c",        "hedges.c",     "heap.c",         "site.c",
    "memory.c",      "legal.c",      "stuff.c",        "solve.c",
    "stress.c",      "matrix_ops.c", "circuit.c",      "matinv.c",
    "lu.c",          "dijkstra.c",   "bfs.c",          "kkutils.c",
    "embed_graph.c", "pca.c",        "closest.c",      "conjgrad.c",
    "delaunay.c",    "sgd.c",        "randomkit.c",
    // "adjust.c", "bfs.c", "call_tri.c", "circuit.c", "closest.c", "compute_hierarchy.c", "conjgrad.c", "constrained_majorization.c", "constrained_majorization_ipsep.c", "constraint.c", "delaunay.c", "dijkstra.c", "edges.c", "embed_graph.c", "geometry.c", "heap.c", "hedges.c", "info.c", "kkutils.c", "legal.c", "lu.c", "matinv.c", "matrix_ops.c", "memory.c", "multispline.c", "neatoinit.c", "neatosplines.c", "opt_arrangement.c", "overlap.c", "pca.c", "poly.c", "quad_prog_solve.c", "quad_prog_vpsc.c", "randomkit.c", "sgd.c", "site.c", "smart_ini_x.c", "solve.c", "stress.c", "stuff.c", "voronoi.c"
       "call_tri.c",
    "overlap.c",
};

const src_sfdpgen = [_][]const u8{
    "sfdpinit.c", "spring_electrical.c", "Multilevel.c", "sparse_solve.c", "post_process.c",
};

const src_sparse = [_][]const u8{
    "SparseMatrix.c", "QuadTree.c", "general.c",
};

const src_fdpgen = [_][]const u8{
    "layout.c",  "tlayout.c", "grid.c", "fdpinit.c", "clusteredges.c", "comp.c",
    "xlayout.c",
};

const src_twopigen = [_][]const u8{
    "twopiinit.c", "circle.c",
};

const src_patchwork = [_][]const u8{
    "patchworkinit.c", "patchwork.c", "tree_map.c",
};

const src_osage = [_][]const u8{"osageinit.c"};

const src_label = [_][]const u8{
    "index.c", "split.q.c", "xlabels.c", "rectangle.c", "node.c",
};
