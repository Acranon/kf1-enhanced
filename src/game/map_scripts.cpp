#include <kf/platform/prelude.h>
#include <kf/game/actor.h>
#include <kf/game/audio.h>
#include <kf/game/collision.h>
#include <kf/game/game.h>
#include <kf/game/graphics.h>
#include <kf/game/notify.h>
#include <kf/lib/map.h>
#include <kf/lib/map_data.h>
#include <kf/lib/null.h>
#include <kf/lib/random.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
constexpr unsigned floor2_harp_ambient_stage = 2, floor2_harp_ambient_page_end = 3;
constexpr unsigned floor2_reveal_stage = 2, floor3_fire_ball_stage = 3, floor5_weapon_transform_stage = 5;
struct ExchangeDialogue {
    unsigned event_slot, stage, page_end, last_page_slot, last_page, response_page, next_page, stage_limit;
};
constexpr ExchangeDialogue key_exchange = {2, 1, 3, 0, 7, 3, 4, 5};
constexpr ExchangeDialogue healing_exchange = {2, 2, 2, 1, 7, 2, 3, 5};
constexpr ExchangeDialogue harp_exchange = {1, 2, 2, 1, 5, 2, 3, 2};
struct ScriptCellRegion { int x_min, z_min, x_end, z_end; };
struct ScriptPointXZ { int x, z; };
constexpr ScriptCellRegion floor1_actor_entry = {7, 31, 12, 41};
constexpr ScriptCellRegion floor1_actor_exit = {2, 25, 14, 46};
constexpr ScriptCellRegion floor1_removal_entry = {2, 27, 5, 30};
constexpr ScriptCellRegion floor1_removal_exit = {2, 11, 28, 41};
constexpr ScriptPointXZ floor1_removed_object = {9000, 57000};
constexpr ScriptPointXZ floor1_cross = {21000, 67000};
constexpr int floor3_healing_x_min = 15, floor3_healing_x_end = 18, floor3_healing_z = 64;
constexpr int floor5_boss_trigger_x_min = 38, floor5_boss_trigger_x_end = 41, floor5_boss_trigger_z = 7;
constexpr ScriptPointXZ weapon_transform_effect_cell = {85, 40};
}

enum class KfMapWeaponTransformPhase : s32 {
    MAP_WEAPON_TRANSFORM_SPIN_UP = 0,
    MAP_WEAPON_TRANSFORM_SPIN_DOWN = 1
}; using enum KfMapWeaponTransformPhase;

enum class KfMapImageGroup : s32 {
    MAP_IMAGE_GROUP_SIGNBOARD = 0,
    MAP_IMAGE_GROUP_INSCRIPTION = 1
}; using enum KfMapImageGroup;

enum {
    MAP_WEAPON_TRANSFORM_HOLD_UPDATES = 40,
    MAP_WEAPON_TRANSFORM_SWAP_COUNTDOWN = 20,
    MAP_SHOP_SEQUENCE_INDEX = 2,
    MAP_FLOOR3_DIALOGUE_DOOR_LINK = 0x37
};

enum {
    MAP_FLOOR1_AMBIENT_VOLUME = 115,
    MAP_SCRIPT_OBJECT_SEARCH_PADDING = 3000,
    MAP_FLOOR2_AMBIENT_RANDOM_LIMIT = 4000,
    MAP_BOSS_REVEAL_YAW_MIN = 1808,
    MAP_BOSS_REVEAL_YAW_END = 2289,
    MAP_PASSAGE_OPEN_SOUND_VOLUME = 100,
    MAP_TRANSFER_FADE_IN_STEP = 128,
    MAP_TRANSFER_LIGHT_BLEND_SHIFT = 2,
    MAP_TRANSFER_RISE_STEP = 130,
    MAP_TRANSFER_YAW_STEP = 128,
    MAP_TRANSFER_FADE_OUT_STEP = 256,
    MAP_WEAPON_TRANSFORM_HEIGHT = 1300,
    MAP_WEAPON_TRANSFORM_BLAST_HEIGHT = 600,
    MAP_WEAPON_TRANSFORM_MAX_YAW_STEP = 240,
    MAP_WEAPON_TRANSFORM_YAW_ACCELERATION = 1,
    MAP_ATTRIBUTE_PROBE_DISTANCE = 1500,
    MAP_CONTAINER_CAMERA_PITCH_SPAN = KF_ANGLE_HALF_TURN - KF_PLAYER_CAMERA_PITCH_LIMIT + 1,
    MAP_CONTAINER_CAMERA_PITCH_STEP = 16,
    MAP_CONTAINER_OPEN_PITCH_STEP = 32,
    MAP_DOOR_PARTNER_SEARCH_PADDING = 6000
};

static constexpr bool map_dialogue_just_started(const KfDialogueState &dialogue, unsigned stage)
{
    return dialogue.stage == stage && dialogue.page == KF_DIALOGUE_FIRST_PAGE
        && dialogue.page_delay == KF_DIALOGUE_PAGE_DELAY_TICKS;
}

static constexpr bool script_region_contains_cell(const ScriptCellRegion &region, KfMapCellCoordinates cell)
{
    return cell.x >= region.x_min && cell.z >= region.z_min
        && cell.x < region.x_end && cell.z < region.z_end;
}

