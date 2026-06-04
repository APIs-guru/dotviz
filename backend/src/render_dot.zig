const vizjs_types = @import("vizjs_types.zig");
const graphviz = vizjs_types.graphviz;

pub fn renderDot(graph: ?*graphviz.Agraph_t, request: vizjs_types.RenderRequest) ?[:0]const u8 {
    const output = graphviz.render_dot(graph, request.dotOutputMaxLineLength);
    return @ptrCast(output.data[0..output.data_position]);
}
