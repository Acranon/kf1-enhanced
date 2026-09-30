#ifndef KF_GAME_STATE_H
#define KF_GAME_STATE_H

#include <kf/game/audio.h>
#include <kf/game/session.h>
#include <kf/lib/geometry_types.h>
#include <kf/lib/types.h>

#include <array>

// Floor-local script configuration: indices are not global character IDs.
inline constexpr u8 KF_FLOOR1_TRIGGER_ACTOR_TILE_X = 7;
inline constexpr u8 KF_FLOOR1_TRIGGER_ACTOR_TILE_Z = 40;
inline constexpr unsigned KF_FLOOR2_DEPARTING_EVENT = 0;
inline constexpr unsigned KF_FLOOR2_HARP_EVENT = 1;
inline constexpr unsigned KF_FLOOR2_REVEAL_EVENT = 3;
inline constexpr unsigned KF_FLOOR3_FIRE_BALL_EVENT = 1;
inline constexpr unsigned KF_FLOOR5_WEAPON_TRANSFORM_EVENT = 1;
extern u32 DAT_80057d24;
extern GameResult game_result;

enum {
    KF_GAMEPLAY_SOUND_LIFT_DOOR = 0,
    KF_GAMEPLAY_SOUND_HINGED_DOOR = 1,
    KF_GAMEPLAY_SOUND_CONTAINER_OPEN = 2,
    KF_GAMEPLAY_SOUND_EFFECT_SWITCH = 3,
    KF_GAMEPLAY_SOUND_FLOOR_DEFORM = 4,
    KF_GAMEPLAY_SOUND_FLOOR1_REVIVAL = 5,
    KF_GAMEPLAY_SOUND_WARP_SHIMMER = 6,
    KF_GAMEPLAY_SOUND_STONE_PASSAGE = 7,
    KF_GAMEPLAY_SOUND_HARP = 8,
    KF_GAMEPLAY_SOUND_FLOOR5_EVENT_LOOP = 10,
    KF_GAMEPLAY_SOUND_MAP_PIECE_REVEAL = 11,
    KF_GAMEPLAY_SOUND_KEY_UNLOCK = 12,
    KF_GAMEPLAY_SOUND_COUNT = 13
};
extern std::array<SoundRef, KF_GAMEPLAY_SOUND_COUNT> gameplay_sound_refs;
inline constexpr unsigned talk_image_path_capacity = 20;
extern std::array<char, talk_image_path_capacity> talk_image_path_template;

#endif // KF_GAME_STATE_H
