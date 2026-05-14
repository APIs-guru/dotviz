const std = @import("std");
const wasm_allocator = std.heap.wasm_allocator;

const vizjs_types = @import("vizjs_types.zig");
const graphviz = vizjs_types.graphviz;
const readGraphJSON = @import("read_graph_json.zig").readGraphJSON;

extern var Y_invert: bool;
extern var Reduce: bool;

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
) std.ArrayList(vizjs_types.RenderError) {
    const array_list = &errors_strings.array_list;
    if (array_list.items.len == 0) {
        return .empty;
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
                .message = .{ .slice = std.mem.trimEnd(u8, items[i], " \n") },
            }) catch @panic("cannot allocate");
        }
    }
    return result;
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

fn stringifyResponseJSON(response: vizjs_types.RenderResponse) WasmString {
    // use global WASM allocator since response is need to be passed to JS
    var json_writer = std.Io.Writer.Allocating.init(wasm_allocator);
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
    defer arena.deinit();
    const arena_allocator = arena.allocator();

    const json_string = json_bytes[0..size];
    defer wasm_allocator.free(json_string);

    var scanner = std.json.Scanner.initCompleteInput(
        arena_allocator,
        json_string,
    );
    defer scanner.deinit();

    var diag = std.json.Diagnostics{};
    scanner.enableDiagnostics(&diag);
    const request = std.json.parseFromTokenSourceLeaky(
        vizjs_types.RenderRequest,
        arena_allocator,
        &scanner,
        .{},
    ) catch |err| {
        const json_error = vizjs_types.JSONParseError.init(diag, err, json_string);
        return stringifyResponseJSON(.{
            .status = .failure,
            .diagnostics = &[_]vizjs_types.RenderError{.{
                .level = .@"error",
                .message = .{ .err = json_error },
            }},
            .output = null,
        });
    };

    errors_strings = .{
        .allocator = arena_allocator,
        .array_list = .empty,
    };

    // Reset errors
    _ = graphviz.agseterrf(viz_errorf);
    _ = graphviz.agseterr(graphviz.AGWARN);
    _ = graphviz.agreseterrors();

    g_image_map = request.images;

    Y_invert = request.yInvert;
    Reduce = request.reduce;

    const graph = readGraphJSON(arena_allocator, request.graph);
    if (graph == null) {
        return stringifyResponseJSON(.{
            .status = .failure,
            .diagnostics = parseAgerrMessages(arena_allocator).items,
            .output = null,
        });
    }
    defer _ = graphviz.agclose(graph);

    const gvc = graphviz.gw_create_context();
    defer _ = graphviz.gvFreeContext(gvc);

    layoutRender(request.engine, gvc, graph);
    defer layoutCleanup(request.engine, graph);

    var responseDot: ?[:0]const u8 = null;
    defer freeCString(responseDot);
    if (request.renderDot) {
        const output = graphviz.render_dot(graph);
        responseDot = @ptrCast(output.data[0..output.data_position]);
    }

    var responseSvg: ?[:0]const u8 = null;
    defer freeCString(responseSvg);
    if (request.renderSvg) {
        const output = graphviz.render_svg(graph);
        responseSvg = @ptrCast(output.data[0..output.data_position]);
    }

    const responseJSON = stringifyResponseJSON(.{
        .status = .success,
        .diagnostics = parseAgerrMessages(arena_allocator).items,
        .output = .{
            .dot = responseDot,
            .svg = responseSvg,
        },
    });
    return responseJSON;
}

fn layoutRender(engine: vizjs_types.Engine, gvc: ?*graphviz.GVC_t, graph: ?*graphviz.Agraph_t) void {
    switch (engine) {
        .dot => {
            graphviz.my_graph_init(gvc, graph, true);
            graphviz.dot_layout(graph);
        },
        .circo => {
            graphviz.my_graph_init(gvc, graph, false);
            graphviz.circo_layout(graph);
        },
        .neato => {
            graphviz.my_graph_init(gvc, graph, false);
            graphviz.neato_layout(graph);
        },
        .fdp => {
            graphviz.my_graph_init(gvc, graph, false);
            graphviz.fdp_layout(graph);
        },
        .twopi => {
            graphviz.my_graph_init(gvc, graph, false);
            graphviz.twopi_layout(graph);
        },
        .patchwork => {
            graphviz.my_graph_init(gvc, graph, false);
            graphviz.patchwork_layout(graph);
        },
        .osage => {
            graphviz.my_graph_init(gvc, graph, false);
            graphviz.osage_layout(graph);
        },
        .sfdp => {
            graphviz.my_graph_init(gvc, graph, false);
            graphviz.sfdp_layout(graph);
        },
    }

    // FIXME: IMPORTANT: check that we don't use GVC after this line
    graphviz.graphInfo(graph).*.gvc = null;
}

fn layoutCleanup(engine: vizjs_types.Engine, graph: ?*graphviz.Agraph_t) void {
    switch (engine) {
        .dot => graphviz.dot_cleanup(graph),
        .circo => graphviz.circo_cleanup(graph),
        .neato => graphviz.neato_cleanup(graph),
        .fdp => graphviz.fdp_cleanup(graph),
        .twopi => graphviz.twopi_cleanup(graph),
        .patchwork => graphviz.patchwork_cleanup(graph),
        .osage => graphviz.osage_cleanup(graph),
        .sfdp => graphviz.sfdp_cleanup(graph),
    }
    graphviz.graph_cleanup(graph);
}

fn freeCString(string: ?[:0]const u8) void {
    if (string) |slice| {
        std.c.free(@ptrCast(@constCast(slice.ptr)));
    }
}

var g_image_map: vizjs_types.ImageDimensionsMap = undefined;
export fn gvusershape_size(graph: *graphviz.Agraph_t, name: [*c]u8) graphviz.point {
    const dimensions = g_image_map.map.get(std.mem.span(name)) orelse @panic("no image found");
    return graphviz.my_gvusershape_size(graph, dimensions.height, dimensions.width);
}

export fn get_dimensions_by_name(name: [*c]u8, dpi: graphviz.pointf) graphviz.point {
    const dimensions = g_image_map.map.get(std.mem.span(name)) orelse return .{
        .x = -1,
        .y = -1,
    };
    return graphviz.convert_image_dimensions(dpi, dimensions.height, dimensions.width);
}
