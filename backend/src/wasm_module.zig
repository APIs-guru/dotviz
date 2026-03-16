const std = @import("std");
const testing = std.testing;
const vizjs_types = @import("vizjs_types.zig");
const wasm_allocator = std.heap.wasm_allocator;

pub const graphviz = @cImport({
    @cInclude("geom.h");
    @cInclude("cgraph_wrapper.h");
    @cInclude("context_inline.h");
    @cInclude("layout_inline.h");
    @cInclude("gvusershape_size.h");
    @cInclude("inline_render_dot/render_inline_dot.h");
    @cInclude("inline_render_svg/render_svg.h");
});

extern var Y_invert: bool;
extern var Reduce: bool;

fn toCString(maybe_string: ?[:0]const u8) [*c]const u8 {
    if (maybe_string) |string| {
        return @ptrCast(string.ptr);
    }
    return null;
}

fn setDefaultAttributes(
    allocator: std.mem.Allocator,
    graph: ?*graphviz.Agraph_t,
    attributes: *vizjs_types.Attributes,
    kind: c_int,
) void {
    var iterator = attributes.map.iterator();
    while (iterator.next()) |attr| {
        const name = allocator.dupeZ(u8, attr.key_ptr.*) catch @panic(
            "cannot dupeZ in setDefaultAttributes",
        );

        if (graphviz.agattr_text(graph, kind, name, null) == null) {
            _ = graphviz.agattr_text(graph, kind, name, "");
        }

        switch (attr.value_ptr.*) {
            .text => |val| {
                _ = graphviz.agattr_text(graph, kind, name, @ptrCast(val.ptr));
            },
            .html => |val| {
                _ = graphviz.agattr_html(graph, kind, name, @ptrCast(val.ptr));
            },
        }
    }
}

fn setAttributes(
    allocator: std.mem.Allocator,
    object: ?*anyopaque,
    attributes: *vizjs_types.Attributes,
) void {
    var iterator = attributes.map.iterator();
    while (iterator.next()) |attr| {
        const name = allocator.dupeZ(u8, attr.key_ptr.*) catch @panic(
            "cannot dupeZ in setAttributes",
        );
        switch (attr.value_ptr.*) {
            .text => |val| {
                _ = graphviz.agsafeset_text(object, name, @ptrCast(val.ptr), "");
            },
            .html => |val| {
                _ = graphviz.agsafeset_html(object, name, @ptrCast(val.ptr), "");
            },
        }
    }
}

fn readGraph(allocator: std.mem.Allocator, graph: anytype, json: anytype) void {
    if (json.graphAttributes) |attributes| {
        setDefaultAttributes(allocator, graph, attributes, graphviz.AGRAPH);
    }
    if (json.nodeAttributes) |attributes| {
        setDefaultAttributes(allocator, graph, attributes, graphviz.AGNODE);
    }
    if (json.edgeAttributes) |attributes| {
        setDefaultAttributes(allocator, graph, attributes, graphviz.AGEDGE);
    }
    if (json.nodes) |nodes| {
        for (nodes) |node| {
            const node_ptr = graphviz.agnode(graph, node.name, graphviz.true);
            if (node.attributes) |attributes| {
                setAttributes(
                    allocator,
                    node_ptr,
                    attributes,
                );
            }
        }
    }

    if (json.edges) |edges| {
        for (edges) |edge| {
            const tail_ptr = graphviz.agnode(graph, @ptrCast(edge.tail.ptr), graphviz.true);
            const head_ptr = graphviz.agnode(graph, @ptrCast(edge.head.ptr), graphviz.true);
            const edge_ptr = graphviz.agedge(graph, tail_ptr, head_ptr, null, graphviz.true);
            if (edge.attributes) |attributes| {
                setAttributes(
                    allocator,
                    edge_ptr,
                    attributes,
                );
            }
        }
    }

    if (json.subgraphs) |subgraphs| {
        for (subgraphs) |subgraph| {
            const subgraph_ptr = graphviz.agsubg(graph, toCString(subgraph.name), graphviz.true);
            readGraph(allocator, subgraph_ptr, subgraph);
        }
    }
}

pub export fn wasm_alloc(len: usize) ?[*]u8 {
    const mem = wasm_allocator.alloc(u8, len) catch return null;
    return mem.ptr;
}

pub export fn wasm_free(ptr: [*]u8, len: usize) void {
    const slice = ptr[0..len];
    wasm_allocator.free(slice);
}

var errors_strings: struct {
    allocator: std.mem.Allocator,
    array_list: std.ArrayList([]const u8),
} = undefined;

fn viz_errorf(text: [*c]u8) callconv(.c) c_int {
    var allocator = errors_strings.allocator;
    var array_list = &errors_strings.array_list;
    const copy = allocator.dupe(
        u8,
        std.mem.span(text),
    ) catch @panic("cannot allocate");
    array_list.append(allocator, copy) catch @panic("cannot allocate");
    return 0;
}

