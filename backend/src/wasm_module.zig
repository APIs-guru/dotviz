const std = @import("std");
const testing = std.testing;
const vizjs_types = @import("vizjs_types.zig");

pub const c = @cImport({
    @cInclude("gvc.h");
    @cInclude("agrw.h");
});

const allocator = std.heap.wasm_allocator;
pub const Agrw_t = usize;

pub export fn viz_dot_to_graph(dot_string: [*:0]const u8) Agrw_t {
    return c.gw_agmemread(dot_string);
}

pub export fn viz_json_to_graph(json: [*c]const u8) Agrw_t {
    const parsed = vizjs_types.Graph.initFromJson(
        allocator,
        std.mem.span(json),
    ) catch unreachable;

    var agrw: Agrw_t = 0;
    if (parsed.name) |graph_name| {
        const string = allocator.dupeZ(u8, graph_name) catch unreachable;
        defer allocator.free(string);
        agrw = c.gw_agopen(string, c.Agrw_directed);
    } else {
        const default_name: [*c]const u8 = "graph";
        agrw = c.gw_agopen(default_name, c.Agrw_directed);
    }

    var nodes = std.StringHashMap(c.Agrw_node_t).init(allocator);
    defer nodes.deinit();

    if (parsed.nodes) |nodes_| {
        for (nodes_) |node| {
            const node_name = allocator.dupeZ(u8, node.name) catch unreachable;
            defer allocator.free(node_name);
            const gw_node = c.gw_agnode(agrw, node_name);
            nodes.put(node.name, gw_node) catch unreachable;
        }
    }

    if (parsed.edges) |edges| {
        for (edges) |edge| {
            const tail = nodes.get(edge.tail);
            const head = nodes.get(edge.head);
            if (tail) |_tail| {
                if (head) |_head| {
                    _ = c.gw_agedge(agrw, _tail, _head);
                }
            }
        }
    }

    return agrw;
}

// pub export fn viz_free_graph(graph: Agrw_t) void {
//     _ = c.gw_gvFreeLayout(GVC, graph);
//     _ = c.gw_agclose(graph);
// }

// pub export fn viz_layout_graph(graph: Agrw_t) c_int {
//     return c.gw_gvLayoutDot(GVC, graph);
// }

// pub export fn viz_layout_done(graph: Agrw_t) bool {
//     return c.gw_gvLayoutDone(GVC, graph);
// }

// pub export fn viz_graph_to_svg(
//     graph: Agrw_t,
// ) ?[*]u8 {
//     if (GVC == null or graph == 0) return null;
//     var buf: ?[*]u8 = null;
//     var buf_len: usize = 0;
//     const err = c.gw_gvRenderDataSvg(GVC, graph, &buf, &buf_len);
//     if (err != 0) {
//         std.debug.print("{d}\n", .{err});
//         c.gw_gvFreeRenderData(buf);
//         return null;
//     }
//     return buf;
// }

// pub export fn viz_free_svg(buf: ?[*]u8) void {
//     c.gw_gvFreeRenderData(buf);
// }

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
) ?[*]u8 {
    std.debug.assert(gvc != null);
    std.debug.assert(graph != 0);
    var buf: ?[*]u8 = null;
    var buf_len: usize = 0;
    const err = c.gw_gvRenderDataSvg(gvc, graph, &buf, &buf_len);
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
    std.debug.assert(graph != 0);
    _ = c.gw_gvFreeLayout(gvc, graph);
}

pub export fn viz_free_graph(graph: Agrw_t) void {
    std.debug.assert(graph != 0);
    _ = c.gw_agclose(graph);
}

pub export fn viz_free_context(gvc: GVC) void {
    std.debug.assert(gvc != null);
    _ = c.gvFinalize(gvc);
    _ = c.gvFreeContext(gvc);
}

pub export fn wasm_alloc(len: usize) ?[*]u8 {
    const mem = allocator.alloc(u8, len) catch return null;
    return mem.ptr;
}

pub export fn wasm_free(ptr: [*]u8, len: usize) void {
    const slice = ptr[0..len];
    allocator.free(slice);
}

pub export fn viz_read_one_graph_from_dot(string: [*c]u8) Agrw_t {
    // _ = string;
    // @panic("viz_read_one_graph_from_dot");
    var graph: Agrw_t = 0;

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
