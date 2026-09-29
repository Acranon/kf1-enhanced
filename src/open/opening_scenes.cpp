#include <kf/lib/null.h>

#include <kf/open/audio.h>
#include <kf/lib/math.h>
#include <kf/lib/render_face.h>
#include <kf/open/camera_path.h>
#include <kf/open/opening_helpers.h>
#include <kf/open/opening_render.h>
#include <kf/open/opening_scenes.h>
#include <kf/open/render.h>
#include <kf/open/resources.h>
#include <kf/open/scene0.h>

enum {
    ENDING_SCROLL_BLEND_LAST = 0xfff,
    ENDING_SCROLL_BLEND_STEP = 0x40,
    SCENE0_UPDATES_PER_SECOND = 22,
    // Scene 3 (story panels over the flythrough) has the same per-update camera
    // and 1-pixel text steps as scene 0; presenting once per 60 Hz tick ran it
    // three times too fast. Reuse scene 0's retail-calibrated cadence.
    SCENE3_UPDATES_PER_SECOND = SCENE0_UPDATES_PER_SECOND,
    SCENE0_ROTATION_START_POINT = 8,
    SCENE0_FADE_OUT_START_POINT = 15,
    SCENE0_YAW_STEP = 40,
    SCENE0_SOUND_VOLUME = 100,
    OPENING_COLOR_FADE_STEP = 0x100,
    SCENE1_LEFT_TPAGE_X = 0x140,
    SCENE1_RIGHT_TPAGE_X = 0x1c0,
    SCENE1_PANEL_WIDTH = 192,
    SCENE1_RIGHT_PANEL_X = 128,
    SCENE1_SHADE_STEP = 4,
    SCENE1_SEQUENCE_STOP_FRAME = 600,
    SCENE1_HOLD_FRAMES = 1000,
    CYLINDER_TRANSITION_FIRST_ENTITY_SLOT = 24
};

enum {
    SCENE3_YAW_STEP = 0x10,
    SCENE_CAMERA_WAVE_SHIFT = 7,
    SCENE_CAMERA_WAVE_ANGLE_STEP = 100,
    PANEL_TPAGE_FIRST_X = 0x1c0,
    PANEL_TPAGE_X_STRIDE = 0x40,
    PANEL_CLUT_FIRST_Y = 0x1ed,
    PANEL_CLIP_Y_BIAS = 255,
    PANEL_CLIP_SPAN = PANEL_CLIP_Y_BIAS + KF_DISPLAY_HEIGHT,
    PANEL_OT_DEPTH = 4,
    ENDING_ROTATION_START_POINT = 4,
    ENDING_MAX_BRIGHTNESS = 255,
    ENDING_BRIGHTEN_STEP = 4,
    ENDING_FADE_DARKEN_STEP = 0x10,
    ENDING_PANEL_COUNT = 9,
    ENDING_PANEL_STOP_Y = 50,
    ENDING_BACKGROUND_OT_DEPTH = 0x2f65,
    ENDING_TMD_PROJECTION_SHIFT = 2,
    ENDING_MODEL_START_Y_OFFSET = 1500,
    ENDING_MODEL_FINAL_Y = -8000,
    ENDING_MODEL_Y_STEP = 3,
    ENDING_LIGHT_MIDPOINT_STEP = 0x40,
    ENDING_LIGHT_GREEN_STEP = 3,
    ENDING_BACKGROUND_BLEND_STEP = 3,
    ENDING_SEQUENCE_DELAY_START = 20,
    ENDING_SEQUENCE_VOLUME_DIVISOR = 3,
    ENDING_SCROLL_CAMERA_POINT_COUNT = 3
};

enum class KfEndingLightingPhase : s16 {
    ENDING_LIGHT_TO_MIDPOINT = 0,
    ENDING_LIGHT_TO_GREEN = 1,
    ENDING_LIGHT_FINISHED = 2
}; using enum KfEndingLightingPhase;

enum class KfEndingSequencePhase : s16 {
    ENDING_SEQUENCE_WAIT_SCROLL = 0,
    ENDING_SEQUENCE_DELAY = 1,
    ENDING_SEQUENCE_FADE = 2,
    ENDING_SEQUENCE_REPLACED = 3
}; using enum KfEndingSequencePhase;

enum class KfEndingScrollState : s16 {
    ENDING_SCROLL_WAIT_DISK = 0,
    ENDING_SCROLL_ACTIVE = 1
}; using enum KfEndingScrollState;

enum class KfEndingScrollTick : s16 {
    ENDING_SCROLL_TICK_WRAP = -1,
    ENDING_SCROLL_TICK_STARFIELD_AND_PANELS = 0,
    ENDING_SCROLL_TICK_HOLD_AFTER_PANELS = 1,
    ENDING_SCROLL_TICK_PANELS = 2,
    ENDING_SCROLL_TICK_HOLD_AFTER_STARFIELD = 3
}; using enum KfEndingScrollTick;

