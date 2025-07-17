const std = @import("std");
const testing = std.testing;
const core = @import("core.zig");

pub const c = @cImport({
    @cInclude("gvc.h");
    @cInclude("hello.h");
    @cInclude("agrw.h");
});

const allocator = std.heap.wasm_allocator;
pub const Agrw_t = usize;

extern var GVC: ?*c.GVC_t;

pub export fn viz_dot_to_graph(dot_string: [*:0]const u8) Agrw_t {
    return c.read_graph_from_string(dot_string);
}

pub export fn viz_json_to_graph(json: [*c]const u8) Agrw_t {
    const parsed = core.parse_json_to_graph(
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

pub export fn viz_free_graph(graph: Agrw_t) void {
    _ = c.gw_agclose(graph);
}

pub export fn viz_layout_graph(graph: Agrw_t) c_int {
    return c.layout_graph(GVC, graph);
}

pub export fn viz_layout_done(graph: Agrw_t) bool {
    return c.layout_done(graph);
}

pub export fn viz_graph_to_svg(
    graph: Agrw_t,
    buf_ptr: [*]u8,
    buf_len: usize,
) usize {
    if (GVC == null or graph == 0) return 0;

    var written_len: usize = 0;

    const err = c.render_graph_to_svg(
        GVC,
        graph,
        buf_ptr,
        buf_len,
        &written_len,
    );

    if (err != 0) {
        std.debug.print("{d}\n", .{err});

        return 0;
    }
    return written_len;
}

pub export fn viz_create_context() void {
    GVC = c.create_context();
}

pub export fn viz_alloc(len: usize) ?[*]u8 {
    const mem = allocator.alloc(u8, len) catch return null;
    return mem.ptr;
}

pub export fn viz_free(ptr: [*]u8, len: usize) void {
    const slice = ptr[0..len];
    allocator.free(slice);
}
