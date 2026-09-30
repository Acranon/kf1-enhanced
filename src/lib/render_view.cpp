#include <kf/platform/prelude.h>
#include <kf/lib/graphics.h>

void render_set_view_transform(KfRenderState &view,
    const VECTOR *position_or_null, const SVECTOR *rotation_or_null)
{
    if (position_or_null != NULL) {
        view.view_position = *position_or_null;
        view.view_cell.x = view.view_position.vx / KF_MAP_TILE_SIZE;
        view.view_cell.z = view.view_position.vz / KF_MAP_TILE_SIZE;
    }
    if (rotation_or_null != NULL) {
        view.view_rotation = *rotation_or_null;
    }
    kf::matrix_set_rotation_xyz(view.view_rotation, view.view_matrix);
    const SVECTOR angles{view.view_rotation.vx, 0, 0};
    kf::matrix_set_rotation_xyz(angles, view.pitch_matrix);
}
