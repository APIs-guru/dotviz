const std = @import("std");
const testing = std.testing;
const wasm_allocator = std.heap.wasm_allocator;

const vizjs_types = @import("vizjs_types.zig");

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
    attributes: vizjs_types.Attributes,
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

        const sym = brk: switch (attr.value_ptr.*) {
            .text => |val| {
                break :brk graphviz.agattr_text(graph, kind, name, @ptrCast(val.ptr));
            },
            .html => |val| {
                break :brk graphviz.agattr_html(graph, kind, name, @ptrCast(val.ptr));
            },
        };
        if (graphviz.agroot(graph) == graph) {
            graphviz.wrapped_sym_set_print(sym);
        }
    }
}

fn setAttributes(
    allocator: std.mem.Allocator,
    object: ?*anyopaque,
    attributes: vizjs_types.Attributes,
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

fn edgePortToString(
    allocator: std.mem.Allocator,
    endpoint_json: vizjs_types.EdgeEndpoint,
) ?[:0]const u8 {
    const maybePort = endpoint_json.port.name;
    const maybeCompass = endpoint_json.compass;
    if (maybePort) |port| {
        if (maybeCompass) |compass| {
            return std.fmt.allocPrintSentinel(allocator, "{s}:{s}", .{ port, compass }, 0) catch @panic(
                "cannot allocPrintSentinel in edgePortToString",
            );
        } else {
            return port;
        }
    } else {
        return maybeCompass orelse null;
    }
}

fn readGraphJSON(allocator: std.mem.Allocator, graph: ?*graphviz.Agraph_t, graph_json: vizjs_types.Graph) void {
    setDefaultAttributes(allocator, graph, graph_json.graphAttributes, graphviz.AGRAPH);
    setDefaultAttributes(allocator, graph, graph_json.nodeAttributes, graphviz.AGNODE);
    setDefaultAttributes(allocator, graph, graph_json.edgeAttributes, graphviz.AGEDGE);

    const allNodes = allocator.alloc(?*graphviz.Agnode_t, graph_json.allNodes.len) catch @panic(
        "cannot alloc for allNodes",
    );
    for (graph_json.allNodes, 0..) |node_json, i| {
        const node_ptr = graphviz.agnode(graph, node_json.name, graphviz.true);
        setAttributes(allocator, node_ptr, node_json.attributes);
        allNodes[i] = node_ptr;
    }

    const allEdges = allocator.alloc(?*graphviz.Agedge_t, graph_json.allEdges.len) catch @panic(
        "cannot alloc for allEdges",
    );
    for (graph_json.allEdges, 0..) |edge_json, i| {
        const tail = edge_json.tail;
        const head = edge_json.head;

        const tail_node = allNodes[tail.port.node];
        const head_node = allNodes[head.port.node];
        const key: [*c]const u8 = edge_json.key orelse null;
        const edge = graphviz.agedge(graph, tail_node, head_node, key, graphviz.true);
        if (edgePortToString(allocator, tail)) |tailport| {
            _ = graphviz.agsafeset_text(edge, "tailport", tailport, "");
        }
        if (edgePortToString(allocator, head)) |headport| {
            _ = graphviz.agsafeset_text(edge, "headport", headport, "");
        }
        setAttributes(allocator, edge, edge_json.attributes);
        allEdges[i] = edge;
    }

    for (graph_json.subgraphs) |subgraph_json| {
        const subgraph = graphviz.agsubg(graph, toCString(subgraph_json.name), graphviz.true);
        readSubgraphJSON(allocator, subgraph, subgraph_json, allNodes, allEdges);
    }
}

fn readSubgraphJSON(
    allocator: std.mem.Allocator,
    subgraph: anytype,
    subgraph_json: vizjs_types.Subgraph,
    allNodes: []?*graphviz.Agnode_t,
    allEdges: []?*graphviz.Agedge_t,
) void {
    setDefaultAttributes(allocator, subgraph, subgraph_json.graphAttributes, graphviz.AGRAPH);
    setDefaultAttributes(allocator, subgraph, subgraph_json.nodeAttributes, graphviz.AGNODE);
    setDefaultAttributes(allocator, subgraph, subgraph_json.edgeAttributes, graphviz.AGEDGE);

    for (subgraph_json.memberNodes) |node| {
        _ = graphviz.agsubnode(subgraph, allNodes[node], graphviz.true);
    }

    for (subgraph_json.memberEdges) |edge| {
        _ = graphviz.agsubedge(subgraph, allEdges[edge], graphviz.true);
    }

    for (subgraph_json.subgraphs) |child_subgraph_json| {
        const child_subgraph = graphviz.agsubg(subgraph, toCString(child_subgraph_json.name), graphviz.true);
        readSubgraphJSON(allocator, child_subgraph, child_subgraph_json, allNodes, allEdges);
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
    var json_writer = std.Io.Writer.Allocating.init(wasm_allocator);
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

    readGraphJSON(arena_allocator, graph, graph_json);

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
