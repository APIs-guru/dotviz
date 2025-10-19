const std = @import("std");
const testing = std.testing;
const vizjs_types = @import("vizjs_types.zig");
const wasm_allocator = std.heap.wasm_allocator;

pub const c = @cImport({
    @cInclude("gvc.h");
    @cInclude("agrw.h");
    @cInclude("layout_inline.h");
    @cInclude("render_inline.h");
});

extern var Y_invert: bool;
extern var Reduce: bool;
pub const Agrw_t = ?*anyopaque;

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
        const name = allocator.dupeZ(u8, attr.key_ptr.*) catch @panic(
            "cannot dupeZ in setDefaultAttributes",
        );
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
        const name = allocator.dupeZ(u8, attr.key_ptr.*) catch @panic(
            "cannot dupeZ in setAttributes",
        );
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

fn setAllDefaultAttributes(
    allocator: std.mem.Allocator,
    graph: anytype,
    json: anytype,
) void {
    if (json.graphAttributes) |attributes| {
        setDefaultAttributes(allocator, graph, attributes, c.AGRAPH);
    }
    if (json.nodeAttributes) |attributes| {
        setDefaultAttributes(allocator, graph, attributes, c.AGNODE);
    }
    if (json.edgeAttributes) |attributes| {
        setDefaultAttributes(allocator, graph, attributes, c.AGEDGE);
    }
}

fn readGraph(allocator: std.mem.Allocator, graph: anytype, json: anytype) void {
    setAllDefaultAttributes(allocator, graph, json);
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

const GVC = ?*c.GVC_t;

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
    array_list: std.ArrayList([]const u8),
) []vizjs_types.RenderError {
    if (array_list.items.len == 0) {
        return &.{};
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
                .message = std.mem.trimEnd(u8, items[i], " \n"),
            }) catch @panic("cannot allocate");
        }
    }
    return result.toOwnedSlice(allocator) catch @panic("cannot allocate");
}

fn parseRequestJSON(allocator: std.mem.Allocator, json_bytes: [*]u8, size: usize) vizjs_types.RenderRequest {
    const json_string = json_bytes[0..size];
    const request: vizjs_types.RenderRequest = std.json.parseFromSliceLeaky(
        vizjs_types.RenderRequest,
        allocator,
        json_string,
        .{},
    ) catch @panic("cannot parse"); // TODO: implement error handling
    // https://ziggit.dev/t/example-using-diagnostics-for-error-handling-with-std-json/7882/2
    return request;
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

fn stringifyResponseJSON(allocator: std.mem.Allocator, response: vizjs_types.RenderResponse) WasmString {
    var json_writer = std.io.Writer.Allocating.init(allocator);
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
    const arena_allocator = arena.allocator();
    defer arena.deinit();

    const request = parseRequestJSON(arena_allocator, json_bytes, size);

    errors_strings = .{
        .allocator = arena_allocator,
        .array_list = .empty,
    };

    // Reset errors
    _ = c.agseterrf(viz_errorf);
    _ = c.agseterr(c.AGWARN);
    _ = c.agreseterrors();

    const graphptr: Agrw_t = switch (request.graph) {
        .dot => |dot_string| blk: {
            var graph: Agrw_t = null;

            // Try to read one graph
            graph = c.gw_agmemread(dot_string);

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

            break :blk graph;
        },
        .graph => |graph_json| blk: {
            const graph = c.gw_agopen(
                toCString(graph_json.name),
                graph_json.directed,
                graph_json.strict,
            );
            if (graph != null) {
                readGraph(arena_allocator, graph, graph_json);
            }
            break :blk graph;
        },
    };

    if (graphptr == null) {
        return stringifyResponseJSON(wasm_allocator, .{
            .status = .failure,
            .errors = parseAgerrMessages(
                arena_allocator,
                errors_strings.array_list,
            ),
            .output = null,
        });
    }
    defer _ = c.gw_agclose(graphptr);

    setAllDefaultAttributes(arena_allocator, graphptr, request);

    // FIXME: maybe call setDefaultAttributes

    Y_invert = request.yInvert;
    Reduce = request.reduce;

    const gvc = c.gw_create_context();
    defer {
        _ = c.gvFinalize(gvc);
        _ = c.gvFreeContext(gvc);
    }

    if (c.gw_gvLayoutDot(gvc, graphptr) != 0) {
        return stringifyResponseJSON(wasm_allocator, .{
            .status = .failure,
            .errors = parseAgerrMessages(
                arena_allocator,
                errors_strings.array_list,
            ),
            .output = null,
        });
    }
    defer _ = c.gw_gvFreeLayout(graphptr);

    var responseDot: ?[:0]const u8 = null;
    defer freeCString(responseDot);
    if (request.renderDot) {
        const output = c.render_dot(graphptr);
        var response: [:0]const u8 = undefined;
        response.ptr = output.data;
        response.len = output.data_position;
        responseDot = response;
    }

    var responseSvg: ?[:0]const u8 = null;
    defer freeCString(responseSvg);
    if (request.renderSvg) {
        var response: [:0]const u8 = undefined;
        c.gw_gvRenderData(
            gvc,
            graphptr,
            @ptrCast(&response.ptr),
            &response.len,
        );
        responseSvg = response;
    }

    const responseJSON = stringifyResponseJSON(wasm_allocator, .{
        .status = .success,
        .errors = parseAgerrMessages(
            arena_allocator,
            errors_strings.array_list,
        ),
        .output = .{
            .dot = responseDot,
            .svg = responseSvg,
        },
    });
    return responseJSON;
}

fn freeCString(string: ?[:0]const u8) void {
    if (string) |slice| {
        std.c.free(@ptrCast(@constCast(slice.ptr)));
    }
}