KfCameraPathPoint opening_scene0_camera_path[KF_OPENING_SCENE0_CAMERA_POINT_COUNT] = {
    {{101000, -13000, 101000, 0}, {256, 0, 200, 0}, 0, 0},
    {{101000, -12000, 117000, 0}, {0, 128, -200, 0}, 400, 0},
    {{101000, -11700, 127000, 0}, {0, 1024, 100, 0}, 400, 0},
    {{95000, -11500, 127000, 0}, {0, 0, 300, 0}, 400, 0},
    {{95000, -11200, 151000, 0}, {-256, 0, 0, 0}, 400, 0},
    {{95000, -11800, 165000, 0}, {0, -800, 0, 0}, 400, 0},
    {{97000, -12200, 167000, 0}, {0, -1024, -200, 0}, 400, 0},
    {{113000, -11900, 167000, 0}, {0, -1024, -100, 0}, 400, 0},
    {{127000, -11800, 167000, 0}, {0, -1024, 0, 0}, 400, 0},
    {{131000, -12200, 167000, 0}, {100, -800, -100, 0}, 400, 0},
    {{139000, -12000, 171000, 0}, {100, -512, -100, 0}, 400, 0},
    {{149000, -11900, 181000, 0}, {100, -750, -100, 0}, 400, 0},
    {{161000, -11900, 189000, 0}, {100, -1024, -100, 0}, 400, 0},
    {{163000, -11900, 187000, 0}, {100, -1536, -100, 0}, 300, 0},
    {{163000, -11900, 163000, 0}, {100, -512, -100, 0}, 200, 0},
    {{163000, -12100, 161000, 0}, {100, -256, -100, 0}, 100, 0},
    {{KF_CAMERA_PATH_END_X, -1, -1, 0}, {-1, -1, -1, 0}, -1, 0},
};

KfCameraPathPoint opening_scene3_camera_path[KF_OPENING_SCENE3_CAMERA_POINT_COUNT] = {
    {{101000, -11500, 115200, 0}, {0, 0x800, 0, 0}, 0, 0},
    {{101000, -11500, 101000, 0}, {0, 0x800, 0, 0}, 0x10, 0},
    {{KF_CAMERA_PATH_END_X, -1, -1, 0}, {-1, -1, -1, 0}, -1, 0},
};

KfCameraPathPoint opening_ending_camera_path[KF_OPENING_ENDING_CAMERA_POINT_COUNT] = {
    {{101000, -11500, 101000, 0}, {0, 0, 0, 0}, 0, 0},
    {{101000, -11500, 108600, 0}, {0, 0, 0, 0}, 35, 0},
    {{101000, -11500, 108800, 0}, {0, 0, 0, 0}, 20, 0},
    {{101000, -11500, 109000, 0}, {0, 0, 0, 0}, 10, 0},
    {{101000, -11500, 109050, 0}, {0, 0, 0, 0}, 1, 0},
    {{101000, -11500, 109400, 0}, {0, 0, 0, 0}, 30, 0},
    {{101000, -11500, 109800, 0}, {0, 0, 0, 0}, 60, 0},
    {{101000, -11500, 117000, 0}, {0, 0, 0, 0}, 100, 0},
    {{KF_CAMERA_PATH_END_X, -1, -1, 0}, {-1, -1, -1, 0}, -1, 0},
};

static KfCameraPathPoint opening_ending_scroll_camera_path[ENDING_SCROLL_CAMERA_POINT_COUNT] = {
    {{101000, -8500, 89000, 0}, {0, 0, 0, 0}, 0, 0},
    {{101000, -12000, 89000, 0}, {0, 0, 0, 0}, 2, 0},
    {{KF_CAMERA_PATH_END_X, -1, -1, 0}, {-1, -1, -1, 0}, -1, 0},
};

SoundRef opening_scene0_sound = {9, 0, 0x43};

KfScreenRect opening_scene3_overlay_rects[KF_OPENING_SCENE3_OVERLAY_COUNT] = {
    {32, 256, 255, 254},
    {32, 512, 255, 254},
};

static KfScreenRect opening_ending_scroll_panels[ENDING_PANEL_COUNT] = {
    {32, 256, 255, 254},
    {32, 512, 255, 254},
    {32, 768, 255, 254},
    {32, 1024, 255, 254},
    {32, 1261, 255, 254},
    {32, 1490, 255, 254},
    {32, 1746, 255, 254},
    {32, 2002, 255, 254},
    {32, 2258, 255, 254},
};

u8 opening_scene3_overlay_uv[KF_QUAD_TEX_DESCRIPTOR_BYTES] = {0, 0, 0, 0, 255, 0, 254, 0};

CVECTOR opening_scene3_overlay_color = {200, 200, 200, 0};

static KfScreenRect opening_ending_scroll_backgrounds[2] = {
    {0, 0, KF_DISPLAY_WIDTH, 160}, {0, 160, KF_DISPLAY_WIDTH, 160},
};

static CVECTOR opening_ending_scroll_top_start = {32, 0, 32, 0};

static CVECTOR opening_ending_scroll_bottom_start = {255, 0, 0, 0};

static CVECTOR opening_ending_scroll_top_end = {0, 0, 32, 0};

