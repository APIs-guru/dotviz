const std = @import("std");
const vizjs_types = @import("vizjs_types.zig");
const graphviz = vizjs_types.graphviz;

pub fn readGraphJSON(allocator: std.mem.Allocator, graph_json: vizjs_types.Graph) ?*graphviz.Agraph_t {
    const graph = graphviz.wrapped_agopen(
        graph_json.name orelse null,
        graph_json.directed,
        graph_json.strict,
    );
    if (graph == null) {
        return graph;
    }

    setDefaultAttributes(allocator, graph, graph_json.graphAttributes, graphviz.AGRAPH);
    setDefaultAttributes(allocator, graph, graph_json.nodeAttributes, graphviz.AGNODE);
    setDefaultAttributes(allocator, graph, graph_json.edgeAttributes, graphviz.AGEDGE);

    _ = graphviz.agbindrec(graph, "Agraphinfo_t", @sizeOf(graphviz.Agraphinfo_t), graphviz.true);
    const info = graphviz.graphInfo(graph);

    const drawing = graphviz.calloc(@sizeOf(graphviz.layout_t), 1);
    if (drawing == null) @panic("cannot alloc graphviz.layout_t");
    info.*.drawing = @ptrCast(@alignCast(drawing));
    info.*.charset = graphviz.CHAR_UTF8;

    const allNodes = allocator.alloc(?*graphviz.Agnode_t, graph_json.allNodes.len) catch @panic(
        "cannot alloc for allNodes",
    );
    for (graph_json.allNodes, 0..) |node_json, i| {
        const node_ptr = agnode(allocator, graph, node_json.name);
        setAttributes(allocator, node_ptr, node_json.attributes);
        allNodes[i] = node_ptr;
    }

    const allEdges = allocator.alloc(?*graphviz.Agedge_t, graph_json.allEdges.len) catch @panic(
        "cannot alloc for allEdges",
    );
    for (graph_json.allEdges, 0..) |edge_json, i| {
        const tail = edge_json.tail;
        const head = edge_json.head;

        const tail_node = allNodes[tail.node];
        const head_node = allNodes[head.node];
        const edge = agedge(allocator, graph, tail_node, head_node, edge_json.key);
        if (edgePortToString(allocator, tail, graph_json)) |tailport| {
            agsafeset_text(allocator, edge, "tailport", tailport);
        }
        if (edgePortToString(allocator, head, graph_json)) |headport| {
            agsafeset_text(allocator, edge, "headport", headport);
        }
        setAttributes(allocator, edge, edge_json.attributes);
        allEdges[i] = edge;
    }

    for (graph_json.subgraphs) |subgraphIndex| {
        readSubgraphJSON(allocator, graph, graph_json, subgraphIndex, allNodes, allEdges);
    }

    return graph;
}

fn agnode(allocator: std.mem.Allocator, graph: ?*graphviz.Agraph_t, name: [:0]const u8) ?*graphviz.Agnode_t {
    const c_name = allocator.dupeZ(u8, name) catch @panic(
        "cannot alloc for node name",
    );
    return graphviz.agnode(graph, c_name, graphviz.true);
}

fn agedge(allocator: std.mem.Allocator, graph: ?*graphviz.Agraph_t, tail: ?*graphviz.Agnode_t, head: ?*graphviz.Agnode_t, maybeName: ?[:0]const u8) ?*graphviz.Agedge_t {
    const c_name: [*c]u8 = if (maybeName) |name| allocator.dupeZ(u8, name) catch @panic(
        "cannot alloc for edge key",
    ) else null;
    return graphviz.agedge(graph, tail, head, c_name, graphviz.true);
}

fn agsubg(allocator: std.mem.Allocator, graph: ?*graphviz.Agraph_t, maybeName: ?[:0]const u8) ?*graphviz.Agraph_t {
    const c_name: [*c]u8 = if (maybeName) |name| allocator.dupeZ(u8, name) catch @panic(
        "cannot alloc for subgraph name",
    ) else null;
    return graphviz.agsubg(graph, c_name, graphviz.true);
}

