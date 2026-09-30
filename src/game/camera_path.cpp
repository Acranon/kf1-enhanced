#include <kf/platform/prelude.h>
#include <kf/game/player.h>
#include <kf/lib/map.h>

#include <span>

void camera_path_compute_segment(KfCameraPathState *path)
{
    const KfCameraPathPoint *point = &path->points[path->point_index];
    path->point_index++;
    camera_path_prepare_segment(path, point);
}

void camera_path_begin(KfCameraPathState *path, std::span<const KfCameraPathPoint> points)
{
    path->points = points;
    path->position = player_state.camera_position;
    path->rotation = player_state.camera_rotation;
    path->point_index = 0;
    camera_path_publish_fixed(path);
    camera_path_compute_segment(path);
}

void camera_path_step(KfCameraPathState *path, s32 y_offset)
{
    if (path->frames_remaining == KF_CAMERA_PATH_FINISHED) {
        return;
    }
    path->frames_remaining--;
    if (path->frames_remaining == KF_CAMERA_PATH_FINISHED) {
        camera_path_compute_segment(path);
        if (path->frames_remaining == KF_CAMERA_PATH_FINISHED) {
            return;
        }
    }
    camera_path_advance_pose(path, y_offset);
}