static std::array<KfCameraPathPoint, 2> map_floor5_camera_path = {
    KfCameraPathPoint{{173000, -11500, 85000, 0}, {0, KF_ANGLE_HALF_TURN, 0, 0}, 100, 0},
    KfCameraPathPoint{{KF_CAMERA_PATH_END_X, -1, -1, 0}, {-1, -1, -1, 0}, -1, 0}
};

static VECTOR map_floor1_sound_position = {65000, -10000, 25000, 0};

static MATRIX map_transfer_light_matrix = {
    .m = {{
        {0, -KF_FIXED12_ONE, 0},
        {0, -KF_FIXED12_ONE, 0},
        {0, -KF_FIXED12_ONE, 0},
    }},
    .t = {},
};

static constexpr unsigned map_screen_path_capacity = 16;
static constexpr unsigned map_screen_floor_offset = 5, map_screen_group_offset = 8, map_screen_number_offset = 9;
static std::array<char, map_screen_path_capacity> map_screen_image_path = {"KAN/B0/K000.TIM"};

s32 actor_pool_find_at_tile(u8 tile_x, u8 tile_z)
{
    KfActor *actor = actor_state.actors.data();
    s16 index;

    for (index = 0; index < KF_ACTOR_CAPACITY; index++, actor++) {
        if (actor->slot_state != KF_ACTOR_SLOT_FREE && actor->tile_x == tile_x
            && actor->tile_z == tile_z) {
            return index;
        }
    }
    return -1;
}

void map_ambient_script_floor1(void)
{
    const auto &cell = player_state.motion_state.map_cell;
    if (map_floor_script(KF_FLOOR_1).floor1.revival_enabled == KF_MAP_SCRIPT_SET) {
        audio_play_spatial_default_range(
            &gameplay_sound_refs[KF_GAMEPLAY_SOUND_FLOOR1_REVIVAL], &map_floor1_sound_position, MAP_FLOOR1_AMBIENT_VOLUME);
    }

    switch (map_floor_script(KF_FLOOR_1).floor1.actor_activation_stage) {
    case KF_MAP_TRIGGER_COMPLETE:
        break;
    case KF_MAP_TRIGGER_AWAIT_ENTRY:
        if (script_region_contains_cell(floor1_actor_entry, cell)) {
            map_floor_script(KF_FLOOR_1).floor1.actor_activation_stage = KF_MAP_TRIGGER_AWAIT_EXIT;
        }
        break;
    case KF_MAP_TRIGGER_AWAIT_EXIT:
        if (!script_region_contains_cell(floor1_actor_exit, cell)) {
            s32 actor_index;
            s32 object_index;

            map_floor_script(KF_FLOOR_1).floor1.actor_activation_stage = KF_MAP_TRIGGER_COMPLETE;
            actor_index = actor_pool_find_at_tile(KF_FLOOR1_TRIGGER_ACTOR_TILE_X, KF_FLOOR1_TRIGGER_ACTOR_TILE_Z);
            if (actor_index != -1) {
                actor_state.actors[actor_index].lifecycle = KF_ACTOR_LIFECYCLE_DORMANT;
                actor_initialize_slot(actor_index);
            }
            object_index = map_floor1_cross_index();
            if (object_index != -1) {
                map_object_state.objects[object_index].object_id = KF_MAP_OBJECT_BROKEN_STONE_CROSS;
            }
        }
        break;
    }

    switch (map_floor_script(KF_FLOOR_1).floor1.object_removal_stage) {
    case KF_MAP_TRIGGER_COMPLETE:
        break;
    case KF_MAP_TRIGGER_AWAIT_ENTRY:
        if (script_region_contains_cell(floor1_removal_entry, cell)) {
            map_floor_script(KF_FLOOR_1).floor1.object_removal_stage = KF_MAP_TRIGGER_AWAIT_EXIT;
        }
        break;
    case KF_MAP_TRIGGER_AWAIT_EXIT:
        if (!script_region_contains_cell(floor1_removal_exit, cell)) {
            s32 object_index;

            map_floor_script(KF_FLOOR_1).floor1.object_removal_stage = KF_MAP_TRIGGER_COMPLETE;
            object_index = map_object_pool_find_near_point(floor1_removed_object.x, floor1_removed_object.z, MAP_SCRIPT_OBJECT_SEARCH_PADDING);
            if (object_index != -1) {
                map_object_state.objects[object_index].object_id = KF_OBJECT_NONE;
            }
        }
        break;
    }
}

s32 map_floor1_cross_index(void)
{
    return map_object_pool_find_near_point(floor1_cross.x, floor1_cross.z, MAP_SCRIPT_OBJECT_SEARCH_PADDING);
}

void map_ambient_script_floor2(void)
{
    if (map_runtime_state.events[KF_FLOOR2_HARP_EVENT].dialogue.stage == floor2_harp_ambient_stage && map_runtime_state.events[KF_FLOOR2_HARP_EVENT].dialogue.page < floor2_harp_ambient_page_end
        && kf::random_next() < MAP_FLOOR2_AMBIENT_RANDOM_LIMIT) {
        audio_play_spatial_default_range(
            &gameplay_sound_refs[KF_GAMEPLAY_SOUND_HARP], &map_runtime_state.events[KF_FLOOR2_HARP_EVENT].reference_position, KF_AUDIO_MAX_VOLUME);
    }
}

