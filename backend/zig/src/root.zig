const std = @import("std");
const testing = std.testing;

pub const c = @cImport({
    @cInclude("gvc.h");
    @cInclude("hello.h");
});

var output_buf: [64 * 1024]u8 = undefined;
var output_len: usize = 0;

pub export fn viz_dot_to_svg(dot_string: [*]const u8) ?[*]const u8 {
    const gvc = c.viz_create_context();
    if (gvc == null) return null;

    const err = c.hello(
        gvc,
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
