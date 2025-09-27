// TEST: to use with wasmtime
// zig build run -fwasmtime

const std = @import("std");
const lib = @import("dotviz_lib");

pub fn main() !void {
    _ = lib;
    // const res = lib.viz_dot_to_graph("digraph {a -> b}");
    // std.debug.print("{d}\n", .{res});
    // _ = lib.viz_create_context();
    // const dot = "digraph {a -> b}";
    // const graphptr = lib.viz_dot_to_graph(dot);
    // _ = lib.viz_layout_graph(graphptr);
    // defer lib.viz_free_graph(graphptr);
    // const svg = lib.viz_graph_to_svg(graphptr).?;
    // defer lib.viz_free_svg(svg);
    // std.debug.print("{s}\n", .{@as([*c]u8, @ptrCast(svg))});
}