static CVECTOR opening_ending_scroll_bottom_end = {0, 0, 32, 0};

static CVECTOR opening_ending_scroll_panel_color = {200, 200, 200, 0};

static CVECTOR opening_ending_scroll_background_color = {0, 0, 0, 0};

static u8 opening_ending_scroll_uv[KF_QUAD_TEX_DESCRIPTOR_BYTES] = {0, 0, 1, 0, 255, 0, 254, 0};

void opening_scene0_run(void)
{
    KfOpeningEntity *entity_11;
    KfOpeningEntity *entity_12;
    s16 blend;
    s16 next_blend;

    blend = 0;
    opening_resources_load_scene0();
    entity_11 = opening_entity_find_by_object_id(
        opening_entity_state.entities, KF_OPENING_SCENE0_DECREASING_YAW_MODEL);
    entity_12 = opening_entity_find_by_object_id(
        opening_entity_state.entities, KF_OPENING_SCENE0_INCREASING_YAW_MODEL);
    entity_12->rotation.y = KF_ANGLE_THREE_QUARTER_TURN;
    entity_11->rotation.y = KF_ANGLE_THREE_QUARTER_TURN;
    opening_camera_path_begin(opening_scene0_camera_path);
    // Stable port cadence calibrated from the measured retail flythrough.
    auto pacer = kf::host_begin_update_pacer(SCENE0_UPDATES_PER_SECOND);

    for (;;) {
        opening_poll_input();
        opening_camera_path_step(0);
        if (opening_camera_path_state.point_index >= SCENE0_ROTATION_START_POINT) {
            if (entity_11->rotation.y == KF_ANGLE_THREE_QUARTER_TURN) {
                sound_ref_play(audio_playback(), &opening_scene0_sound, SCENE0_SOUND_VOLUME);
            }
            entity_11->rotation.y -= SCENE0_YAW_STEP;
            entity_12->rotation.y += SCENE0_YAW_STEP;
        }

        if (opening_camera_path_state.point_index >= SCENE0_FADE_OUT_START_POINT ||
            opening_input_action != KF_OPENING_INPUT_NONE) {
            if (blend < 0) {
                break;
            }
            next_blend = blend - OPENING_COLOR_FADE_STEP;
            blend = next_blend;
            lighting_set_color_matrix(open_graphics_runtime.render_state,
                &color_matrix_table[kf_enum_encode<s32>(KF_OPEN_COLOR_BLACK)],
                &color_matrix_table[kf_enum_encode<s32>(KF_OPEN_COLOR_DEFAULT)], next_blend);
        } else if (blend < KF_FIXED12_ONE) {
            next_blend = blend + OPENING_COLOR_FADE_STEP;
            blend = next_blend;
            lighting_set_color_matrix(open_graphics_runtime.render_state,
                &color_matrix_table[kf_enum_encode<s32>(KF_OPEN_COLOR_BLACK)],
                &color_matrix_table[kf_enum_encode<s32>(KF_OPEN_COLOR_DEFAULT)], next_blend);
        }

        audio_set_listener_transform(audio_state,
            &opening_camera_path_state.position,
            &opening_camera_path_state.rotation);
        opening_scene0_render_frame(
            &opening_camera_path_state.position,
            &opening_camera_path_state.rotation);
        kf::host_wait_update(pacer);
    }

    audio_stop_sequence(KF_AUDIO_STOP_FADE);
}

void opening_scene1_draw_fade(u8 shade)
{
    kf::DrawFace left {}, right {};
    const CVECTOR color = {shade, shade, shade, 0};

    display_begin_frame(open_graphics_runtime.display_state);
    left.material = {kf::SurfaceKind::Texture,
        {SCENE1_LEFT_TPAGE_X, KF_TEXTURE_LOWER_PAGE_Y, 0, 0, kf::TextureFormat::Direct16},
        kf::BlendMode::average};
    right.material = {kf::SurfaceKind::Texture,
        {SCENE1_RIGHT_TPAGE_X, KF_TEXTURE_LOWER_PAGE_Y, 0, 0, kf::TextureFormat::Direct16},
        kf::BlendMode::average};
    render_face_rectangle(&left, 0, 0, SCENE1_PANEL_WIDTH, KF_DISPLAY_HEIGHT);
    render_face_rectangle(&right, SCENE1_RIGHT_PANEL_X, 0,
        SCENE1_RIGHT_PANEL_X + SCENE1_PANEL_WIDTH, KF_DISPLAY_HEIGHT);
    render_face_uv_rectangle(&left, 0, 0, SCENE1_PANEL_WIDTH, KF_DISPLAY_HEIGHT);
    render_face_uv_rectangle(&right, 0, 0, SCENE1_PANEL_WIDTH, KF_DISPLAY_HEIGHT);
    render_face_submit(&left, &color, KfFaceShading::Flat, 0);
    render_face_submit(&right, &color, KfFaceShading::Flat, 0);
    display_present_frame(open_graphics_runtime.display_state);
}

