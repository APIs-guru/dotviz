const std = @import("std");
const testing = std.testing;
const vizjs_types = @import("vizjs_types.zig");
const wasm_allocator = std.heap.wasm_allocator;

pub const c = @cImport({
    @cInclude("gvc.h");
    @cInclude("agrw.h");
    @cInclude("layout_inline.h");
});

pub const Agrw_t = ?*anyopaque;

pub export fn viz_dot_to_graph(dot_string: [*:0]const u8) Agrw_t {
    return c.gw_agmemread(dot_string);
}

fn toCString(maybe_string: ?[:0]const u8) [*c]const u8 {
    if (maybe_string) |string| {
        return @ptrCast(string.ptr);
    }
    return null;
}

fn setDefaultAttributes(
    allocator: std.mem.Allocator,
    graph: Agrw_t,
    attributes: *vizjs_types.Attributes,
    kind: c_int,
) void {
    var iterator = attributes.map.iterator();
    while (iterator.next()) |attr| {
        const name = allocator.dupeZ(u8, attr.key_ptr.*) catch unreachable;
        switch (attr.value_ptr.*) {
            .text => |val| {
                c.gw_agattr_text(graph, kind, name, @ptrCast(val.ptr));
            },
            .html => |val| {
                c.gw_agattr_html(graph, kind, name, @ptrCast(val.ptr));
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
        const name = allocator.dupeZ(u8, attr.key_ptr.*) catch unreachable;
        switch (attr.value_ptr.*) {
            .text => |val| {
                c.gw_agsafeset_text(object, name, @ptrCast(val.ptr));
            },
            .html => |val| {
                c.gw_agsafeset_html(object, name, @ptrCast(val.ptr));
            },
        }
    }
}

fn readGraph(allocator: std.mem.Allocator, graph: anytype, json: anytype) void {
    // set default attributes
    if (json.graphAttributes) |attributes| {
        setDefaultAttributes(allocator, graph, attributes, c.AGRAPH);
    }
    if (json.nodeAttributes) |attributes| {
        setDefaultAttributes(allocator, graph, attributes, c.AGNODE);
    }
    if (json.edgeAttributes) |attributes| {
        setDefaultAttributes(allocator, graph, attributes, c.AGEDGE);
    }

    if (json.nodes) |nodes| {
        for (nodes) |node| {
            const node_ptr = c.gw_agnode(graph, node.name);
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
            const tail_ptr = c.gw_agnode(graph, @ptrCast(edge.tail.ptr));
            const head_ptr = c.gw_agnode(graph, @ptrCast(edge.head.ptr));
            const edge_ptr = c.gw_agedge(graph, tail_ptr, head_ptr);
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
            const subgraph_ptr = c.gw_agsubg(graph, toCString(subgraph.name));
            readGraph(allocator, subgraph_ptr, subgraph);
        }
    }
}

pub export fn viz_json_to_graph(json_bytes: [*]u8, size: usize) Agrw_t {
    var arena = std.heap.ArenaAllocator.init(wasm_allocator);
    const arena_allocator = arena.allocator();
    defer arena.deinit();

    const json_string = json_bytes[0..size];
    const json: vizjs_types.Graph = std.json.parseFromSliceLeaky(
        vizjs_types.Graph,
        arena_allocator,
        json_string,
        .{},
    ) catch unreachable; // TODO: implement error handling
    // https://ziggit.dev/t/example-using-diagnostics-for-error-handling-with-std-json/7882/2

    const graph = c.gw_agopen(toCString(json.name), json.directed, json.strict);

    readGraph(arena_allocator, graph, json);

    return graph;
}

///////////////////////////////////////////////// new functions

extern var Y_invert: bool;
pub export fn viz_set_y_invert(value: bool) void {
    Y_invert = value;
}

extern var Reduce: bool;
pub export fn viz_set_reduce(value: bool) void {
    Reduce = value;
}

pub export fn viz_create_context() ?*c.GVC_t {
    return c.gw_create_context();
}

extern fn jsHandleGraphvizError(ptr: [*]const u8) void;

fn viz_errorf(text: [*c]u8) callconv(.c) c_int {
    jsHandleGraphvizError(text);
    return 0;
}

pub export fn viz_reset_errors() void {
    _ = c.agseterrf(&viz_errorf);
    _ = c.agseterr(c.AGWARN);
    _ = c.agreseterrors();
}

const GVC = ?*c.GVC_t;

pub export fn viz_layout(
    gvc: GVC,
    graph: Agrw_t,
) c_int {
    std.debug.assert(gvc != null);
    return c.gw_gvLayoutDot(gvc, graph);
}

pub export fn viz_render(
    gvc: GVC,
    graph: Agrw_t,
    format: [*c]u8,
) ?[*]u8 {
    std.debug.assert(gvc != null);
    std.debug.assert(graph != null);
    var buf: ?[*]u8 = null;
    var buf_len: usize = 0;
    const err = c.gw_gvRenderData(gvc, graph, format, &buf, &buf_len);
    if (err != 0) {
        c.gw_gvFreeRenderData(buf);
        return null;
    }
    return buf;
}

pub export fn viz_free_svg(buf: ?[*]u8) void {
    c.gw_gvFreeRenderData(buf);
}

pub export fn viz_free_layout(gvc: GVC, graph: Agrw_t) void {
    std.debug.assert(gvc != null);
    std.debug.assert(graph != null);
    _ = c.gw_gvFreeLayout(gvc, graph);
}

pub export fn viz_free_graph(graph: Agrw_t) void {
    std.debug.assert(graph != null);
    _ = c.gw_agclose(graph);
}

pub export fn viz_free_context(gvc: GVC) void {
    std.debug.assert(gvc != null);
    _ = c.gvFinalize(gvc);
    _ = c.gvFreeContext(gvc);
}

pub export fn wasm_alloc(len: usize) ?[*]u8 {
    const mem = wasm_allocator.alloc(u8, len) catch return null;
    return mem.ptr;
}

pub export fn wasm_free(ptr: [*]u8, len: usize) void {
    const slice = ptr[0..len];
    wasm_allocator.free(slice);
}

pub export fn viz_read_one_graph_from_dot(string: [*c]u8) Agrw_t {
    // _ = string;
    // @panic("viz_read_one_graph_from_dot");
    var graph: Agrw_t = null;

    // Reset errors

    _ = c.agseterrf(viz_errorf);
    _ = c.agseterr(c.AGWARN);
    _ = c.agreseterrors();

    // Try to read one graph
    graph = c.gw_agmemread(string);

    // Consume the rest of the input
    // while (true) {
    //     // FIXME: figure out why it is here
    //     var other_graph: ?Agrw_t = null;
    //     other_graph = c.gw_agmemread(null);
    //     if (other_graph) |g| {
    //         _ = c.gw_agclose(g);
    //     } else {
    //         break;
    //     }
    // }

    return graph;
}

pub export fn viz_set_default_graph_attribute(
    graph: Agrw_t,
    name: [*c]u8,
    value: [*c]u8,
    is_html: bool,
) void {
    if (is_html) {
        c.gw_agattr_html(graph, c.AGRAPH, name, value);
    } else {
        c.gw_agattr_text(graph, c.AGRAPH, name, value);
    }
}

pub export fn viz_set_default_node_attribute(
    graph: Agrw_t,
    name: [*c]u8,
    value: [*c]u8,
    is_html: bool,
) void {
    if (is_html) {
        c.gw_agattr_html(graph, c.AGNODE, name, value);
    } else {
        c.gw_agattr_text(graph, c.AGNODE, name, value);
    }
}

pub export fn viz_set_default_edge_attribute(
    graph: Agrw_t,
    name: [*c]u8,
    value: [*c]u8,
    is_html: bool,
) void {
    if (is_html) {
        c.gw_agattr_html(graph, c.AGEDGE, name, value);
    } else {
        c.gw_agattr_text(graph, c.AGEDGE, name, value);
    }
}
