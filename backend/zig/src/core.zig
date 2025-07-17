const std = @import("std");
const vizjs_types = @import("vizjs_types.zig");

pub fn parse_json_to_graph(
    allocator: std.mem.Allocator,
    json_string: []const u8,
) !vizjs_types.Graph {
    const res = try std.json.parseFromSliceLeaky(
        vizjs_types.Graph,
        allocator,
        json_string,
        .{ .ignore_unknown_fields = true },
    );
    return res;
}