void opening_scene1_run(void)
{
    s32 frame;
    s32 shade;

    opening_resources_load_scene1();
    shade = 0;
    do {
        opening_scene1_draw_fade((u8)shade);
        opening_poll_input();
        shade += SCENE1_SHADE_STEP;
    } while (shade < KF_TEXTURE_BASE_BRIGHTNESS + 1);

    frame = 0;
    do {
        if (frame == SCENE1_SEQUENCE_STOP_FRAME) {
            audio_stop_sequence(KF_AUDIO_STOP_IMMEDIATE);
        }
        kf::host_wait_frame();
        opening_poll_input();
        if (opening_input_action != KF_OPENING_INPUT_NONE) {
            break;
        }
        frame++;
    } while (frame < SCENE1_HOLD_FRAMES);

    audio_stop_sequence(KF_AUDIO_STOP_FADE);
    shade = KF_TEXTURE_BASE_BRIGHTNESS;
    do {
        opening_scene1_draw_fade((u8)shade);
        shade -= SCENE1_SHADE_STEP;
    } while (shade >= 0);
}

static void opening_remove_transition_cylinders(void)
{
    auto *entity = &opening_entity_state.entities[CYLINDER_TRANSITION_FIRST_ENTITY_SLOT];
    for (s16 i = 0; i < KF_CYLINDER_TRANSITION_COUNT; ++i, ++entity)
        entity->object_id = KF_OPENING_ENTITY_FREE;
}

void opening_cylinder_transition(KfOpeningCylinderTransitionMode transition_mode, const VECTOR *position)
{
    struct KfOpeningTransformSnapshot {
        VECTOR position;
        SVECTOR rotation;
    } transform_snapshot;
    KfOpeningEntity *entity;
    s16 entity_index;
    s16 frame;
    s16 initial_scale_y;
    s16 scale_step;

    switch (transition_mode) {
    case KF_OPENING_CYLINDER_TRANSITION_GROW:
        initial_scale_y = 0;
        scale_step = KF_CYLINDER_TRANSITION_SCALE_STEP;
        break;
    case KF_OPENING_CYLINDER_TRANSITION_REMOVE:
        opening_remove_transition_cylinders();
        return;
    case KF_OPENING_CYLINDER_TRANSITION_SHRINK:
    case KF_OPENING_CYLINDER_TRANSITION_CREATE:
        initial_scale_y = KF_CYLINDER_TRANSITION_TALL_SCALE;
        scale_step = -KF_CYLINDER_TRANSITION_SCALE_STEP;
        break;
    }

    entity = &opening_entity_state.entities[CYLINDER_TRANSITION_FIRST_ENTITY_SLOT];
    entity_index = KF_CYLINDER_TRANSITION_COUNT - 1;

    transform_snapshot.position = *position;
    do {
        entity->object_id = KF_OPENING_TRANSITION_CYLINDER;
        entity->position = *position;
        entity->rotation.z = 0;
        entity->rotation.y = 0;
        entity->rotation.x = 0;
        entity->scale = VECTOR{KF_FIXED12_ONE, initial_scale_y, KF_FIXED12_ONE}.narrowed();
        entity++;
        entity_index--;
    } while (entity_index != -1);

    if (transition_mode == KF_OPENING_CYLINDER_TRANSITION_CREATE) {
        return;
    }

    frame = 0;
    do {
        entity = &opening_entity_state.entities[CYLINDER_TRANSITION_FIRST_ENTITY_SLOT];
        entity_index = 0;
        do {
            if ((entity_index << KF_CYLINDER_TRANSITION_STAGGER_SHIFT) < frame) {
                u16 scale_y = entity->scale.vy;

                if (scale_y < KF_CYLINDER_TRANSITION_TALL_SCALE + 1) {
                    entity->scale.vy = scale_step + scale_y;
                }
            }
            entity_index++;
            entity->rotation.y =
                (entity->rotation.y + KF_CYLINDER_TRANSITION_YAW_STEP) & KF_ANGLE_WRAP_MASK;
            entity++;
        } while (entity_index < KF_CYLINDER_TRANSITION_COUNT);
        opening_render_frame(NULL, NULL);
        kf::host_wait_frame();
        frame++;
    } while (frame < KF_CYLINDER_TRANSITION_FRAMES);

    if (transition_mode == KF_OPENING_CYLINDER_TRANSITION_GROW) {
        return;
    }

    opening_remove_transition_cylinders();
}

