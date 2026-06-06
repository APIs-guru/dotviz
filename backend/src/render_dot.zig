const vizjs_types = @import("vizjs_types.zig");
const graphviz = vizjs_types.graphviz;

pub fn renderDot(graph: ?*graphviz.Agraph_t, request: vizjs_types.RenderRequest) ?[:0]const u8 {
    graphviz.my_attach_attrs_and_arrows(graph);

    // reset node state
    var n: ?*graphviz.Agnode_t = graphviz.agfstnode(graph);
    while (n != null) : (n = graphviz.agnxtnode(graph, n)) {
        graphviz.nodeInfoPtr(n).*.state = 0;
    }

    var output = graphviz.my_agwrite(graph, request.dotOutputMaxLineLength);
    return @ptrCast(output.data[0..output.data_position]);
}
