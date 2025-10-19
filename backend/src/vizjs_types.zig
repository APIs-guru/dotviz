const std = @import("std");
const Allocator = std.mem.Allocator;
const ParseOptions = std.json.ParseOptions;
const json = std.json;

pub const AttributeValue = union(enum) {
    text: [:0]const u8,
    html: [:0]const u8,

    const Self = @This();

    pub fn jsonParse(allocator: Allocator, source: anytype, options: ParseOptions) !Self {
        // FIXME: check. if needed
        _ = options.max_value_len.?;

        const next_type = try source.peekNextTokenType();

        switch (next_type) {
            .string => {
                const s = try json.innerParse([:0]u8, allocator, source, options);
                return Self{ .text = s };
            },
            .number => {
                const f = try json.innerParse(f64, allocator, source, options);
                const s = try std.fmt.allocPrintSentinel(allocator, "{d}", .{f}, 0);
                return Self{ .text = s };
            },
            .true, .false => {
                const b = try json.innerParse(bool, allocator, source, options);
                return Self{ .text = if (b) "true" else "false" };
            },
            .object_begin => {
                const h = try json.innerParse(struct {
                    html: [:0]const u8,
                }, allocator, source, options);
                return Self{ .html = h.html };
            },
            else => return error.UnexpectedToken,
        }
    }
};

const CString = struct {
    cstring: [:0]u8,

    const Self = @This();

    pub fn jsonParse(allocator: Allocator, source: anytype, options: ParseOptions) !Self {
        const s = try json.innerParse([:0]u8, allocator, source, options);
        return Self{ .string = s };
    }
};

pub const Attributes = std.json.ArrayHashMap(AttributeValue);

pub const Node = struct {
    name: [:0]const u8,
    attributes: ?*Attributes = null,
};

pub const Edge = struct {
    tail: [:0]const u8,
    head: [:0]const u8,
    attributes: ?*Attributes = null,
};

pub const Subgraph = struct {
    name: ?[:0]const u8 = null,
    graphAttributes: ?*Attributes = null,
    nodeAttributes: ?*Attributes = null,
    edgeAttributes: ?*Attributes = null,
    nodes: ?[]Node = null,
    edges: ?[]Edge = null,
    subgraphs: ?[]Subgraph = null,
};

pub const Graph = struct {
    name: ?[:0]const u8 = null,
    directed: bool = true,
    strict: bool = false,
    graphAttributes: ?*Attributes = null,
    nodeAttributes: ?*Attributes = null,
    edgeAttributes: ?*Attributes = null,
    nodes: ?[]Node = null,
    edges: ?[]Edge = null,
    subgraphs: ?[]Subgraph = null,

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

const GraphInput = union(enum) {
    dot: [:0]const u8,
    graph: Graph,
};

pub const ImageDimensions = struct {
    width: u32,
    height: u32,
};

pub const ImageDimensionsMap = std.json.ArrayHashMap(ImageDimensions);

pub const RenderRequest = struct {
    graph: GraphInput,
    graphAttributes: ?*Attributes,
    nodeAttributes: ?*Attributes,
    edgeAttributes: ?*Attributes,
    renderDot: bool,
    renderSvg: bool,
    engine: [:0]const u8,
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

pub const RenderError = struct {
    level: RenderErrorLevel,
    message: []const u8,
};

const RenderOutput = struct {
    svg: ?[:0]const u8 = null,
    dot: ?[:0]const u8 = null,
};

pub const RenderResponse = struct {
    status: RenderStatus,
    errors: []RenderError = &.{},
    output: ?RenderOutput,
};
