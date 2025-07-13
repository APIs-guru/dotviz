const std = @import("std");
const lib = @import("dotviz_lib");

export var GVC: ?*lib.c.GVC_t = null;
pub fn main() !void {
    _ = lib.viz_svg_len();
}
