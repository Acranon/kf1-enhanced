#include <kf/platform/prelude.h>
#include <kf/cutscene/render.h>
#include <kf/cutscene/scene0.h>

void opening_scene0_render_frame(
    const VECTOR *position_or_null, const SVECTOR *rotation_or_null)
{
    render_set_view_transform(open_graphics_runtime.render_state, position_or_null, rotation_or_null);
    display_begin_frame(open_graphics_runtime.display_state);
    opening_render_map_cells();
    opening_render_entities_and_items();
    display_present_frame(open_graphics_runtime.display_state);
}
