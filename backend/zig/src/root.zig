const std = @import("std");
const testing = std.testing;
const core = @import("core.zig");

pub const c = @cImport({
    @cInclude("gvc.h");
    @cInclude("hello.h");
    @cInclude("agrw.h");
});
const wasm_allocator = std.heap.wasm_allocator;
pub const Agrw_t = usize;

var output_buf: [64 * 1024]u8 = undefined;
var output_len: usize = 0;

extern var GVC: ?*c.GVC_t;

pub export fn viz_dot_to_svg(dot_string: [*c]const u8) ?[*]const u8 {
    if (GVC == null) return null;

    const err = c.hello(
        GVC,
        dot_string,
        &output_buf,
        output_buf.len,
        &output_len,
    );

    if (err != 0) return null;

    return &output_buf;
}

pub export fn viz_svg_len() usize {
    return output_len;
}

pub export fn viz_json_to_graph(json: [*c]const u8) Agrw_t {
    const graph = core.parse_json_to_graph(
        std.heap.wasm_allocator,
        std.mem.span(json),
    ) catch unreachable;
    var agrw: Agrw_t = 0;
    if (graph.name) |graph_name| {
        const string = wasm_allocator.dupeZ(
            u8,
            graph_name,
        ) catch unreachable;
        defer wasm_allocator.free(string);
        agrw = c.gw_agopen(string, c.Agrw_directed);
    } else {
        const string: [*c]const u8 = "graph";
        agrw = c.gw_agopen(string, c.Agrw_directed);
    }

    var nodes = std.StringHashMap(c.Agrw_node_t).init(wasm_allocator);
    defer nodes.deinit();
    for (graph.nodes.?) |node| {
        const node_name = wasm_allocator.dupeZ(u8, node.name) catch unreachable;
        defer wasm_allocator.free(node_name);
        const gw_node = c.gw_agnode(agrw, node_name);
        nodes.put(node.name, gw_node) catch unreachable;
    }

    for (graph.edges.?) |edge| {
        const tail = nodes.get(edge.tail);
        const head = nodes.get(edge.head);
        if (tail) |_tail| {
            if (head) |_head| {
                _ = c.gw_agedge(agrw, _tail, _head);
            }
        }
    }
    return agrw;
}

pub export fn viz_free_graph(graph: Agrw_t) void {
    _ = c.gw_agclose(graph);
}

pub export fn viz_graph_to_svg(graph: Agrw_t) ?[*]const u8 {
    if (GVC == null or graph == 0) return "a";
    _ = c.render_graph_to_svg(
        GVC,
        graph,
        &output_buf,
        output_buf.len,
        &output_len,
    );
    return &output_buf;
}

pub export fn viz_create_context() void {
    GVC = c.create_context();
}