fn agsafeset_text(allocator: std.mem.Allocator, obj: ?*anyopaque, name: [:0]const u8, value: [*c]const u8) void {
    const c_name = allocator.dupeZ(u8, name) catch @panic(
        "cannot alloc for attribute name",
    );
    _ = graphviz.agsafeset_text(obj, c_name, value, "");
}

fn readSubgraphJSON(
    allocator: std.mem.Allocator,
    owner: ?*graphviz.Agraph_t,
    graph_json: vizjs_types.Graph,
    subgraphIndex: usize,
    allNodes: []?*graphviz.Agnode_t,
    allEdges: []?*graphviz.Agedge_t,
) void {
    const subgraph_json = graph_json.allSubgraphs[subgraphIndex];
    const subgraph = agsubg(allocator, owner, subgraph_json.name);
    setDefaultAttributes(allocator, subgraph, subgraph_json.graphAttributes, graphviz.AGRAPH);
    setDefaultAttributes(allocator, subgraph, subgraph_json.nodeAttributes, graphviz.AGNODE);
    setDefaultAttributes(allocator, subgraph, subgraph_json.edgeAttributes, graphviz.AGEDGE);

    for (subgraph_json.memberNodes) |node| {
        _ = graphviz.agsubnode(subgraph, allNodes[node], graphviz.true);
    }

    for (subgraph_json.memberEdges) |edge| {
        _ = graphviz.agsubedge(subgraph, allEdges[edge], graphviz.true);
    }

    for (subgraph_json.subgraphs) |childSubgraphIndex| {
        readSubgraphJSON(allocator, subgraph, graph_json, childSubgraphIndex, allNodes, allEdges);
    }
}

fn setDefaultAttributes(
    allocator: std.mem.Allocator,
    graph: ?*graphviz.Agraph_t,
    attributes: vizjs_types.Attributes,
    kind: c_int,
) void {
    var iterator = attributes.map.iterator();
    while (iterator.next()) |attr| {
        const name = allocator.dupeZ(u8, attr.key_ptr.*) catch @panic(
            "cannot dupeZ in setDefaultAttributes",
        );

        if (graphviz.agattr_text(graph, kind, name, null) == null) {
            _ = graphviz.agattr_text(graph, kind, name, "");
        }

        const sym = if (attr.value_ptr.*) |value| switch (value) {
            .text => |val| graphviz.agattr_text(graph, kind, name, @ptrCast(val.ptr)),
            .html => |val| graphviz.agattr_html(graph, kind, name, @ptrCast(val.ptr)),
        } else graphviz.agattr_text(graph, kind, name, "");
        if (graphviz.agroot(graph) == graph) {
            sym.*.print = 1;
        }
    }
}

fn setAttributes(
    allocator: std.mem.Allocator,
    object: ?*anyopaque,
    attributes: vizjs_types.Attributes,
) void {
    var iterator = attributes.map.iterator();
    while (iterator.next()) |attr| {
        const name = allocator.dupeZ(u8, attr.key_ptr.*) catch @panic(
            "cannot dupeZ in setAttributes",
        );
        _ = if (attr.value_ptr.*) |value| switch (value) {
            .text => |val| graphviz.agsafeset_text(object, name, @ptrCast(val.ptr), ""),
            .html => |val| graphviz.agsafeset_html(object, name, @ptrCast(val.ptr), ""),
        } else graphviz.agsafeset_text(object, name, "", "");
    }
}

fn edgePortToString(allocator: std.mem.Allocator, endpoint_json: vizjs_types.EdgeEndpoint, graph_json: vizjs_types.Graph) ?[:0]const u8 {
    const node = graph_json.allNodes[endpoint_json.node];
    const maybePort = node.ports[endpoint_json.port];
    const maybeCompass = endpoint_json.compass;
    if (maybeCompass) |compass| {
        return std.fmt.allocPrintSentinel(allocator, "{s}:{s}", .{ maybePort orelse "", compass }, 0) catch @panic(
            "cannot allocPrintSentinel in edgePortToString",
        );
    }
    return maybePort;
}