void opening_scene3_run(void)
{
    KfOpeningEntity *entity_13;
    KfOpeningEntity *entity_14;
    VECTOR transition_position;
    kf::FaceMaterial materials[KF_OPENING_SCENE3_OVERLAY_COUNT];
    KfScreenRect *overlay_rect;
    s16 *overlay_y;
    s16 blend;
    s16 overlay_index;
    s32 wave_angle;

    wave_angle = 0;
    opening_resources_load_scene3();
    for (unsigned i = 0; i < KF_OPENING_SCENE3_OVERLAY_COUNT; ++i)
        materials[i] = {kf::SurfaceKind::Texture,
            {static_cast<u16>(PANEL_TPAGE_FIRST_X + i * PANEL_TPAGE_X_STRIDE),
             KF_TEXTURE_LOWER_PAGE_Y, 0, static_cast<u16>(PANEL_CLUT_FIRST_Y + i),
             kf::TextureFormat::Indexed4}, kf::BlendMode::average};

    entity_13 = opening_entity_find_by_object_id(
        opening_entity_state.entities, KF_OPENING_SCENE3_INCREASING_YAW_MODEL);
    entity_14 = opening_entity_find_by_object_id(
        opening_entity_state.entities, KF_OPENING_SCENE3_DECREASING_YAW_MODEL);
    entity_14->rotation.y = 0;
    entity_13->rotation.y = 0;
    opening_camera_path_begin(opening_scene3_camera_path);
    auto pacer = kf::host_begin_update_pacer(SCENE3_UPDATES_PER_SECOND);

    blend = 0;
    for (;;) {
        lighting_set_color_matrix(open_graphics_runtime.render_state,
            &color_matrix_table[kf_enum_encode<s32>(KF_OPEN_COLOR_BLACK)],
            &color_matrix_table[kf_enum_encode<s32>(KF_OPEN_COLOR_DEFAULT)], blend);
        opening_render_frame(
            &opening_camera_path_state.position,
            &opening_camera_path_state.rotation);
        opening_poll_input();
        kf::host_wait_update(pacer);
        blend += OPENING_COLOR_FADE_STEP;
        if (blend >= KF_FIXED12_ONE + 1) {
            break;
        }
    }

    while (entity_13->rotation.y < KF_ANGLE_QUARTER_TURN) {
        entity_13->rotation.y += SCENE3_YAW_STEP;
        entity_14->rotation.y -= SCENE3_YAW_STEP;
        audio_set_listener_transform(audio_state,
            &opening_camera_path_state.position,
            &opening_camera_path_state.rotation);
        opening_render_frame(
            &opening_camera_path_state.position,
            &opening_camera_path_state.rotation);
        opening_poll_input();
        kf::host_wait_update(pacer);
        if (opening_input_action != KF_OPENING_INPUT_NONE) {
            break;
        }
    }

    do {
        opening_camera_path_step(kf::angle_sine(wave_angle) >> SCENE_CAMERA_WAVE_SHIFT);
        if (opening_camera_path_state.frames_remaining == KF_CAMERA_PATH_FINISHED) {
            break;
        }
        wave_angle = (wave_angle + SCENE_CAMERA_WAVE_ANGLE_STEP) & KF_ANGLE_WRAP_MASK;
        render_set_view_transform(open_graphics_runtime.render_state,
            &opening_camera_path_state.position,
            &opening_camera_path_state.rotation);
        display_begin_frame(open_graphics_runtime.display_state);
        opening_render_entities();
        overlay_index = 0;
        overlay_rect = opening_scene3_overlay_rects;
        overlay_y = &overlay_rect->y;
        do {
            if ((u16)(--*overlay_y + PANEL_CLIP_Y_BIAS) < PANEL_CLIP_SPAN) {
                sprite_add_ft4(
                    overlay_rect,
                    opening_scene3_overlay_uv,
                    materials[overlay_index],
                    &opening_scene3_overlay_color,
                    PANEL_OT_DEPTH);
            }
            overlay_index++;
            overlay_y += sizeof(*overlay_rect) / sizeof(*overlay_y);
            overlay_rect++;
        } while (overlay_index < KF_OPENING_SCENE3_OVERLAY_COUNT);
        display_present_frame(open_graphics_runtime.display_state);
        opening_poll_input();
        kf::host_wait_update(pacer);
    } while (opening_input_action == KF_OPENING_INPUT_NONE);

    transition_position = {
        opening_camera_path_state.position.vx,
        KF_OPENING_SCENE_BASE_Y,
        opening_camera_path_state.position.vz};
    if (opening_input_action == KF_OPENING_INPUT_NONE) {
        opening_cylinder_transition(KF_OPENING_CYLINDER_TRANSITION_GROW, &transition_position);
    }

    blend = 0;
    do {
        lighting_set_color_matrix(open_graphics_runtime.render_state,
            &color_matrix_table[kf_enum_encode<s32>(KF_OPEN_COLOR_DEFAULT)],
            &color_matrix_table[kf_enum_encode<s32>(KF_OPEN_COLOR_BLACK)], blend);
        opening_render_frame(
            &opening_camera_path_state.position,
            &opening_camera_path_state.rotation);
        blend += OPENING_COLOR_FADE_STEP;
    } while (blend < KF_FIXED12_ONE + 1);

    if (opening_input_action == KF_OPENING_INPUT_NONE) {
        opening_cylinder_transition(KF_OPENING_CYLINDER_TRANSITION_REMOVE, &transition_position);
    }
}