fn parseAgerrMessages(
    allocator: std.mem.Allocator,
) std.ArrayList(vizjs_types.RenderError) {
    const array_list = &errors_strings.array_list;
    if (array_list.items.len == 0) {
        return .empty;
    }
    const eql = std.mem.eql;
    const items = array_list.items;
    var result = std.ArrayList(vizjs_types.RenderError).initCapacity(
        allocator,
        0,
    ) catch @panic("cannot alloc");
    var level: ?vizjs_types.RenderErrorLevel = null;
    var i: usize = 0;
    while (i < items.len) : (i += 1) {
        if (eql(u8, items[i], "Error") and eql(u8, items[i + 1], ": ")) {
            level = .@"error";
            i += 1;
        } else if (std.mem.eql(u8, items[i], "Warning") and eql(u8, items[i + 1], ": ")) {
            level = .warning;
            i += 1;
        } else {
            result.append(allocator, .{
                .level = level orelse @panic("error level should not be null"),
                .message = .{ .slice = std.mem.trimEnd(u8, items[i], " \n") },
            }) catch @panic("cannot allocate");
        }
    }
    return result;
}

const WasmString = packed struct(u64) {
    ptr: u32,
    len: u32,

    fn init(s: []const u8) @This() {
        return .{
            .ptr = @intFromPtr(s.ptr),
            .len = s.len,
        };
    }
};

fn stringifyResponseJSON(response: vizjs_types.RenderResponse) WasmString {
    // use global WASM allocator since response is need to be passed to JS
    var json_writer = std.io.Writer.Allocating.init(wasm_allocator);
    var formatter: std.json.Formatter(vizjs_types.RenderResponse) = .{
        .options = .{},
        .value = response,
    };
    formatter.format(&json_writer.writer) catch @panic("cannot format");
    const json_response = json_writer.toOwnedSlice() catch @panic(
        "cannot into owned slice",
    );

    return WasmString.init(json_response);
}

pub export fn render(json_bytes: [*]u8, size: usize) WasmString {
    var arena = std.heap.ArenaAllocator.init(wasm_allocator);
    defer arena.deinit();
    const arena_allocator = arena.allocator();

    errors_strings = .{
        .allocator = arena_allocator,
        .array_list = .empty,
    };

    // Reset errors
    _ = graphviz.agseterrf(viz_errorf);
    _ = graphviz.agseterr(graphviz.AGWARN);
    _ = graphviz.agreseterrors();

    const json_string = json_bytes[0..size];
    defer wasm_allocator.free(json_string);

    var scanner = std.json.Scanner.initCompleteInput(
        arena_allocator,
        json_string,
    );
    defer scanner.deinit();

    var diag = std.json.Diagnostics{};
    scanner.enableDiagnostics(&diag);
    const request = std.json.parseFromTokenSourceLeaky(
        vizjs_types.RenderRequest,
        arena_allocator,
        &scanner,
        .{},
    ) catch |err| {
        const json_error = vizjs_types.JSONParseError.init(diag, err, json_string);
        var errors = parseAgerrMessages(arena_allocator);
        errors.append(arena_allocator, .{
            .level = .@"error",
            .message = .{
                .err = json_error,
            },
        }) catch @panic("cannot append error");
        return stringifyResponseJSON(.{
            .status = .failure,
            .errors = errors.items,
            .output = null,
        });
    };

    g_image_map = request.images;

    const graph_json = request.graph;
    const graph = graphviz.wrapped_agopen(
        toCString(graph_json.name),
        graph_json.directed,
        graph_json.strict,
    );
    if (graph == null) {
        return stringifyResponseJSON(.{
            .status = .failure,
            .errors = parseAgerrMessages(arena_allocator).items,
            .output = null,
        });
    }
    defer _ = graphviz.agclose(graph);

    readGraph(arena_allocator, graph, graph_json);

    Y_invert = request.yInvert;
    Reduce = request.reduce;

    const gvc = graphviz.gw_create_context();
    defer {
        _ = graphviz.gvFreeContext(gvc);
    }

    const engine = std.meta.stringToEnum(vizjs_types.Engine, request.engine) orelse {
        const message = std.fmt.allocPrint(
            wasm_allocator,
            "Layout type: \"{s}\" not recognized. Use one of: dot circo neato fdp twopi patchwork osage sfdp",
            .{request.engine},
        ) catch @panic("cannot allocate error message");

        var errors = parseAgerrMessages(arena_allocator);
        errors.append(arena_allocator, .{
            .level = .@"error",
            .message = .{ .slice = message },
        }) catch @panic("cannot append error");

        return stringifyResponseJSON(.{
            .status = .failure,
            .errors = errors.items,
            .output = null,
        });
    };

    if (graphviz.agget(graph, "layout")) |layout_cstr| {
        const layout_str = std.mem.span(layout_cstr);
        const layout = std.meta.stringToEnum(vizjs_types.Engine, layout_str) orelse {
            const message = std.fmt.allocPrint(
                arena_allocator,
                "Layout type: \"{s}\" not recognized. Use one of: dot circo neato fdp twopi patchwork osage sfdp",
                .{layout_str},
            ) catch @panic("cannot allocate error message");

            var errors = parseAgerrMessages(arena_allocator);
            errors.append(arena_allocator, .{
                .level = .@"error",
                .message = .{ .slice = message },
            }) catch @panic("cannot append error");

            return stringifyResponseJSON(.{
                .status = .failure,
                .errors = errors.items,
                .output = null,
            });
        };

        if (layout != engine) {
            const message = std.fmt.allocPrint(
                arena_allocator,
                "Layouts should be the same. {} != {}",
                .{ layout, engine },
            ) catch @panic("cannot allocate error message");

            var errors = parseAgerrMessages(arena_allocator);
            errors.append(arena_allocator, .{
                .level = .@"error",
                .message = .{ .slice = message },
            }) catch @panic("cannot append error");

            return stringifyResponseJSON(.{
                .status = .failure,
                .errors = errors.items,
                .output = null,
            });
        }
    }

    layoutRender(engine, gvc, graph);
    defer layoutCleanup(engine, graph);

    var responseDot: ?[:0]const u8 = null;
    defer freeCString(responseDot);
    if (request.renderDot) {
        const output = graphviz.render_dot(graph);
        responseDot = @ptrCast(output.data[0..output.data_position]);
    }

    var responseSvg: ?[:0]const u8 = null;
    defer freeCString(responseSvg);
    if (request.renderSvg) {
        const output = graphviz.render_svg(graph);
        responseSvg = @ptrCast(output.data[0..output.data_position]);
    }

    const responseJSON = stringifyResponseJSON(.{
        .status = .success,
        .errors = parseAgerrMessages(arena_allocator).items,
        .output = .{
            .dot = responseDot,
            .svg = responseSvg,
        },
    });
    return responseJSON;
}