void map_ambient_script_floor3(void)
{
    const auto &cell = player_state.motion_state.map_cell;
    if (cell.x >= floor3_healing_x_min && cell.x < floor3_healing_x_end
        && cell.z == floor3_healing_z) {
        player_restore_vitals_with_color_cycle();
        if (effect_state.magic.entries[kf_enum_encode<u8>(KF_MAGIC_RESIST_FIRE)].learned == KF_MAGIC_UNLEARNED || effect_state.magic.entries[kf_enum_encode<u8>(KF_MAGIC_BLESS)].learned == KF_MAGIC_UNLEARNED) {
            effect_state.magic.entries[kf_enum_encode<u8>(KF_MAGIC_RESIST_FIRE)].learned = KF_MAGIC_LEARNED;
            effect_state.magic.entries[kf_enum_encode<u8>(KF_MAGIC_BLESS)].learned = KF_MAGIC_LEARNED;
            notify_enqueue(KF_NOTIFICATION_MAGIC_LEARNED);
        }
    }
}

void map_ambient_script_floor4(void)
{
}

void map_ambient_script_floor5(void)
{
    const auto &cell = player_state.motion_state.map_cell;
    KfMapScriptFlag *encounter_started = &map_floor_script(KF_FLOOR_5).floor5.boss_encounter_started;

    if (*encounter_started == KF_MAP_SCRIPT_UNSET && cell.x >= floor5_boss_trigger_x_min
        && cell.x < floor5_boss_trigger_x_end && cell.z == floor5_boss_trigger_z
        && (u16)player_state.camera_rotation.vy >= MAP_BOSS_REVEAL_YAW_MIN
        && (u16)player_state.camera_rotation.vy < MAP_BOSS_REVEAL_YAW_END) {
        *encounter_started = KF_MAP_SCRIPT_SET;
        screen_show_image_until_input("TALK/C17/T55171.TIM");
        render_frame(NULL, NULL);
        render_frame(NULL, NULL);
        screen_show_image_until_input("TALK/C17/T55172.TIM");
        actor_state.definitions.entries[KF_FLOOR5_BOSS_DEFINITION].action_animations[KF_ACTOR_ANIM_SLOT_MELEE] = KF_ANIMATION_CLIP_THIRD;
        actor_state.definitions.entries[KF_FLOOR5_BOSS_DEFINITION].action_animations[KF_ACTOR_ANIM_SLOT_EFFECT0] = KF_ANIMATION_CLIP_FOURTH;
        actor_state.definitions.entries[KF_FLOOR5_BOSS_DEFINITION].action_animations[KF_ACTOR_ANIM_SLOT_EFFECT1] = KF_ANIMATION_CLIP_FOURTH;
        actor_state.definitions.entries[KF_FLOOR5_BOSS_DEFINITION].action_animations[KF_ACTOR_ANIM_SLOT_EFFECT2] = KF_ANIMATION_CLIP_FOURTH;
        actor_state.definitions.entries[KF_FLOOR5_BOSS_DEFINITION].action_animations[KF_ACTOR_ANIM_SLOT_MULTI_HIT_ATTACK] = KF_ANIMATION_CLIP_SECOND;
        map_apply_copy_region(KF_MAP_COPY_FLOOR5_BOSS_ENCOUNTER);
    }
}

void map_action_script_floor1(void)
{
    if (item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)][kf_enum_encode<u8>(KF_ITEM_DRAGON_CHALICE)] != 0
        && map_floor_script(KF_FLOOR_1).floor1.passage_opened == KF_MAP_SCRIPT_UNSET) {
        map_floor_script(KF_FLOOR_1).floor1.passage_opened = KF_MAP_SCRIPT_SET;
        map_apply_copy_region(KF_MAP_COPY_FLOOR1_PASSAGE);
        sound_ref_play(audio_playback(), &gameplay_sound_refs[KF_GAMEPLAY_SOUND_STONE_PASSAGE], MAP_PASSAGE_OPEN_SOUND_VOLUME);
    }
}

void map_floor2_event_transfer_fade(void)
{
    const auto input_context = kf::host_set_input_context(kf::InputContext::Scripted);
    MATRIX saved;
    s32 blend;

    saved = game_graphics_runtime.map_event_light_matrix;

    for (blend = 0; blend < KF_FIXED12_ONE + 1; blend += MAP_TRANSFER_FADE_IN_STEP) {
        lighting_set_color_matrix(game_graphics_runtime.render_state, &color_matrix_table[kf_enum_encode<s32>(KF_GAME_COLOR_DEFAULT)], &color_matrix_table[kf_enum_encode<s32>(KF_GAME_COLOR_WHITE)], blend);
        if (blend >= KF_FIXED12_ONE / 4 + 1) {
            map_runtime_state.events[KF_FLOOR2_REVEAL_EVENT].reference_position.vy -= MAP_TRANSFER_RISE_STEP;
            map_runtime_state.events[KF_FLOOR2_REVEAL_EVENT].rotation.vy += MAP_TRANSFER_YAW_STEP;
        } else {
            matrix_interpolate(&saved, &map_transfer_light_matrix,
                               &game_graphics_runtime.map_event_light_matrix, blend << MAP_TRANSFER_LIGHT_BLEND_SHIFT);
        }
        render_frame(NULL, NULL);
    }

    map_runtime_state.events[KF_FLOOR2_REVEAL_EVENT].state = KF_MAP_EVENT_DISABLED;
    map_floor_script(KF_FLOOR_5).floor5.character_arrived = KF_MAP_SCRIPT_SET;

    for (blend = KF_FIXED12_ONE; blend >= 0; blend -= MAP_TRANSFER_FADE_OUT_STEP) {
        lighting_set_color_matrix(game_graphics_runtime.render_state, &color_matrix_table[kf_enum_encode<s32>(KF_GAME_COLOR_DEFAULT)], &color_matrix_table[kf_enum_encode<s32>(KF_GAME_COLOR_WHITE)], blend);
        render_frame(NULL, NULL);
    }

    lighting_set_active_color_matrix(KF_GAME_COLOR_DEFAULT);
    game_graphics_runtime.map_event_light_matrix = saved;
    kf::host_set_input_context(input_context);
}

