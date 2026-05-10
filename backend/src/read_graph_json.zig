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

        const tail_node = allNodes[tail.port.node];
        const head_node = allNodes[head.port.node];
        const edge = agedge(allocator, graph, tail_node, head_node, edge_json.key);
        if (edgePortToString(allocator, tail)) |tailport| {
            agsafeset_text(allocator, edge, "tailport", tailport);
        }
        if (edgePortToString(allocator, head)) |headport| {
            agsafeset_text(allocator, edge, "headport", headport);
        }
        setAttributes(allocator, edge, edge_json.attributes);
        allEdges[i] = edge;
    }

    for (graph_json.subgraphs) |subgraph_json| {
        const subgraph = agsubg(allocator, graph, subgraph_json.name);
        readSubgraphJSON(allocator, subgraph, subgraph_json, allNodes, allEdges);
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
    subgraph: anytype,
    subgraph_json: vizjs_types.Subgraph,
    allNodes: []?*graphviz.Agnode_t,
    allEdges: []?*graphviz.Agedge_t,
) void {
    setDefaultAttributes(allocator, subgraph, subgraph_json.graphAttributes, graphviz.AGRAPH);
    setDefaultAttributes(allocator, subgraph, subgraph_json.nodeAttributes, graphviz.AGNODE);
    setDefaultAttributes(allocator, subgraph, subgraph_json.edgeAttributes, graphviz.AGEDGE);

    for (subgraph_json.memberNodes) |node| {
        _ = graphviz.agsubnode(subgraph, allNodes[node], graphviz.true);
    }

    for (subgraph_json.memberEdges) |edge| {
        _ = graphviz.agsubedge(subgraph, allEdges[edge], graphviz.true);
    }

    for (subgraph_json.subgraphs) |child_subgraph_json| {
        const child_subgraph = agsubg(allocator, subgraph, child_subgraph_json.name);
        readSubgraphJSON(allocator, child_subgraph, child_subgraph_json, allNodes, allEdges);
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

        const sym = brk: switch (attr.value_ptr.*) {
            .text => |val| {
                break :brk graphviz.agattr_text(graph, kind, name, @ptrCast(val.ptr));
            },
            .html => |val| {
                break :brk graphviz.agattr_html(graph, kind, name, @ptrCast(val.ptr));
            },
        };
        if (graphviz.agroot(graph) == graph) {
            graphviz.wrapped_sym_set_print(sym);
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
        switch (attr.value_ptr.*) {
            .text => |val| {
                _ = graphviz.agsafeset_text(object, name, @ptrCast(val.ptr), "");
            },
            .html => |val| {
                _ = graphviz.agsafeset_html(object, name, @ptrCast(val.ptr), "");
            },
        }
    }
}

fn edgePortToString(
    allocator: std.mem.Allocator,
    endpoint_json: vizjs_types.EdgeEndpoint,
) ?[:0]const u8 {
    const maybePort = endpoint_json.port.name;
    const maybeCompass = endpoint_json.compass;
    if (maybePort) |port| {
        if (maybeCompass) |compass| {
            return std.fmt.allocPrintSentinel(allocator, "{s}:{s}", .{ port, compass }, 0) catch @panic(
                "cannot allocPrintSentinel in edgePortToString",
            );
        } else {
            return port;
        }
    } else {
        return maybeCompass orelse null;
    }
}
