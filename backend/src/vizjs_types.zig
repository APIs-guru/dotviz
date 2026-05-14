const std = @import("std");
const Allocator = std.mem.Allocator;
const ParseOptions = std.json.ParseOptions;
const json = std.json;

pub const graphviz = @cImport({
    @cInclude("stdlib.h");
    @cInclude("const.h");
    @cInclude("geom.h");
    @cInclude("xdot.h");
    @cInclude("cgraph_wrapper.h");
    @cInclude("context_inline.h");
    @cInclude("layout_inline.h");
    @cInclude("gvusershape_size.h");
    @cInclude("inline_render_dot/render_inline_dot.h");
    @cInclude("inline_render_svg/render_svg.h");
});

pub const AttributeValue = union(enum) {
    text: [:0]const u8,
    html: [:0]const u8,
};

pub const Attributes = std.json.ArrayHashMap(?AttributeValue);

pub const Node = struct {
    name: [:0]const u8,
    attributes: Attributes,
};

pub const EdgePort = struct {
    node: usize,
    name: ?[:0]const u8 = null,
};

pub const EdgeEndpoint = struct {
    port: EdgePort,
    compass: ?[:0]const u8 = null,
};

pub const Edge = struct {
    tail: EdgeEndpoint,
    head: EdgeEndpoint,
    key: ?[:0]const u8 = null,
    attributes: Attributes,
};

pub const Subgraph = struct {
    name: ?[:0]const u8 = null,
    graphAttributes: Attributes,
    nodeAttributes: Attributes,
    edgeAttributes: Attributes,
    memberNodes: []usize,
    memberEdges: []usize,
    subgraphs: []Subgraph,
};

pub const Graph = struct {
    name: ?[:0]const u8 = null,
    directed: bool,
    strict: bool,
    graphAttributes: Attributes,
    nodeAttributes: Attributes,
    edgeAttributes: Attributes,
    allNodes: []Node,
    allEdges: []Edge,
    subgraphs: []Subgraph,

    pub fn initFromJson(
        allocator: std.mem.Allocator,
        json_string: []const u8,
    ) !Graph {
        const res = try std.json.parseFromSliceLeaky(
            Graph,
            allocator,
            json_string,
            .{ .ignore_unknown_fields = true },
        );
        return res;
    }
};

pub const ImageDimensions = struct {
    width: [:0]const u8,
    height: [:0]const u8,
};

pub const ImageDimensionsMap = std.json.ArrayHashMap(ImageDimensions);

pub const Engine = enum {
    dot,
    circo,
    neato,
    fdp,
    twopi,
    patchwork,
    osage,
    sfdp,
};

pub const RenderRequest = struct {
    graph: Graph,
    renderDot: bool,
    renderSvg: bool,
    engine: Engine,
    yInvert: bool,
    reduce: bool,
    images: ImageDimensionsMap,
};

const RenderStatus = enum {
    failure,
    success,
};

pub const RenderErrorLevel = enum {
    @"error",
    warning,
};

pub const JSONParseError = struct {
    line: u64,
    column: u64,
    err: [:0]const u8,
    json_line: []const u8,

    pub fn init(diagnostics: std.json.Diagnostics, err: anyerror, json_string: []const u8) JSONParseError {
        const offset: usize = @intCast(diagnostics.getByteOffset());
        const start = std.mem.lastIndexOfScalar(u8, json_string[0..offset], '\n') orelse 0;
        const end = std.mem.indexOfScalarPos(u8, json_string, offset, '\n') orelse json_string.len;
        const json_line = json_string[@max(start, offset -| 40)..@min(end, offset +| 40)];

        return .{
            .line = diagnostics.getLine(),
            .column = diagnostics.getColumn(),
            .err = @errorName(err),
            .json_line = json_line,
        };
    }

    pub fn jsonStringify(self: @This(), jws: anytype) !void {
        try jws.beginWriteRaw();
        try jws.writer.print("\"JSON error {s} at {d}:{d}: `", .{ self.err, self.line, self.column });
        try std.json.Stringify.encodeJsonStringChars(self.json_line, jws.options, jws.writer);
        try jws.writer.print("`\"", .{});
        jws.endWriteRaw();
    }
};

const RenderErrorMessage = union(enum) {
    slice: []const u8,
    err: JSONParseError,

    pub fn jsonStringify(self: @This(), jws: anytype) !void {
        switch (self) {
            .slice => {
                try jws.write(self.slice);
            },
            .err => {
                try self.err.jsonStringify(jws);
            },
        }
    }
};

pub const RenderError = struct {
    level: RenderErrorLevel,
    message: RenderErrorMessage,
};

const RenderOutput = struct {
    svg: ?[:0]const u8 = null,
    dot: ?[:0]const u8 = null,
};

pub const RenderResponse = struct {
    status: RenderStatus,
    diagnostics: []const RenderError = &.{},
    output: ?RenderOutput,
};