void map_action_script_floor2(void)
{
    if (map_dialogue_just_started(map_runtime_state.events[KF_FLOOR2_REVEAL_EVENT].dialogue, floor2_reveal_stage)
        && map_runtime_state.events[KF_FLOOR2_REVEAL_EVENT].state == KF_MAP_EVENT_ACTIVE) {
        map_floor2_event_transfer_fade();
    }
}

void map_action_script_floor3(void)
{
    if (item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)][kf_enum_encode<u8>(KF_ITEM_WIND_BLADE_BRACELET)] != 0) {
        if (effect_state.magic.entries[kf_enum_encode<u8>(KF_MAGIC_WIND_CUTTER)].learned == KF_MAGIC_UNLEARNED) {
            effect_state.magic.entries[kf_enum_encode<u8>(KF_MAGIC_WIND_CUTTER)].learned = KF_MAGIC_LEARNED;
            notify_enqueue(KF_NOTIFICATION_MAGIC_LEARNED);
        }
    }
    if (map_dialogue_just_started(map_runtime_state.events[KF_FLOOR3_FIRE_BALL_EVENT].dialogue, floor3_fire_ball_stage)) {
        if (effect_state.magic.entries[kf_enum_encode<u8>(KF_MAGIC_FIRE_BALL)].learned == KF_MAGIC_UNLEARNED) {
            effect_state.magic.entries[kf_enum_encode<u8>(KF_MAGIC_FIRE_BALL)].learned = KF_MAGIC_LEARNED;
            notify_enqueue(KF_NOTIFICATION_MAGIC_LEARNED);
        }
    }
}

void map_action_script_floor4(void)
{
}

void map_floor5_weapon_transform_cutscene(void)
{
    const auto input_context = kf::host_set_input_context(kf::InputContext::Scripted);
    MATRIX color_matrix;
    KfCameraPathState path;
    KfMapObject *sword;
    SVECTOR direction;
    VECTOR spawn;
    s32 grid_height;
    s32 spin;
    s32 hold;
    KfMapWeaponTransformPhase phase;

    if (player_state.equipped_weapon_id == KF_ITEM_DRAGON_SWORD) {
        player_equip_weapon(KF_OBJECT_NONE);
    }
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)][kf_enum_encode<u8>(KF_ITEM_DRAGON_SWORD)] = 0;
    collision_adjust_cell_occupancy(player_state.motion_state.map_cell.x, player_state.motion_state.map_cell.z, -1);

    camera_path_begin(&path, map_floor5_camera_path);
    for (;;) {
        camera_path_step(&path, 0);
        if (path.frames_remaining == KF_CAMERA_PATH_FINISHED) {
            break;
        }
        render_frame(&path.position, &path.rotation);
    }

    player_state.camera_position = path.position;
    player_state.camera_rotation = path.rotation;
    player_state.motion_state.map_cell.x = player_state.camera_position.vx / KF_MAP_TILE_SIZE;
    player_state.motion_state.map_cell.z = player_state.camera_position.vz / KF_MAP_TILE_SIZE;
    collision_adjust_cell_occupancy(player_state.motion_state.map_cell.x, player_state.motion_state.map_cell.z, 1);

    color_matrix = game_graphics_runtime.render_state.lighting.color_matrix;
    sword = map_object_effect_pool_acquire(
        KF_MAP_OBJECT_PLACEMENT_DROP_FIRST, KF_MAP_OBJECT_EFFECT_GROUP_CAPACITY, map_object_state.placement_drop_sequence);
    sword->object_id = KF_ITEM_DRAGON_SWORD;
    sword->cell_x = weapon_transform_effect_cell.x;
    sword->cell_z = weapon_transform_effect_cell.z;
    sword->position.vx = sword->cell_x * KF_MAP_TILE_SIZE + KF_MAP_TILE_CENTER;
    sword->position.vz = sword->cell_z * KF_MAP_TILE_SIZE + KF_MAP_TILE_CENTER;
    grid_height = map_floor_height_grid.cells[sword->cell_z][sword->cell_x];
    sword->rotation.angles.z = 0;
    sword->rotation.angles.x = 0;
    sword->rotation.angles.y = KF_ANGLE_HALF_TURN;
    sword->action = KF_MAP_OBJECT_OP_NONE;
    sword->position.vy = -(grid_height * KF_MAP_HEIGHT_STEP) - MAP_WEAPON_TRANSFORM_HEIGHT;

    spin = 0;
    hold = 0;
    phase = MAP_WEAPON_TRANSFORM_SPIN_UP;
    for (;;) {
        switch (phase) {
        case MAP_WEAPON_TRANSFORM_SPIN_UP:
            sword->rotation.angles.y += spin;
            if (hold != 0) {
                hold -= 1;
                if (hold == 1) {
                    phase = MAP_WEAPON_TRANSFORM_SPIN_DOWN;
                } else if (hold == MAP_WEAPON_TRANSFORM_SWAP_COUNTDOWN) {
                    spawn = sword->position;
                    spawn.vy -= MAP_WEAPON_TRANSFORM_BLAST_HEIGHT;
                    effect_pool_construct(
                        0, KF_EFFECT_USE_PLAYER_MAGIC | KF_EFFECT_COLLISION_TARGET_ACTORS_AND_PLAYER,
                        KF_EFFECT_KIND_RADIAL_BLAST, &spawn, &direction, KfEffectSoundArguments{KF_EFFECT_SOUND_PLAY});
                    sword->object_id = KF_ITEM_MOONLIGHT_SWORD;
                }
            } else if (spin < MAP_WEAPON_TRANSFORM_MAX_YAW_STEP) {
                spin += MAP_WEAPON_TRANSFORM_YAW_ACCELERATION;
            } else {
                hold = MAP_WEAPON_TRANSFORM_HOLD_UPDATES;
            }
            break;
        case MAP_WEAPON_TRANSFORM_SPIN_DOWN:
            sword->rotation.angles.y += spin;
            if (spin > 0) {
                spin -= MAP_WEAPON_TRANSFORM_YAW_ACCELERATION;
            } else {
                map_object_start_action_if_idle(sword, KF_MAP_OBJECT_OP_FALL_AND_TIP);
                sword->link.fields.vertical_velocity = 0;
                kf::host_set_input_context(input_context);
                return;
            }
            break;
        default:
            break;
        }
        effect_pool_update();
        render_frame(NULL, NULL);
    }
}