void opening_ending_scene_run(void)
{
    KfOpeningEntity *entity_13;
    KfOpeningEntity *entity_14;
    VECTOR transition_position;
    s16 brightness;
    s16 blend;
    s32 wave_angle;

    wave_angle = 0;
    opening_resources_load_ending();
    entity_13 = opening_entity_find_by_object_id(
        opening_entity_state.entities, KF_OPENING_SCENE3_INCREASING_YAW_MODEL);
    entity_14 = opening_entity_find_by_object_id(
        opening_entity_state.entities, KF_OPENING_SCENE3_DECREASING_YAW_MODEL);
    entity_14->rotation.y = 0;
    entity_13->rotation.y = 0;
    opening_camera_path_begin(opening_ending_camera_path);

    transition_position = {opening_camera_path_state.position.vx, KF_OPENING_SCENE_BASE_Y,
        opening_camera_path_state.position.vz};
    blend = 0;
    opening_cylinder_transition(KF_OPENING_CYLINDER_TRANSITION_CREATE, &transition_position);

    do {
        lighting_set_color_matrix(open_graphics_runtime.render_state,
            &color_matrix_table[kf_enum_encode<s32>(KF_OPEN_COLOR_BLACK)],
            &color_matrix_table[kf_enum_encode<s32>(KF_OPEN_COLOR_DEFAULT)], blend);
        opening_render_frame(
            &opening_camera_path_state.position,
            &opening_camera_path_state.rotation);
        blend += OPENING_COLOR_FADE_STEP;
    } while (blend < KF_FIXED12_ONE + 1);

    opening_cylinder_transition(KF_OPENING_CYLINDER_TRANSITION_SHRINK, &transition_position);
    brightness = 0;
    blend = 0;
    for (;;) {
        opening_camera_path_step(kf::angle_sine(wave_angle) >> SCENE_CAMERA_WAVE_SHIFT);
        if (opening_camera_path_state.frames_remaining == KF_CAMERA_PATH_FINISHED) {
            break;
        }
        if (opening_camera_path_state.point_index >= ENDING_ROTATION_START_POINT) {
            if (entity_13->rotation.y < KF_ANGLE_QUARTER_TURN) {
                entity_13->rotation.y += SCENE3_YAW_STEP;
                entity_14->rotation.y -= SCENE3_YAW_STEP;
                audio_set_listener_transform(audio_state,
                    &opening_camera_path_state.position,
                    &opening_camera_path_state.rotation);
                opening_render_frame(
                    &opening_camera_path_state.position,
                    &opening_camera_path_state.rotation);
                open_graphics_runtime.render_state.lighting.ambient = {brightness, brightness, brightness};
                open_graphics_runtime.render_state.lighting.fog = {brightness, brightness, brightness};
                open_graphics_runtime.display_state.frame_style.red =
                    open_graphics_runtime.display_state.frame_style.green =
                    open_graphics_runtime.display_state.frame_style.blue =
                    static_cast<u8>(brightness) / kf::color8_scale;
                if (brightness >= ENDING_MAX_BRIGHTNESS) {
                    brightness = ENDING_MAX_BRIGHTNESS;
                } else {
                    brightness += ENDING_BRIGHTEN_STEP;
                }
                if (blend < ENDING_SCROLL_BLEND_LAST) {
                    blend += ENDING_SCROLL_BLEND_STEP;
                } else {
                    blend = ENDING_SCROLL_BLEND_LAST;
                }
            }
        } else {
            wave_angle = (wave_angle + SCENE_CAMERA_WAVE_ANGLE_STEP) & KF_ANGLE_WRAP_MASK;
        }
        opening_render_frame(
            &opening_camera_path_state.position,
            &opening_camera_path_state.rotation);
    }

    brightness = ENDING_MAX_BRIGHTNESS;
    blend = 0;
    do {
        if (brightness < 0) {
            brightness = 0;
        }
        open_graphics_runtime.render_state.lighting.ambient = {brightness, brightness, brightness};
        open_graphics_runtime.render_state.lighting.fog = {brightness, brightness, brightness};
        open_graphics_runtime.display_state.frame_style.red =
            open_graphics_runtime.display_state.frame_style.green =
            open_graphics_runtime.display_state.frame_style.blue =
            static_cast<u8>(brightness) / kf::color8_scale;
        lighting_set_color_matrix(open_graphics_runtime.render_state,
            &color_matrix_table[kf_enum_encode<s32>(KF_OPEN_COLOR_DEFAULT)],
            &color_matrix_table[kf_enum_encode<s32>(KF_OPEN_COLOR_BLACK)], blend);
        opening_render_frame(
            &opening_camera_path_state.position,
            &opening_camera_path_state.rotation);
        blend += OPENING_COLOR_FADE_STEP;
        brightness -= ENDING_FADE_DARKEN_STEP;
    } while (blend < KF_FIXED12_ONE + 1);
}