fn layoutRender(engine: vizjs_types.Engine, gvc: ?*graphviz.GVC_t, graph: ?*graphviz.Agraph_t) void {
    switch (engine) {
        .dot => {
            graphviz.my_graph_init(gvc, graph, true);
            graphviz.dot_layout(graph);
        },
        .circo => {
            graphviz.my_graph_init(gvc, graph, false);
            graphviz.circo_layout(graph);
        },
        .neato => {
            graphviz.my_graph_init(gvc, graph, false);
            graphviz.neato_layout(graph);
        },
        .fdp => {
            graphviz.my_graph_init(gvc, graph, false);
            graphviz.fdp_layout(graph);
        },
        .twopi => {
            graphviz.my_graph_init(gvc, graph, false);
            graphviz.twopi_layout(graph);
        },
        .patchwork => {
            graphviz.my_graph_init(gvc, graph, false);
            graphviz.patchwork_layout(graph);
        },
        .osage => {
            graphviz.my_graph_init(gvc, graph, false);
            graphviz.osage_layout(graph);
        },
        .sfdp => {
            graphviz.my_graph_init(gvc, graph, false);
            graphviz.sfdp_layout(graph);
        },
    }
    // FIXME: IMPORTANT: check that we don't use GVC after this line
    graphviz.set_gvc_to_null(graph);
}

fn layoutCleanup(engine: vizjs_types.Engine, graph: ?*graphviz.Agraph_t) void {
    switch (engine) {
        .dot => graphviz.dot_cleanup(graph),
        .circo => graphviz.circo_cleanup(graph),
        .neato => graphviz.neato_cleanup(graph),
        .fdp => graphviz.fdp_cleanup(graph),
        .twopi => graphviz.twopi_cleanup(graph),
        .patchwork => graphviz.patchwork_cleanup(graph),
        .osage => graphviz.osage_cleanup(graph),
        .sfdp => graphviz.sfdp_cleanup(graph),
    }
    graphviz.graph_cleanup(graph);
}

fn freeCString(string: ?[:0]const u8) void {
    if (string) |slice| {
        std.c.free(@ptrCast(@constCast(slice.ptr)));
    }
}

var g_image_map: vizjs_types.ImageDimensionsMap = undefined;
export fn gvusershape_size(graph: *graphviz.Agraph_t, name: [*c]u8) graphviz.point {
    const dimensions = g_image_map.map.get(std.mem.span(name)) orelse @panic("no image found");
    return graphviz.my_gvusershape_size(graph, dimensions.height, dimensions.width);
}

export fn get_dimensions_by_name(name: [*c]u8, dpi: graphviz.pointf) graphviz.point {
    const dimensions = g_image_map.map.get(std.mem.span(name)) orelse return .{
        .x = -1,
        .y = -1,
    };
    return graphviz.convert_image_dimensions(dpi, dimensions.height, dimensions.width);
}