void map_action_script_floor5(void)
{
    if (map_dialogue_just_started(map_runtime_state.events[KF_FLOOR5_WEAPON_TRANSFORM_EVENT].dialogue, floor5_weapon_transform_stage)) {
        map_floor5_weapon_transform_cutscene();
        map_floor_script(KF_FLOOR_5).floor5.weapon_transformed = KF_MAP_SCRIPT_SET;
    }
}

static bool map_exchange_is_available(const ExchangeDialogue &exchange, KfObjectId offered_item)
{
    const auto &inventory = item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)];
    const auto &dialogue = map_runtime_state.events[exchange.event_slot].dialogue;
    return inventory[kf_enum_encode<u8>(offered_item)] != 0
        && dialogue.stage == exchange.stage && dialogue.page < exchange.page_end;
}

static void map_finish_exchange_dialogue(KfMapEvent &speaker, const ExchangeDialogue &exchange)
{
    talk_show_dialogue_page(player_state.progress_state.current_floor,
        speaker.dialogue.stage, speaker.character_id, exchange.response_page);
    auto &target = map_runtime_state.events[exchange.event_slot];
    target.dialogue.page = exchange.next_page;
    target.dialogue.page_delay = 0;
    target.dialogue.stage_limit = exchange.stage_limit;
    map_event_refresh_dialogue_stage(&target);
}

void map_event_interact(KfMapEvent *event)
{
    auto &inventory = item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)];

    switch (event->character_id) {
    case KF_CHARACTER_KEY_OF_THE_DEAD_EXCHANGE:
        if (map_exchange_is_available(key_exchange, KF_ITEM_GOLD_CROSS)) {
            inventory[kf_enum_encode<u8>(KF_ITEM_KEY_OF_THE_DEAD)] = 1;
            map_runtime_state.events[key_exchange.event_slot].dialogue_pages
                .last_page[key_exchange.last_page_slot] = key_exchange.last_page;
            inventory[kf_enum_encode<u8>(KF_ITEM_GOLD_CROSS)]--;
            map_finish_exchange_dialogue(*event, key_exchange);
            return;
        }
        break;
    case KF_CHARACTER_HEALING_EXCHANGE:
        if (map_exchange_is_available(healing_exchange, KF_ITEM_MIRROR_OF_TRUTH)) {
            effect_state.magic.entries[kf_enum_encode<u8>(KF_MAGIC_HEALING)].learned = KF_MAGIC_LEARNED;
            inventory[kf_enum_encode<u8>(KF_ITEM_MIRROR_OF_TRUTH)]--;
            notify_enqueue(KF_NOTIFICATION_MAGIC_LEARNED);
            map_runtime_state.events[healing_exchange.event_slot].dialogue_pages
                .last_page[healing_exchange.last_page_slot] = healing_exchange.last_page;
            map_finish_exchange_dialogue(*event, healing_exchange);
            return;
        }
        break;
    case KF_CHARACTER_HARP_EXCHANGE:
        if (map_exchange_is_available(harp_exchange, KF_ITEM_DRAGON_KING_GRASS_FRUIT)) {
            inventory[kf_enum_encode<u8>(KF_ITEM_HARP)] = 1;
            map_runtime_state.events[harp_exchange.event_slot].dialogue_pages
                .last_page[harp_exchange.last_page_slot] = harp_exchange.last_page;
            inventory[kf_enum_encode<u8>(KF_ITEM_DRAGON_KING_GRASS_FRUIT)]--;
            map_finish_exchange_dialogue(*event, harp_exchange);
            return;
        }
        break;
    case KF_CHARACTER_FLOOR3_DOOR_UNLOCKER:
        map_object_pool_clear_link(MAP_FLOOR3_DIALOGUE_DOOR_LINK);
        break;
    default:
        break;
    }

    if (event->dialogue.stage_limit != 0) {
        if (event->dialogue_pages.last_page[event->dialogue.stage - 1] != 0) {
            talk_show_dialogue_page(player_state.progress_state.current_floor,
                                    event->dialogue.stage, event->character_id, event->dialogue.page);
            if (event->dialogue.page_delay == 0) {
                event->dialogue.page_delay = KF_DIALOGUE_PAGE_DELAY_TICKS;
            }
        }
    }
}

