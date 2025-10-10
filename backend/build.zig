const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{
        .default_target = .{
            .cpu_arch = .wasm32,
            .os_tag = .wasi,
        },
    });

    const optimize = b.standardOptimizeOption(.{
        .preferred_optimize_mode = .ReleaseSmall,
    });
    const graphviz_build_mode = std.builtin.OptimizeMode.ReleaseSmall;

    const lib_mod = b.createModule(.{
        .root_source_file = b.path("src/wasm_module.zig"),
        .target = target,
        .optimize = optimize,
    });

    const exe_mod = b.createModule(.{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
        .sanitize_c = .off,
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
    lib.linkLibrary(graphviz_build);

    lib.addIncludePath(b.path("src"));
    lib.addCSourceFile(.{ .file = b.path("src/agrw.c") });
    lib.addCSourceFile(.{ .file = b.path("src/layout_inline.c") });
    lib.addCSourceFile(.{ .file = b.path("src/render_inline.c") });
    lib.addCSourceFile(.{ .file = b.path("src/context_inline.c") });
    lib.addCSourceFile(.{ .file = b.path("src/render_inline_dot.c") });
    lib.addCSourceFile(.{ .file = b.path("src/render_inline_svg.c") });
    lib.addCSourceFile(.{ .file = b.path("src/output_dot.c") });
    lib.addCSourceFile(.{ .file = b.path("src/write_c_inline.c") });
    lib.root_module.export_symbol_names = &.{
        "viz_dot_to_graph",
        "viz_json_to_graph",
        "viz_free_graph",
        "viz_layout_graph",
        "viz_layout",
        "viz_render",
        "viz_free_layout",
        "viz_free_context",
        "viz_layout_done",
        "viz_graph_to_svg",
        "viz_free_svg",
        "viz_create_context",
        "viz_alloc",
        "viz_free",
        "viz_set_y_invert",
        "viz_set_reduce",
        "viz_reset_errors",
        "wasm_alloc",
        "wasm_free",
        "viz_read_one_graph_from_dot",
        "viz_set_default_graph_attribute",
        "viz_set_default_node_attribute",
        "viz_set_default_edge_attribute",
    };
    lib.root_module.strip = true;
    lib.export_table = true;
    lib.bundle_ubsan_rt = false;
    applyWasiEmulation(lib);

    const exe = b.addExecutable(.{
        .name = "dotviz",
        .root_module = exe_mod,
    });
    exe.addIncludePath(b.path("src"));
    exe.want_lto = true;
    exe.import_symbols = true;
    exe.export_table = true;
    exe.bundle_ubsan_rt = false;
    exe.root_module.strip = true;
    exe.bundle_ubsan_rt = false;
    applyWasiEmulation(exe);

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

    const lib_mod = b.createModule(.{
        .root_source_file = null,
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .sanitize_c = .off,
    });

    const lib = b.addLibrary(.{
        .name = "graphvizstatic",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib.addCSourceFile(.{ .file = b.path(
        "src/graphviz_build/src/dummy.c",
    ) });
    lib.addIncludePath(graphviz_dep.path("lib/cdt"));
    lib.addIncludePath(graphviz_dep.path("lib/cgraph"));
    const expat_dep = b.dependency("libexpat", .{
        .target = target,
        .optimize = optimize,
    });
    lib.linkLibrary(expat_dep.artifact("expat"));

    const config_h = b.addConfigHeader(.{
        .style = .blank,
        .include_path = "config.h",
    }, .{
        .HAVE_TCL = 0,
        .DEFAULT_DPI = 96,
        .HAVE_EXPAT = 1,
        .HAVE_SYS_MMAN_H = 1,
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
    lib_cdt.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/cdt"),
        .files = &src_cdt,
    });
    lib_cdt.addIncludePath(graphviz_dep.path("lib"));
    lib_cdt.addIncludePath(graphviz_dep.path("lib/cdt"));

    const lib_cgraph = b.addLibrary(.{
        .name = "cgraph",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_cgraph.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/cgraph"),
        .files = &src_cgraph,
    });
    lib_cgraph.addIncludePath(b.path("inc/cgraph"));
    lib.addCSourceFiles(.{
        .root = b.path("src/graphviz_build/src/cgraph/"),
        .files = &.{
            "grammar.c",
            "scan.c",
        },
    });
    lib.addConfigHeader(config_h);
    lib.addIncludePath(graphviz_dep.path("lib"));
    lib.addIncludePath(b.path("src/graphviz_build/inc/cgraph/"));
    lib_cgraph.addConfigHeader(config_h);
    lib_cgraph.addIncludePath(b.path("src/graphviz_build/inc/cgraph/"));
    lib_cgraph.addIncludePath(graphviz_dep.path("lib"));
    lib_cgraph.addIncludePath(graphviz_dep.path("lib/cdt"));
    lib_cgraph.addIncludePath(graphviz_dep.path("lib/cgraph"));

    const lib_common = b.addLibrary(.{
        .name = "gvc",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_common.addIncludePath(b.path("src/graphviz_build/inc/"));
    lib_common.addIncludePath(b.path("src/graphviz_build/inc/common/"));
    lib_common.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/common"),
        .files = &src_common,
    });
    lib.addCSourceFiles(.{
        .root = b.path("src/graphviz_build/src/common/"),
        .files = &.{"htmlparse.c"},
    });
    addInclude(lib, graphviz_dep);
    lib.addIncludePath(b.path("src/graphviz_build/inc/common/"));
    lib_common.addIncludePath(graphviz_dep.path("lib"));
    addInclude(lib_common, graphviz_dep);
    lib_common.linkLibrary(expat_dep.artifact("expat"));
    lib_common.addIncludePath(.{
        .dependency = .{
            .dependency = expat_dep,
            .sub_path = "lib",
        },
    });
    lib_common.addConfigHeader(config_h);
    lib_common.addConfigHeader(builddate_h);

    const lib_util = b.addLibrary(.{
        .name = "util",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_util.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/util"),
        .files = &src_util,
    });
    lib_util.addIncludePath(graphviz_dep.path("lib"));
    lib_util.addIncludePath(graphviz_dep.path("lib/util"));

    const lib_gvc = b.addLibrary(.{
        .name = "gvc",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_gvc.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/gvc"),
        .files = &src_gvc,
    });
    lib_gvc.addIncludePath(graphviz_dep.path("lib"));
    addInclude(lib_gvc, graphviz_dep);
    lib_gvc.addConfigHeader(config_h);
    lib_gvc.addConfigHeader(builddate_h);

    const lib_xdot = b.addLibrary(.{
        .name = "xdot",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_xdot.addCSourceFile(.{ .file = .{
        .dependency = .{
            .dependency = graphviz_dep,
            .sub_path = "lib/xdot/xdot.c",
        },
    } });
    lib_xdot.addIncludePath(graphviz_dep.path("lib"));

    const lib_pathplan = b.addLibrary(.{
        .name = "pathplan",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_pathplan.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/pathplan"),
        .files = &src_pathplan,
    });
    lib_pathplan.addIncludePath(graphviz_dep.path("lib"));
    lib_pathplan.addIncludePath(graphviz_dep.path("lib/pathplan"));

    const lib_dotgen = b.addLibrary(.{
        .name = "dotgen",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_dotgen.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/dotgen"),
        .files = &src_dotgen,
    });
    addInclude(lib_dotgen, graphviz_dep);
    lib_dotgen.addConfigHeader(config_h);

    const lib_plugin_dot_layout = b.addLibrary(.{
        .name = "dot_layout",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_plugin_dot_layout.addCSourceFile(.{ .file = .{
        .dependency = .{
            .dependency = graphviz_dep,
            .sub_path = "plugin/dot_layout/gvlayout_dot_layout.c",
        },
    } });
    lib_plugin_dot_layout.addCSourceFile(.{ .file = .{
        .dependency = .{
            .dependency = graphviz_dep,
            .sub_path = "plugin/dot_layout/gvplugin_dot_layout.c",
        },
    } });
    addInclude(lib_plugin_dot_layout, graphviz_dep);
    lib_plugin_dot_layout.addConfigHeader(config_h);

    const lib_pack = b.addLibrary(.{
        .name = "pack",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_pack.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/pack"),
        .files = &.{
            "ccomps.c",
            "pack.c",
        },
    });
    addInclude(lib_pack, graphviz_dep);
    lib_pack.addConfigHeader(config_h);

    const lib_label = b.addLibrary(.{
        .name = "label",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_label.addCSourceFiles(.{
        .root = graphviz_dep.path("lib/label"),
        .files = &src_label,
    });
    addInclude(lib_label, graphviz_dep);
    lib_label.addConfigHeader(config_h);

    const lib_plugin_core = b.addLibrary(.{
        .name = "plugin_core",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib_plugin_core.addCSourceFiles(.{
        .root = graphviz_dep.path("plugin/core"),
        .files = &src_plugin_core,
    });
    addInclude(lib_plugin_core, graphviz_dep);
    lib_plugin_core.addConfigHeader(config_h);

    inline for (&.{
        lib,          lib_cdt,         lib_cgraph,            lib_common,
        lib_dotgen,   lib_gvc,         lib_label,             lib_pack,
        lib_pathplan, lib_plugin_core, lib_plugin_dot_layout, lib_util,
        lib_xdot,
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
    lib.installHeadersDirectory(graphviz_dep.path("lib/util"), "util/", h);
    lib.installHeader(graphviz_dep.path("lib/gvc/gvc.h"), "gvc.h");
    lib.installHeadersDirectory(b.path("src/graphviz_build/inc/common"), "", .{});

    b.installArtifact(lib);
    return lib;
}

fn addInclude(step: *std.Build.Step.Compile, graphviz_dep: *std.Build.Dependency) void {
    step.addIncludePath(graphviz_dep.path("lib"));
    step.addIncludePath(graphviz_dep.path("lib/common"));
    step.addIncludePath(graphviz_dep.path("lib/pathplan"));
    step.addIncludePath(graphviz_dep.path("lib/gvc"));
    step.addIncludePath(graphviz_dep.path("lib/cgraph"));
    step.addIncludePath(graphviz_dep.path("lib/cdt"));
}

fn applyWasiEmulation(step: *std.Build.Step.Compile) void {
    if (step.root_module.resolved_target.?.result.os.tag == .wasi) {
        step.root_module.addCMacro("_WASI_EMULATED_SIGNAL", "");
        step.linkSystemLibrary("wasi-emulated-signal");
        step.root_module.addCMacro("_WASI_EMULATED_PROCESS_CLOCKS", "");
        step.linkSystemLibrary("wasi-emulated-process-clocks");
        step.root_module.addCMacro("_WASI_EMULATED_MMAN", "");
        step.linkSystemLibrary("wasi-emulated-mman");
        step.root_module.addCMacro("_WASI_EMULATED_GETPID", "");
        step.linkSystemLibrary("wasi-emulated-getpid");
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
    "gvtextlayout.c", "gvjobs.c",      "gvlayout.c", "gvplugin.c",
    "gvrender.c",     "gvusershape.c", "gvc.c",      "gvdevice.c",
    "gvconfig.c",     "gvcontext.c",   "gvevent.c",  "gvtool_tred.c",
    "gvloadimage.c",
};

const src_common = [_][]const u8{
    "splines.c",  "htmllex.c", "colxlate.c", "textspan_lut.c", "postproc.c",
    "taper.c",    "globals.c", "timing.c",   "psusershape.c",  "emit.c",
    "textspan.c", "utils.c",   "args.c",     "routespl.c",     "shapes.c",
    "pointset.c", "ns.c",      "ellipse.c",  "arrows.c",       "geom.c",
    "input.c", //"output.c",
    "labels.c",
    "htmltable.c",
};

const src_util = [_][]const u8{
    "xml.c", "gv_fopen.c", "gv_find_me.c", "random.c", "base64.c",
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

const src_label = [_][]const u8{
    "index.c", "split.q.c", "xlabels.c", "rectangle.c", "node.c",
};

const src_plugin_core = [_][]const u8{
    "gvrender_core_dot.c",
    "gvrender_core_svg.c",
};
