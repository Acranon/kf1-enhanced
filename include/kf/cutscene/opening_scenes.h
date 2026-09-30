#ifndef KF_OPEN_OPENING_SCENES_H
#define KF_OPEN_OPENING_SCENES_H

#include <kf/cutscene/audio.h>
#include <kf/lib/map.h>
#include <kf/lib/render_types.h>

#include <array>

enum { KF_OPENING_SCENE3_PANEL_COUNT = 2 };

enum {
    KF_OPENING_SCENE0_CAMERA_POINT_COUNT = 17,
    KF_OPENING_SCENE3_CAMERA_POINT_COUNT = 3,
    KF_OPENING_ENDING_CAMERA_POINT_COUNT = 9
};

enum class KfOpeningCylinderTransitionMode : s16 {
    KF_OPENING_CYLINDER_TRANSITION_GROW = 0,
    KF_OPENING_CYLINDER_TRANSITION_REMOVE = 1,
    KF_OPENING_CYLINDER_TRANSITION_SHRINK = 2,
    KF_OPENING_CYLINDER_TRANSITION_CREATE = 3
}; using enum KfOpeningCylinderTransitionMode;

extern std::array<KfCameraPathPoint, KF_OPENING_SCENE0_CAMERA_POINT_COUNT> opening_scene0_camera_path;
extern std::array<KfCameraPathPoint, KF_OPENING_SCENE3_CAMERA_POINT_COUNT> opening_scene3_camera_path;
extern std::array<KfCameraPathPoint, KF_OPENING_ENDING_CAMERA_POINT_COUNT> opening_ending_camera_path;
extern SoundRef opening_scene0_sound;
extern std::array<KfScreenRect, KF_OPENING_SCENE3_PANEL_COUNT> opening_scene3_panels;
extern std::array<u8, KF_QUAD_TEX_DESCRIPTOR_BYTES> opening_scene3_panel_uv;
extern CVECTOR opening_scene3_panel_color;

extern void opening_scene0_run(void);
extern void opening_scene1_draw_fade(u8 shade);
extern void opening_scene1_run(void);
extern void opening_cylinder_transition(KfOpeningCylinderTransitionMode transition_mode, const VECTOR *position);
extern void opening_scene3_run(void);
extern void opening_ending_scene_run(void);
extern void opening_ending_scroll_run(void);

#endif // KF_OPEN_OPENING_SCENES_H
