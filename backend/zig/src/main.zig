const std = @import("std");
const lib = @import("dotviz_lib");

export var GVC: ?*lib.c.GVC_t = null;
pub fn main() !void {
    _ = lib.viz_create_context();
    const dot = "digraph {a -> b}";
    const graphptr = lib.viz_dot_to_graph(dot);
    _ = lib.viz_layout_graph(graphptr);
    const svg = lib.viz_graph_to_svg(graphptr).?;
    std.debug.print("{s}\n", .{@as([*c]u8, @ptrCast(svg))});
}
