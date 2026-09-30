#include <kf/platform/prelude.h>
#include <kf/cutscene/camera_path.h>
#include <kf/cutscene/opening_helpers.h>

#include <span>

KfCameraPathState opening_camera_path_state;

void opening_camera_path_compute_segment(void)
{
    auto &path = opening_camera_path_state;
    path.point_index++;
    const KfCameraPathPoint *point = &path.points[path.point_index];
    camera_path_prepare_segment(&path, point);
}

void opening_camera_path_begin(std::span<const KfCameraPathPoint> points)
{
    auto &path = opening_camera_path_state;
    path.points = points;
    path.position = points[0].position;
    path.rotation = points[0].rotation;
    path.point_index = 0;
    camera_path_publish_fixed(&path);
    opening_camera_path_compute_segment();
}

void opening_camera_path_step(s32 y_offset)
{
    auto &path = opening_camera_path_state;
    if (path.frames_remaining == KF_CAMERA_PATH_FINISHED) {
        return;
    }
    path.frames_remaining--;
    if (path.frames_remaining == KF_CAMERA_PATH_FINISHED) {
        opening_camera_path_compute_segment();
        if (path.frames_remaining == KF_CAMERA_PATH_FINISHED) {
            return;
        }
    }
    camera_path_advance_pose(&path, y_offset);
}


void camera_path_reset_module_state(void)
{
    kf::restore_initial_value<opening_camera_path_state>();
}