void map_show_screen_image(KfMapImageGroup group, s32 index)
{
    char *directory_floor = &map_screen_image_path[map_screen_floor_offset];

    *directory_floor = kf_enum_encode<u8>(player_state.progress_state.current_floor) + '0';
    map_screen_image_path[map_screen_group_offset] = kf_enum_encode<s32>(group) + '0';
    map_screen_image_path[map_screen_number_offset] = index / 10 + '0';
    map_screen_image_path[map_screen_number_offset + 1] = index % 10 + '0';
    screen_show_image_until_input(directory_floor - map_screen_floor_offset);
}

static void map_finish_event_interaction(KfMapEvent *event)
{
    event->animation_phase = 0;
    player_clear_motion();
}

static void map_interact_hinged_container(KfMapObject *object,
    const VECTOR *position, SVECTOR *rotation)
{
    u16 saved_pitch;
    KfMenuResult pickup_result;
    s16 item_index;
    KfObjectId *item_id;

    if (object->link.fields.link_id != KF_MAP_LINK_NONE) {
        notify_enqueue(object->link.fields.linked_notification);
        return;
    }
    if (!angle_within_tolerance(rotation->vy, object->rotation.angles.y, KF_ANGLE_EIGHTH_TURN)) {
        return;
    }

    item_index = KF_MAP_CONTAINER_ITEM_COUNT - 1;
    for (;;) {
        KfObjectId item_parameter = object->link.hinged_container.item_ids[0];
        s32 item_scan_end;

        if (item_parameter != KF_OBJECT_NONE) {
            break;
        }
        item_scan_end = -1;
        if (--item_index == item_scan_end) {
            notify_enqueue(object->link.fields.default_notification);
            return;
        }
    }

    saved_pitch = rotation->vx;
    audio_play_spatial_default_range(
        &gameplay_sound_refs[KF_GAMEPLAY_SOUND_CONTAINER_OPEN], &object->position, KF_AUDIO_MAX_VOLUME);
    while (object->rotation.angles.x >= -(KF_ANGLE_QUARTER_TURN - 1)) {
        u16 current_pitch = rotation->vx;
        u16 relative_pitch = current_pitch;

        relative_pitch -= KF_PLAYER_CAMERA_PITCH_LIMIT;
        if (relative_pitch >= MAP_CONTAINER_CAMERA_PITCH_SPAN) {
            rotation->vx = current_pitch + MAP_CONTAINER_CAMERA_PITCH_STEP;
        }
        object->rotation.angles.x -= MAP_CONTAINER_OPEN_PITCH_STEP;
        render_frame(position, rotation);
    }

    item_id = object->link.hinged_container.item_ids;
    item_index = KF_MAP_CONTAINER_ITEM_COUNT - 1;
    for (;;) {
        if (*item_id != KF_OBJECT_NONE) {
            pickup_result = menu_confirm_pickup(*item_id);
            switch (pickup_result) {
            case KF_MENU_RESULT_ACCEPTED:
                *item_id = KF_OBJECT_NONE;
                break;
            case KF_MENU_RESULT_STACK_FULL:
                notify_enqueue(KF_NOTIFICATION_CANNOT_CARRY_MORE);
                break;
            default:
                break;
            }
        }
        item_index--;
        if (item_index == -1) {
            break;
        }
        item_id++;
    }
    object->rotation.angles.x = 0;
    rotation->vx = saved_pitch;
}

static void map_start_hinged_door_pair(KfMapObject *object,
    const KfMapObjectDefinition *definition, s32 index)
{
    s32 neighbor_index;
    KfMapObject *neighbor;
    KfMapObjectDefinition *neighbor_definition;

    neighbor_index = 0;
    for (;;) {
        neighbor_index = map_object_pool_find_interaction_from(
            neighbor_index, object->position.vx, object->position.vz, MAP_DOOR_PARTNER_SEARCH_PADDING);
        if (neighbor_index == -1) {
            break;
        }
        if (neighbor_index != index) {
            neighbor = &map_object_state.objects[neighbor_index];
            neighbor_definition =
                &map_object_state.definitions.entries[kf_enum_encode<u8>(neighbor->object_id)];
            if (neighbor_definition->behavior_type == KF_MAP_OBJECT_OP_HINGED_DOOR
                || neighbor_definition->behavior_type == KF_MAP_OBJECT_OP_HINGED_DOOR_PARTNER) {
                if (neighbor->link.fields.link_id != KF_MAP_LINK_NONE
                    && neighbor_definition->behavior_type == KF_MAP_OBJECT_OP_HINGED_DOOR) {
                    notify_enqueue(object->link.fields.default_notification);
                    return;
                }
                map_object_start_action_if_idle(
                    neighbor, neighbor_definition->behavior_type);
                object->link.fields.action_parameter.object_index = neighbor_index;
                neighbor->link.fields.action_parameter.object_index = index;
                map_object_start_action_if_idle(object, definition->behavior_type);
                return;
            }
        }
        neighbor_index++;
    }
    object->link.fields.action_parameter.object_index = KF_MAP_OBJECT_PARAMETER_NONE;
    map_object_start_action_if_idle(object, definition->behavior_type);
}