void opening_ending_scroll_run(void)
{
    VECTOR transition_position;
    MATRIX light_matrix = {
        {{0, 0, -4095}, {4095, 0, -2048}, {-4095, 0, -2048}}, {0, 0, 0}
    };
    kf::FaceMaterial materials[ENDING_PANEL_COUNT];
    CVECTOR top_color;
    CVECTOR bottom_color;
    KfOpeningEntity *entity_26;
    KfOpeningEntity *entity_27;
    KfEndingLightingPhase lighting_phase;
    KfEndingScrollState scroll_state;
    KfEndingSequencePhase sequence_phase;
    s32 sequence_delay;
    s32 sequence_volume;
    s16 lighting_blend;
    s16 background_blend;
    KfEndingScrollTick scroll_tick;
    s16 panel_index;
    KfScreenRect *panel;

    open_graphics_runtime.render_state.light_matrix = light_matrix;
    lighting_blend = 0;
    opening_resources_load_ending_entities();
    for (unsigned i = 0; i < ENDING_PANEL_COUNT; ++i)
        materials[i] = {kf::SurfaceKind::Texture,
            {static_cast<u16>(PANEL_TPAGE_FIRST_X + i * PANEL_TPAGE_X_STRIDE),
             KF_TEXTURE_LOWER_PAGE_Y, 0, static_cast<u16>(PANEL_CLUT_FIRST_Y + i),
             kf::TextureFormat::Indexed4}, kf::BlendMode::average};

    entity_26 = opening_entity_find_by_object_id(
        opening_entity_state.entities, KF_OPENING_ENDING_ORANGE_DISK);
    entity_27 = opening_entity_find_by_object_id(
        opening_entity_state.entities, KF_OPENING_ENDING_STARFIELD);
    entity_26->position.vy -= ENDING_MODEL_START_Y_OFFSET;
    entity_27->object_id = KF_OPENING_ENTITY_FREE;
    opening_camera_path_begin(opening_ending_scroll_camera_path);
    lighting_phase = ENDING_LIGHT_TO_MIDPOINT;
    background_blend = 0;
    sequence_phase = ENDING_SEQUENCE_WAIT_SCROLL;
    scroll_state = ENDING_SCROLL_WAIT_DISK;
    scroll_tick = ENDING_SCROLL_TICK_STARFIELD_AND_PANELS;
    open_graphics_runtime.render_state.projection.fog_near = KF_INITIAL_FOG_NEAR_DISTANCE;
    open_graphics_runtime.render_state.lighting.ambient = {0, 0, 0};
    open_graphics_runtime.render_state.lighting.fog = {0, 0, 0};
    open_graphics_runtime.tmd_projection_shift = ENDING_TMD_PROJECTION_SHIFT;

    transition_position = {
        opening_camera_path_state.position.vx,
        KF_OPENING_SCENE_BASE_Y,
        opening_camera_path_state.position.vz};
    open_graphics_runtime.floor_item_state.material.color.r = 0;
    open_graphics_runtime.floor_item_state.material.color.g = 0;
    open_graphics_runtime.floor_item_state.material.color.b = 0;
    top_color = opening_ending_scroll_top_start;
    bottom_color = opening_ending_scroll_bottom_start;

    for (;;) {
        CVECTOR *top = &top_color;

        switch (lighting_phase) {
        case ENDING_LIGHT_TO_MIDPOINT:
            if (lighting_blend <= KF_FIXED12_ONE) {
                lighting_set_color_matrix(open_graphics_runtime.render_state,
                    &color_matrix_table[kf_enum_encode<s32>(KF_OPEN_COLOR_BLACK)],
                    &color_matrix_table[kf_enum_encode<s32>(KF_OPEN_COLOR_ENDING_MIDPOINT)], lighting_blend);
                lighting_blend += ENDING_LIGHT_MIDPOINT_STEP;
            } else {
                lighting_phase = ENDING_LIGHT_TO_GREEN;
                lighting_blend = 0;
            }
            break;
        case ENDING_LIGHT_TO_GREEN:
            if (lighting_blend <= KF_FIXED12_ONE) {
                lighting_set_color_matrix(open_graphics_runtime.render_state,
                    &color_matrix_table[kf_enum_encode<s32>(KF_OPEN_COLOR_ENDING_MIDPOINT)],
                    &color_matrix_table[kf_enum_encode<s32>(KF_OPEN_COLOR_ENDING_GREEN)], lighting_blend);
                lighting_blend += ENDING_LIGHT_GREEN_STEP;
            } else {
                lighting_phase = ENDING_LIGHT_FINISHED;
                lighting_blend = 0;
            }
            break;
        }
        switch (sequence_phase) {
        case ENDING_SEQUENCE_DELAY:
            if (--sequence_delay == -1) {
                sequence_phase = ENDING_SEQUENCE_FADE;
                sequence_volume = KF_AUDIO_MAX_VOLUME * ENDING_SEQUENCE_VOLUME_DIVISOR;
            }
            break;
        case ENDING_SEQUENCE_FADE:
            --sequence_volume;
            kf::sound_master_volume(sequence_volume / ENDING_SEQUENCE_VOLUME_DIVISOR,
                      sequence_volume / ENDING_SEQUENCE_VOLUME_DIVISOR);
            if (sequence_volume == 0) {
                sequence_phase = ENDING_SEQUENCE_REPLACED;
                opening_resources_load_ending_sequence();
            }
            break;
        }
        if (opening_camera_path_state.frames_remaining != KF_CAMERA_PATH_FINISHED) {
            opening_camera_path_step(0);
        }
        if (entity_26->position.vy < ENDING_MODEL_FINAL_Y) {
            entity_26->position.vy += ENDING_MODEL_Y_STEP;
        } else if (scroll_state == ENDING_SCROLL_WAIT_DISK) {
            entity_26->object_id = KF_OPENING_ENTITY_FREE;
            scroll_state = ENDING_SCROLL_ACTIVE;
            entity_27->object_id = KF_OPENING_ENDING_STARFIELD;
        }
        render_set_view_transform(open_graphics_runtime.render_state,
            &opening_camera_path_state.position, &opening_camera_path_state.rotation);
        display_begin_frame(open_graphics_runtime.display_state);
        opening_render_entities();

        background_blend += ENDING_BACKGROUND_BLEND_STEP;
        if (background_blend > KF_FIXED12_ONE) {
            background_blend = KF_FIXED12_ONE;
        }
        color_lerp_cvector(&opening_ending_scroll_top_start,
                          &opening_ending_scroll_top_end, top, background_blend);
        color_lerp_cvector(&opening_ending_scroll_bottom_start,
                          &opening_ending_scroll_bottom_end, &bottom_color, background_blend);
        sprite_add_g4(&opening_ending_scroll_backgrounds[0],
                      top, top, &bottom_color, &bottom_color);
        sprite_add_f4(&opening_ending_scroll_backgrounds[1],
                      &opening_ending_scroll_background_color, ENDING_BACKGROUND_OT_DEPTH);

        if (scroll_tick == ENDING_SCROLL_TICK_STARFIELD_AND_PANELS) {
            entity_27->rotation.z = (entity_27->rotation.z - 1) & KF_ANGLE_WRAP_MASK;
            if (entity_27->object_id != KF_OPENING_ENTITY_FREE &&
                open_graphics_runtime.floor_item_state.material.color.r < ENDING_MAX_BRIGHTNESS) {
                ++open_graphics_runtime.floor_item_state.material.color.r;
                open_graphics_runtime.floor_item_state.material.color.b =
                    open_graphics_runtime.floor_item_state.material.color.r;
                open_graphics_runtime.floor_item_state.material.color.g =
                    open_graphics_runtime.floor_item_state.material.color.r;
            }
        }
        if (scroll_state > ENDING_SCROLL_WAIT_DISK) {
            panel_index = 0;
            panel = opening_ending_scroll_panels;
            do {
                if (scroll_tick == ENDING_SCROLL_TICK_STARFIELD_AND_PANELS ||
                    scroll_tick == ENDING_SCROLL_TICK_PANELS) {
                    if (opening_ending_scroll_panels[ENDING_PANEL_COUNT - 1].y >
                        ENDING_PANEL_STOP_Y) {
                        --panel->y;
                    } else if (sequence_phase == ENDING_SEQUENCE_WAIT_SCROLL) {
                        sequence_phase = ENDING_SEQUENCE_DELAY;
                        sequence_delay = ENDING_SEQUENCE_DELAY_START;
                    }
                }
                if ((u16)(panel->y + PANEL_CLIP_Y_BIAS) < PANEL_CLIP_SPAN) {
                    sprite_add_ft4(panel, opening_ending_scroll_uv,
                                   materials[panel_index],
                                   &opening_ending_scroll_panel_color, PANEL_OT_DEPTH);
                }
                ++panel_index;
                panel++;
            } while (panel_index < ENDING_PANEL_COUNT);
            scroll_tick = kf_enum_decode<KfEndingScrollTick>(kf_enum_encode<s16>(scroll_tick) - 1);
            if (scroll_tick == ENDING_SCROLL_TICK_WRAP) {
                scroll_tick = ENDING_SCROLL_TICK_HOLD_AFTER_STARFIELD;
            }
        }
        display_present_frame(open_graphics_runtime.display_state);
    }
}

void opening_scenes_reset_module_state(void)
{
    kf::restore_initial_value<opening_scene0_camera_path>();
    kf::restore_initial_value<opening_scene3_camera_path>();
    kf::restore_initial_value<opening_ending_camera_path>();
    kf::restore_initial_value<opening_ending_scroll_camera_path>();
    kf::restore_initial_value<opening_scene0_sound>();
    kf::restore_initial_value<opening_scene3_overlay_rects>();
    kf::restore_initial_value<opening_ending_scroll_panels>();
    kf::restore_initial_value<opening_scene3_overlay_uv>();
    kf::restore_initial_value<opening_scene3_overlay_color>();
    kf::restore_initial_value<opening_ending_scroll_backgrounds>();
    kf::restore_initial_value<opening_ending_scroll_top_start>();
    kf::restore_initial_value<opening_ending_scroll_bottom_start>();
    kf::restore_initial_value<opening_ending_scroll_top_end>();
    kf::restore_initial_value<opening_ending_scroll_bottom_end>();
    kf::restore_initial_value<opening_ending_scroll_panel_color>();
    kf::restore_initial_value<opening_ending_scroll_background_color>();
    kf::restore_initial_value<opening_ending_scroll_uv>();
}