void map_interaction_dispatch(const VECTOR *position, SVECTOR *rotation)
{
    const auto input_context = kf::host_set_input_context(kf::InputContext::Scripted);
    s32 index;
    s32 result;
    KfMenuResult pickup_result;
    bool found_item;
    KfMapEvent *event;
    KfMapObject *object;
    KfMapObjectDefinition *definition;

    const auto attribute_probe = vector_yaw_probe_xz(
        *position, rotation->vy, MAP_ATTRIBUTE_PROBE_DISTANCE);
    switch (map_attribute_at_cell(
        attribute_probe.x / KF_MAP_TILE_SIZE, attribute_probe.z / KF_MAP_TILE_SIZE)) {
    case KF_MAP_ATTRIBUTE_PITFALL:
        notify_enqueue(KF_NOTIFICATION_PITFALL);
        break;
    case KF_MAP_ATTRIBUTE_POISON_HOLE:
        notify_enqueue(KF_NOTIFICATION_POISON_HOLE);
        break;
    case KF_MAP_ATTRIBUTE_BOTTOMLESS_PIT:
        notify_enqueue(KF_NOTIFICATION_BOTTOMLESS_PIT);
        break;
    case KF_MAP_ATTRIBUTE_HIDDEN_DOOR:
        notify_enqueue(KF_NOTIFICATION_HIDDEN_DOOR);
        break;
    default:
        break;
    }

    const auto interaction_probe = vector_yaw_probe_xz(
        *position, rotation->vy, MAP_INTERACTION_PROBE_DISTANCE);
    if (game_graphics_runtime.notification_state.control.effect_phase == KF_NOTIFICATION_IDLE
        && (index = map_event_pool_find_overlap(
                interaction_probe.x, interaction_probe.z, MAP_INTERACTION_RADIUS_PADDING)) != -1) {
        event = &map_runtime_state.events[index];
        switch (event->behavior) {
            case KF_MAP_EVENT_BEHAVIOR_SHOP:
                event->animation_phase = 0;
                event->animation_clip = KF_ANIMATION_CLIP_FIRST;
                map_event_advance_animation_blocking(event, KF_MAP_EVENT_ANIMATION_TALK_POSE, KF_MAP_EVENT_ANIMATION_TALK_STEP);
                audio_play_map_sequence(MAP_SHOP_SEQUENCE_INDEX);
                map_event_interact(event);
                menu_open_shop(kf_enum_decode<KfItemStockBank>(kf_enum_encode<u8>(event->character_id)));
                audio_play_current_map_sequence();
                map_event_advance_animation_blocking(event, KF_MAP_EVENT_ANIMATION_PHASE_MASK, KF_MAP_EVENT_ANIMATION_TALK_STEP);
                map_finish_event_interaction(event);
                break;
            case KF_MAP_EVENT_BEHAVIOR_ANIMATION_LOOP:
                result = game_graphics_runtime.asset_animations[
                    event->model_index + KF_ASSET_MAP_EVENT_FIRST].clips.size();
                map_event_advance_animation_blocking(event, KF_MAP_EVENT_ANIMATION_PHASE_MASK, KF_MAP_EVENT_ANIMATION_FINISH_STEP);
                result = result < 2;
                if (result == 0) {
                    event->animation_phase = 0;
                    event->animation_clip = KF_ANIMATION_CLIP_SECOND;
                    map_event_advance_animation_blocking(event, KF_MAP_EVENT_ANIMATION_TALK_POSE, KF_MAP_EVENT_ANIMATION_TALK_STEP);
                }
                map_event_interact(event);
                if (result == 0) {
                    map_event_advance_animation_blocking(event, KF_MAP_EVENT_ANIMATION_PHASE_MASK, KF_MAP_EVENT_ANIMATION_TALK_STEP);
                }
                event->animation_clip = KF_ANIMATION_CLIP_FIRST;
                map_finish_event_interaction(event);
                break;
            case KF_MAP_EVENT_BEHAVIOR_WANDER:
                map_event_interact(event);
                player_clear_motion();
                break;
            default:
                break;
        }
    } else {
        for (index = 0;; index++) {
            index = map_object_pool_find_interaction_from(
                index, interaction_probe.x, interaction_probe.z, MAP_INTERACTION_RADIUS_PADDING);
            if (index == -1) {
                break;
            }
            object = &map_object_state.objects[index];
            definition = &map_object_state.definitions.entries[
                kf_enum_encode<u8>(object->object_id)];
            switch (definition->behavior_type) {
            case KF_MAP_OBJECT_OP_HINGED_CONTAINER:
                map_interact_hinged_container(object, position, rotation);
                break;

            case KF_MAP_OBJECT_OP_ITEM_CONTAINER: {
                s16 item_index;
                KfObjectId *item_id;

                item_id = object->link.item_ids;
                found_item = false;
                item_index = KF_MAP_CONTAINER_ITEM_COUNT - 1;
                for (;;) {
                    if (*item_id != KF_OBJECT_NONE) {
                        found_item = true;
                        pickup_result = menu_confirm_pickup(*item_id);
                        switch (pickup_result) {
                        case KF_MENU_RESULT_ACCEPTED:
                            *item_id = KF_OBJECT_NONE;
                            break;
                        case KF_MENU_RESULT_STACK_FULL:
                            notify_enqueue(KF_NOTIFICATION_CANNOT_CARRY_MORE);
                            break;
                        default:
                            break;
                        }
                    }
                    item_index--;
                    if (item_index == -1) {
                        break;
                    }
                    item_id++;
                }
                if (found_item != false) {
                    break;
                }
                notify_enqueue(object->link.fields.default_notification);
                break;

            }

            case KF_MAP_OBJECT_OP_LIFT_DOOR:
                if (!angle_within_tolerance(rotation->vy, object->rotation.angles.y, MAP_DOOR_FACING_TOLERANCE)
                    && !angle_within_tolerance(
                        rotation->vy, object->rotation.angles.y + KF_ANGLE_HALF_TURN, MAP_DOOR_FACING_TOLERANCE)) {
                    break;
                }
                if (object->action != KF_MAP_OBJECT_OP_NONE) {
                    break;
                }
                if (object->link.fields.link_id != KF_MAP_LINK_NONE) {
                    notify_enqueue(object->link.fields.default_notification);
                    break;
                }
                map_object_start_action_if_idle(object, KF_MAP_OBJECT_OP_LIFT_DOOR);
                continue;

            case KF_MAP_OBJECT_OP_HINGED_DOOR:
            case KF_MAP_OBJECT_OP_HINGED_DOOR_PARTNER:
                if (!angle_within_tolerance(rotation->vy, object->rotation.angles.y, MAP_DOOR_FACING_TOLERANCE)
                    && !angle_within_tolerance(
                        rotation->vy, object->rotation.angles.y + KF_ANGLE_HALF_TURN, MAP_DOOR_FACING_TOLERANCE)) {
                    break;
                }
                if (object->link.fields.link_id != KF_MAP_LINK_NONE && definition->behavior_type == KF_MAP_OBJECT_OP_HINGED_DOOR) {
                    notify_enqueue(object->link.fields.default_notification);
                    break;
                }

                map_start_hinged_door_pair(object, definition, index);
                continue;

            case KF_MAP_OBJECT_OP_ITEM_PICKUP:
                pickup_result = menu_confirm_pickup(object->object_id);
                switch (pickup_result) {
                case KF_MENU_RESULT_ACCEPTED:
                    object->object_id = KF_OBJECT_NONE;
                    break;
                case KF_MENU_RESULT_STACK_FULL:
                    notify_enqueue(KF_NOTIFICATION_CANNOT_CARRY_MORE);
                    continue;
                default:
                    break;
                }
                break;

            case KF_MAP_OBJECT_OP_GOLD_PICKUP:
                result = object->link.gold_amount;
                notify_enqueue(KF_NOTIFICATION_GOLD, result);
                result += player_state.gold;
                player_state.gold = result;
                object->object_id = KF_OBJECT_NONE;
                break;

            case KF_MAP_OBJECT_OP_EFFECT_SWITCH:
                if (object->link.fields.link_id == KF_MAP_LINK_NONE) {
                    notify_enqueue(object->link.fields.default_notification);
                } else {
                    object->action_timer = KF_MAP_OBJECT_SWITCH_FORWARD;
                }
                break;

            case KF_MAP_OBJECT_OP_RESTORE_POINT:
                if (object->link.fields.link_id != KF_MAP_LINK_NONE) {
                    notify_enqueue(object->link.fields.default_notification);
                    break;
                }
                player_restore_vitals_with_color_cycle();
                continue;

            case KF_MAP_OBJECT_OP_SCREEN_IMAGE: {
                KfMapImageGroup image_group;

                if (game_graphics_runtime.notification_state.control.effect_phase != KF_NOTIFICATION_IDLE) {
                    break;
                }
                if (object->object_id == KF_MAP_OBJECT_SIGNBOARD) {
                    image_group = MAP_IMAGE_GROUP_SIGNBOARD;
                } else {
                    if (object->object_id != KF_MAP_OBJECT_INSCRIPTION_PANEL) {
                        kf::host_set_input_context(input_context);
                        return;
                    }
                    image_group = MAP_IMAGE_GROUP_INSCRIPTION;
                }
                map_show_screen_image(image_group, object->link.fields.link_id);
                player_clear_motion();
                continue;
            }

            case KF_MAP_OBJECT_OP_SAVE_POINT:
                map_world_state_persist();
                menu_save_confirm();
                continue;

            default:
                notify_enqueue(object->link.fields.default_notification);
                break;
            }
        }
    }

    switch (player_state.progress_state.current_floor) {
    case KF_FLOOR_1:
        map_action_script_floor1();
        break;
    case KF_FLOOR_2:
        map_action_script_floor2();
        break;
    case KF_FLOOR_3:
        map_action_script_floor3();
        break;
    case KF_FLOOR_4:
        map_action_script_floor4();
        break;
    case KF_FLOOR_5:
        map_action_script_floor5();
        break;
    default:
        break;
    }
    kf::host_set_input_context(input_context);
}

void map_scripts_reset_module_state(void)
{
    kf::restore_initial_value<map_floor5_camera_path>();
    kf::restore_initial_value<map_floor1_sound_position>();
    kf::restore_initial_value<map_transfer_light_matrix>();
    kf::restore_initial_value<map_screen_image_path>();
}
