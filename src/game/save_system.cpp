#include <kf/platform/prelude.h>
#include <kf/game/audio.h>
#include <kf/game/game.h>
#include <kf/game/graphics.h>
#include <kf/game/player.h>
#include <kf/game/save.h>
#include <kf/lib/null.h>
#include <kf/lib/resource_file.h>
#include <kf/platform/saves.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
constexpr int save_no_equipment_id = 255;
constexpr u32 crc32_reflected_polynomial = 0xedb88320u;
// Compact floor stream emitted by map_world_state_persist and read by map_restore_floor_state.
constexpr unsigned saved_event_bytes = 7, saved_actor_bytes = 2;
constexpr unsigned saved_event_stage_limit_offset = 1, saved_event_stage_offset = 2;
constexpr unsigned saved_gold_drop_bytes = 4, saved_item_drop_bytes = 3;
struct MenuSoundNote { u8 program, note; };
constexpr MenuSoundNote menu_cursor_note = {14, 68};
constexpr MenuSoundNote menu_confirm_note = {13, 60};
constexpr MenuSoundNote menu_cancel_note = {15, 63};
constexpr unsigned talk_directory_character_offset = 6;
constexpr unsigned talk_floor_offset = 10;
constexpr unsigned talk_stage_offset = 11;
constexpr unsigned talk_character_tens_offset = 12;
constexpr unsigned talk_character_ones_offset = 13;
constexpr unsigned talk_page_offset = 14;
}

enum {
    SAVE_MESSAGE_NO_SPACE = 102,
    SAVE_MESSAGE_STALE_CATALOG = 103,
    SAVE_MESSAGE_WRITE_FAILED = 107,
    SAVE_MESSAGE_NO_DATA = 111,
    SAVE_MESSAGE_READ_FAILED = 112,
    MESSAGE_IMAGE_SKIP = 0xff,
    MENU_INPUT_SOUND_VOLUME = 0x40,
    IMAGE_WAIT_INITIAL_BRIGHTNESS = 32,
    IMAGE_WAIT_MAX_BRIGHTNESS = 127
};
std::array<char, talk_image_path_capacity> talk_image_path_template = {"TALK/C00/T00000.TIM"};
bool menu_load_message_image(s32 message_id);
void screen_show_image_until_input(const char *path);

// Version one lists every persisted field explicitly. Runtime pointers are
// deliberately absent; enum widths and byte order do not depend on the host ABI.
struct SavedGameState {
    KfPlayerState player;
    KfMapSavedWorld world;
    std::array<std::array<u8, KF_ITEM_COUNT>, KF_ITEM_STOCK_BANK_COUNT> stock;
    std::array<KfMagicLearningState, KF_MAGIC_RECORD_COUNT> learned;
};
enum class SaveCodecMode { Read, Write };
struct SaveCodec {
    u8 *data;
    std::size_t size, position;
    SaveCodecMode mode;
    bool valid;
};

template <typename T, typename Wire>
static void save_assign(T &field, Wire value) { field = static_cast<T>(value); }

template <typename Wire, typename T>
static void save_field(SaveCodec &io, T &field) {
    static_assert(std::is_integral_v<Wire> && sizeof(Wire) <= 4);
    if (!io.valid || sizeof(Wire) > io.size - io.position) {
        io.valid = false;
        return;
    }
    u32 bits = 0;
    if (io.mode == SaveCodecMode::Write) {
        if constexpr (std::is_integral_v<T>)
            bits = static_cast<Wire>(field);
        else
            bits = kf_enum_encode<Wire>(field);
        for (std::size_t i = 0; i < sizeof(Wire); ++i)
            io.data[io.position++] = bits >> (i * 8);
    } else {
        for (std::size_t i = 0; i < sizeof(Wire); ++i)
            bits |= static_cast<u32>(io.data[io.position++]) << (i * 8);
        save_assign(field, static_cast<Wire>(bits));
    }
}

static void save_bytes(SaveCodec &io, std::span<u8> bytes) {
    for (u8 &byte : bytes)
        save_field<u8>(io, byte);
}

static void save_state_fields(SaveCodec &io, SavedGameState &state) {
    auto &p = state.player;
#define SAVE_FIELD(type, name) save_field<type>(io, p.name)
#define SAVE_BYTES(name) save_bytes(io, p.name)
    SAVE_FIELD(s32, experience);
    SAVE_FIELD(s32, next_level_experience);
    SAVE_FIELD(u8, progress_state.level);
    SAVE_FIELD(u8, progress_state.unknown_01);
    SAVE_FIELD(u8, progress_state.current_floor);
    SAVE_FIELD(u8, progress_state.highest_floor);
    SAVE_FIELD(u8, map_variant);
    SAVE_FIELD(u8, allow_near_actor_spawn);
    SAVE_FIELD(u8, weapon_charge_delay);
    SAVE_FIELD(u8, unknown_0f);
    SAVE_FIELD(u16, vitals.maximum_hp);
    SAVE_FIELD(u16, vitals.current_hp);
    SAVE_FIELD(u16, vitals.maximum_mp);
    SAVE_FIELD(u16, vitals.current_mp);
    SAVE_FIELD(u16, attack_charge_state.current);
    SAVE_FIELD(u16, attack_charge_state.committed);
    SAVE_FIELD(u16, magic_charge);
    SAVE_FIELD(u16, physical_power_training);
    SAVE_FIELD(u16, magic_training);
    SAVE_FIELD(u16, base_physical_power);
    SAVE_FIELD(u16, base_magic);
    SAVE_FIELD(u16, physical_power);
    SAVE_FIELD(u16, magic);
    SAVE_FIELD(u16, status_effect_flags);
    SAVE_FIELD(u32, gold);
    SAVE_FIELD(u16, cutting_attack);
    SAVE_FIELD(u16, striking_attack);
    SAVE_FIELD(u16, piercing_attack);
    SAVE_FIELD(u16, holy_attack);
    SAVE_FIELD(u16, fire_attack);
    SAVE_BYTES(unknown_3a);
    SAVE_FIELD(u16, cutting_defense);
    SAVE_FIELD(u16, striking_defense);
    SAVE_FIELD(u16, piercing_defense);
    SAVE_FIELD(u16, poison_resistance);
    SAVE_FIELD(u16, magic_defense);
    SAVE_FIELD(u16, fire_defense);
    SAVE_FIELD(s16, curse_timer);
    SAVE_FIELD(s16, darkness_timer);
    SAVE_FIELD(s16, poison_timer);
    SAVE_FIELD(s16, slowed_timer);
    SAVE_FIELD(s16, fire_defense_timer);
    SAVE_FIELD(s16, illusion_staff_timer);
    SAVE_BYTES(unknown_54);
    SAVE_FIELD(u32, equipment_effect_ticks);
    SAVE_FIELD(u8, selected_magic_id);
    SAVE_BYTES(unknown_5d);
    SAVE_FIELD(u8, equipped_weapon_id);
    SAVE_BYTES(unknown_65);
    SAVE_FIELD(s16, weapon_attack_phase);
    SAVE_BYTES(unknown_72);
    SAVE_FIELD(u8, weapon_magic_shots_remaining);
    SAVE_FIELD(u8, weapon_magic_delay);
    SAVE_FIELD(u8, weapon_attack_fully_charged);
    SAVE_BYTES(unknown_7b);
    SAVE_FIELD(u8, equipped_head_armor_id);
    SAVE_FIELD(u8, equipped_body_armor_id);
    SAVE_FIELD(u8, equipped_shield_id);
    SAVE_FIELD(u8, equipped_arm_armor_id);
    SAVE_FIELD(u8, equipped_leg_armor_id);
    SAVE_FIELD(u8, equipped_accessory_id);
    SAVE_FIELD(u8, audio_effects_enabled);
    SAVE_FIELD(u8, audio_music_enabled);
    SAVE_FIELD(u8, hud_gauges_enabled);
    SAVE_FIELD(u8, compass_enabled);
    SAVE_FIELD(s16, view_rotation_offset.vx);
    SAVE_FIELD(s16, view_rotation_offset.vy);
    SAVE_FIELD(s16, view_rotation_offset.vz);
    SAVE_FIELD(u8, update_state);
    SAVE_FIELD(u8, unknown_a3);
    SAVE_FIELD(s32, camera_position.vx);
    SAVE_FIELD(s32, camera_position.vy);
    SAVE_FIELD(s32, camera_position.vz);
    SAVE_FIELD(s32, foot_height);
    SAVE_FIELD(s16, camera_rotation.vx);
    SAVE_FIELD(s16, camera_rotation.vy);
    SAVE_FIELD(s16, camera_rotation.vz);
    SAVE_FIELD(s16, motion_state.strafe_velocity);
    SAVE_FIELD(s16, motion_state.forward_velocity);
    SAVE_FIELD(u16, motion_state.movement_speed);
    SAVE_FIELD(s16, motion_state.yaw_step);
    SAVE_FIELD(s16, motion_state.pitch_step);
    SAVE_FIELD(u8, motion_state.map_cell.x);
    SAVE_FIELD(u8, motion_state.map_cell.z);
    SAVE_FIELD(u8, previous_map_cell.x);
    SAVE_FIELD(u8, previous_map_cell.z);
    SAVE_BYTES(unknown_ce);
    SAVE_FIELD(s16, view_bob_offset);
    SAVE_FIELD(u16, view_bob_phase);
    SAVE_FIELD(u16, death_camera_pitch_step);
    SAVE_FIELD(s16, death_visual_blend);
    SAVE_FIELD(s16, vertical_velocity);
    SAVE_FIELD(u8, vertical_state);
    SAVE_BYTES(unknown_df);
#undef SAVE_FIELD
#undef SAVE_BYTES
    // The existing world persistence producer already writes bounded-width
    // script/record byte streams. Neither field contains native pointers.
    for (auto &floor : state.world.floors) {
        save_bytes(io, floor.script.bytes);
        save_bytes(io, floor.records);
    }
    for (auto &stock : state.stock)
        save_bytes(io, stock);
    for (auto &learned : state.learned)
        save_field<u8>(io, learned);
}

static u32 save_checksum(const u8 *data, std::size_t size) {
    u32 crc = ~u32{0};
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (std::size_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (crc32_reflected_polynomial & (0u - (crc & 1)));
    }
    return ~crc;
}

static bool save_world_valid(const KfMapSavedWorld &world) {
    static_assert(sizeof(KfMapObjectLink) == 8, "Update the floor record boundary when the compact link representation changes");
    for (const auto &floor : world.floors) {
        if (floor.records[0] == 0)
            continue;
        if (floor.records[0] != 1)
            return false;
        std::size_t position = 1;
        for (std::size_t i = 0; i < KF_MAP_EVENT_CAPACITY; ++i, position += saved_event_bytes) {
            const auto state = kf_enum_decode<KfMapEventState>(floor.records[position]);
            const auto stage = floor.records[position + saved_event_stage_offset];
            if (floor.records[position + saved_event_stage_limit_offset] > KF_DIALOGUE_STAGE_COUNT || stage > KF_DIALOGUE_STAGE_COUNT
                    || (state != KF_MAP_EVENT_FREE && stage == 0))
                return false;
        }
        const unsigned actors = floor.records[position++];
        if (actors > KF_ACTOR_CAPACITY)
            return false;
        for (std::size_t i = 0; i < actors; ++i, position += saved_actor_bytes)
            if (floor.records[position] >= KF_ACTOR_CAPACITY)
                return false;
        const u8 *object_ids = floor.records.data() + position;
        for (std::size_t i = 0; i < KF_MAP_OBJECT_CAPACITY; ++i) {
            const auto id = floor.records[position++];
            if (id >= KF_MAP_OBJECT_DEFINITION_COUNT && id != kf_enum_encode<u8>(KF_OBJECT_NONE))
                return false;
            if (id != kf_enum_encode<u8>(KF_OBJECT_NONE) && i >= KF_MAP_OBJECT_GOLD_DROP_FIRST) {
                if (i < KF_MAP_OBJECT_DEFINITION_DROP_FIRST ? id != kf_enum_encode<u8>(KF_ITEM_GOLD_COIN) : id >= KF_ITEM_COUNT)
                    return false;
            }
        }
        const unsigned objects = floor.records[position++];
        // Eight serialized link bytes, preceded by the original object index.
        constexpr unsigned link_record_bytes = 9;
        constexpr unsigned drop_bytes = KF_MAP_OBJECT_EFFECT_GROUP_CAPACITY * (saved_gold_drop_bytes + 2 * saved_item_drop_bytes);
        if (objects > KF_MAP_OBJECT_EFFECT_FIRST || objects * link_record_bytes + drop_bytes > sizeof floor.records - position)
            return false;
        for (std::size_t i = 0; i < objects; ++i, position += link_record_bytes) {
            const unsigned index = floor.records[position];
            if (index >= KF_MAP_OBJECT_EFFECT_FIRST)
                return false;
            const auto id = object_ids[index];
            if (id == kf_enum_encode<u8>(KF_OBJECT_NONE))
                return false;
            KfMapObjectLink link{};
            std::memcpy(&link, floor.records.data() + position + 1, sizeof link);
            if (!map_saved_link_valid(map_object_state.definitions.entries[id].behavior_type, link))
                return false;
        }
        for (std::size_t group = 0; group < 3; ++group) {
            for (std::size_t i = 0; i < KF_MAP_OBJECT_EFFECT_GROUP_CAPACITY; ++i) {
                if (floor.records[position] >= KF_MAP_COLUMNS || floor.records[position + 1] >= KF_MAP_ROWS)
                    return false;
                position += group == 0 ? saved_gold_drop_bytes : saved_item_drop_bytes;
            }
        }
    }
    return true;
}

static bool save_state_valid(const SavedGameState &state) {
    const auto &p = state.player;
    const auto floor = kf_enum_encode<u8>(p.progress_state.current_floor);
    const auto highest = kf_enum_encode<u8>(p.progress_state.highest_floor);
    const auto weapon = kf_enum_encode<u8>(p.equipped_weapon_id);
    const auto magic = kf_enum_encode<u8>(p.selected_magic_id);
    if (p.vitals.maximum_hp == 0 || p.vitals.maximum_mp == 0
            || floor < 1 || floor > KF_MAP_SAVED_FLOOR_COUNT || highest < 1 || highest > KF_MAP_SAVED_FLOOR_COUNT
            || (weapon != save_no_equipment_id && weapon >= KF_WEAPON_RECORD_COUNT)
            || (magic != save_no_equipment_id && magic >= KF_MAGIC_PLAYER_COUNT)
            || p.camera_position.vx < 0 || p.camera_position.vx >= KF_MAP_COLUMNS * KF_MAP_TILE_SIZE
            || p.camera_position.vz < 0 || p.camera_position.vz >= KF_MAP_ROWS * KF_MAP_TILE_SIZE
            || p.motion_state.map_cell.x >= KF_MAP_COLUMNS
            || p.motion_state.map_cell.z >= KF_MAP_ROWS)
        return false;
    const std::array<KfObjectId, 5> armor = {p.equipped_head_armor_id, p.equipped_body_armor_id,
        p.equipped_shield_id, p.equipped_arm_armor_id, p.equipped_leg_armor_id};
    for (const auto id : armor) {
        const int index = kf_enum_encode<u8>(id) - kf_enum_encode<u8>(KF_ITEM_IRON_MASK);
        if (id != KF_OBJECT_NONE && (index < 0 || index >= KF_ARMOR_RECORD_COUNT))
            return false;
    }
    const auto accessory = kf_enum_encode<u8>(p.equipped_accessory_id);
    if (accessory != save_no_equipment_id && accessory >= KF_ITEM_COUNT)
        return false;
    for (const auto flag : state.learned)
        if (flag != KF_MAGIC_UNLEARNED && flag != KF_MAGIC_LEARNED)
            return false;
    return save_world_valid(state.world);
}

static KfArmorRecord *saved_armor(KfObjectId id) {
    return id == KF_OBJECT_NONE ? nullptr : &armor_records.entries[kf_enum_encode<u8>(id) - kf_enum_encode<u8>(KF_ITEM_IRON_MASK)];
}

static void save_state_apply(const SavedGameState &state) {
    auto *asset = player_state.weapon_asset_buffer;
    auto *cache = player_state.weapon_animation_cache;
    player_state = state.player;
    player_state.weapon_asset_buffer = asset;
    player_state.weapon_animation_cache = cache;
    player_state.selected_magic_record = player_state.selected_magic_id == KF_MAGIC_NONE ? nullptr
        : &effect_state.magic.entries[kf_enum_encode<u8>(player_state.selected_magic_id)];
    player_state.equipped_weapon_record = player_state.equipped_weapon_id == KF_OBJECT_NONE ? nullptr
        : &weapon_records.entries[kf_enum_encode<u8>(player_state.equipped_weapon_id)];
    player_state.equipped_head_armor_record = saved_armor(player_state.equipped_head_armor_id);
    player_state.equipped_body_armor_record = saved_armor(player_state.equipped_body_armor_id);
    player_state.equipped_shield_record = saved_armor(player_state.equipped_shield_id);
    player_state.equipped_arm_armor_record = saved_armor(player_state.equipped_arm_armor_id);
    player_state.equipped_leg_armor_record = saved_armor(player_state.equipped_leg_armor_id);
    map_runtime_state.world_state = state.world;
    item_stock = state.stock;
    for (std::size_t i = 0; i < KF_MAGIC_RECORD_COUNT; ++i)
        effect_state.magic.entries[i].learned = state.learned[i];
    // The existing load-return path reloads the floor, weapon and selected magic.
}

constexpr std::array<u8, 8> save_magic = {'K', 'F', 'J', 'P', 'S', 'A', 'V', 'E'};
constexpr u32 save_version = 1;
constexpr std::size_t save_header_bytes = 20;
struct SaveCatalogEntry { bool occupied; u32 checksum; };
static std::array<SaveCatalogEntry, KF_SAVE_SLOT_COUNT> save_catalog;

static bool save_slot_valid(kf::SaveSlot slot) {
    return slot >= kf::SaveSlot::First && slot <= kf::SaveSlot::Third;
}
static KfSaveSlotSummary save_summary(const SavedGameState &state) {
    const auto &p = state.player;
    return {static_cast<u32>(p.experience), static_cast<KfFloorId>(p.progress_state.current_floor),
        p.vitals.current_hp, p.vitals.maximum_hp, p.vitals.current_mp, p.vitals.maximum_mp,
        KfSaveSlotState::Ready};
}

static kf::SaveFileResult save_read_record(kf::SaveSlot slot, SavedGameState &state, u32 &checksum) {
    std::array<u8, kf::save_file_capacity> data;
    std::size_t size;
    const auto result = kf::save_file_read(slot, data.data(), data.size(), &size);
    if (result != kf::SaveFileResult::Ok)
        return result;
    if (size < save_header_bytes || std::memcmp(data.data(), save_magic.data(), save_magic.size()) != 0)
        return kf::SaveFileResult::Invalid;
    SaveCodec io{data.data(), size, sizeof save_magic, SaveCodecMode::Read, true};
    u32 version = 0, payload_size = 0;
    save_field<u32>(io, version);
    save_field<u32>(io, payload_size);
    save_field<u32>(io, checksum);
    if (!io.valid || version != save_version || payload_size != size - save_header_bytes
            || checksum != save_checksum(data.data() + save_header_bytes, payload_size))
        return kf::SaveFileResult::Invalid;
    save_state_fields(io, state);
    return io.valid && io.position == size && save_state_valid(state)
        ? kf::SaveFileResult::Ok : kf::SaveFileResult::Invalid;
}

static KfSaveResult save_failure(kf::SaveFileResult result, bool writing) {
    s32 message = writing ? SAVE_MESSAGE_WRITE_FAILED : SAVE_MESSAGE_READ_FAILED;
    if (result == kf::SaveFileResult::Missing)
        message = SAVE_MESSAGE_NO_DATA;
    else if (result == kf::SaveFileResult::NoSpace)
        message = SAVE_MESSAGE_NO_SPACE;
    std::fprintf(stderr, "%s save failed (%d)\n", writing ? "Write" : "Read", static_cast<int>(result));
    menu_load_message_image(message);
    return result == kf::SaveFileResult::NoSpace ? KF_SAVE_RESULT_NO_SPACE : KF_SAVE_RESULT_FAILED;
}

KfSaveResult save_system_read_catalog(std::array<KfSaveSlotSummary, KF_SAVE_SLOT_COUNT> &summaries) {
    summaries = {};
    save_catalog = {};
    for (std::size_t i = 0; i < summaries.size(); ++i) {
        SavedGameState state{};
        u32 checksum = 0;
        const auto slot = static_cast<kf::SaveSlot>(i + 1);
        const auto result = save_read_record(slot, state, checksum);
        if (result == kf::SaveFileResult::Missing)
            continue;
        if (result != kf::SaveFileResult::Ok) {
            summaries[i].state = result == kf::SaveFileResult::Invalid
                ? KfSaveSlotState::Damaged : KfSaveSlotState::Unavailable;
            std::fprintf(stderr, "Save slot %zu cannot be read (%d); other slots remain available\n",
                i + 1, static_cast<int>(result));
            continue;
        }
        save_catalog[i] = {true, checksum};
        summaries[i] = save_summary(state);
    }
    return KF_SAVE_RESULT_OK;
}

KfSaveResult save_system_read_slot(kf::SaveSlot slot) {
    if (!save_slot_valid(slot))
        return save_failure(kf::SaveFileResult::Invalid, false);
    SavedGameState state{};
    u32 checksum = 0;
    const auto result = save_read_record(slot, state, checksum);
    if (result != kf::SaveFileResult::Ok)
        return save_failure(result, false);
    const auto &catalog = save_catalog[kf_enum_encode<s16>(slot) - 1];
    if (!catalog.occupied || catalog.checksum != checksum) {
        menu_load_message_image(SAVE_MESSAGE_STALE_CATALOG);
        return KF_SAVE_RESULT_FAILED;
    }
    // No live game state is touched until the complete file is decoded and validated.
    save_state_apply(state);
    kf::automap_load(slot);
    return KF_SAVE_RESULT_OK;
}

static kf::SaveFileResult save_write_record(kf::SaveSlot slot, u32 &checksum) {
    SavedGameState state{};
    state.player = player_state;
    state.world = map_runtime_state.world_state;
    state.stock = item_stock;
    for (std::size_t i = 0; i < KF_MAGIC_RECORD_COUNT; ++i)
        state.learned[i] = effect_state.magic.entries[i].learned;
    if (!save_state_valid(state))
        return kf::SaveFileResult::Invalid;
    std::array<u8, kf::save_file_capacity> data;
    SaveCodec payload{data.data(), data.size(), save_header_bytes, SaveCodecMode::Write, true};
    save_state_fields(payload, state);
    if (!payload.valid)
        return kf::SaveFileResult::Invalid;
    std::memcpy(data.data(), save_magic.data(), save_magic.size());
    SaveCodec header{data.data(), save_header_bytes, sizeof save_magic, SaveCodecMode::Write, true};
    u32 version = save_version;
    u32 payload_size = payload.position - save_header_bytes;
    checksum = save_checksum(data.data() + save_header_bytes, payload_size);
    save_field<u32>(header, version);
    save_field<u32>(header, payload_size);
    save_field<u32>(header, checksum);
    return kf::save_file_write(slot, data.data(), payload.position);
}

KfSaveResult save_system_write_slot(kf::SaveSlot slot) {
    if (!save_slot_valid(slot))
        return save_failure(kf::SaveFileResult::Invalid, true);
    u32 checksum = 0;
    const auto result = save_write_record(slot, checksum);
    if (result != kf::SaveFileResult::Ok)
        return save_failure(result, true);
    save_catalog[kf_enum_encode<s16>(slot) - 1] = {true, checksum};
    kf::automap_save(slot);
    return KF_SAVE_RESULT_OK;
}

// PC extension: quick save/load (F2/F4) through the same record format, in its
// own file. The caller persists the current floor first and reloads it after.
bool save_system_quick_write(void) {
    u32 checksum = 0;
    const auto result = save_write_record(kf::SaveSlot::Quick, checksum);
    if (result != kf::SaveFileResult::Ok)
        std::printf("[quicksave] save failed (%d)\n", static_cast<int>(result));
    if (result == kf::SaveFileResult::Ok)
        kf::automap_save(kf::SaveSlot::Quick);
    return result == kf::SaveFileResult::Ok;
}

bool save_system_quick_read(void) {
    SavedGameState state{};
    u32 checksum = 0;
    const auto result = save_read_record(kf::SaveSlot::Quick, state, checksum);
    if (result != kf::SaveFileResult::Ok) {
        if (result == kf::SaveFileResult::Missing)
            std::printf("[quicksave] no quick save yet (press F2)\n");
        else
            std::printf("[quicksave] load failed (%d)\n", static_cast<int>(result));
        return false;
    }
    save_state_apply(state);
    kf::automap_load(kf::SaveSlot::Quick);
    return true;
}

void menu_play_input_sound(KfMenuSoundCue cue)
{
    SoundRef sound;

    if (cue == MENU_SOUND_CURSOR) {
        sound.program = menu_cursor_note.program;
        sound.note = menu_cursor_note.note;
    } else if (cue == MENU_SOUND_CONFIRM) {
        sound.program = menu_confirm_note.program;
        sound.note = menu_confirm_note.note;
    } else {
        sound.program = menu_cancel_note.program;
        sound.note = menu_cancel_note.note;
    }

    kf::sound_note_play(audio_state.bank, sound.program, sound.note, MENU_INPUT_SOUND_VOLUME, MENU_INPUT_SOUND_VOLUME);
    kf::host_wait_frame();
    kf::sound_note_release(audio_state.bank, sound.program, sound.note);
}

bool menu_load_message_image(s32 message_id)
{
    std::array<char, menu_image_path_capacity> path = {"TIM/M000."};
    u8 *buffer;

    if (message_id != MESSAGE_IMAGE_SKIP) {
        resource_path_write_decimal3(&path[menu_image_number_offset], message_id);
        buffer = game_graphics_runtime.display_state.asset_load_buffer;
        std::size_t image_size;
        if (resource_file_load_into(buffer,
                game_graphics_runtime.display_state.asset_load_capacity, path.data(), &image_size) != KF_RESOURCE_LOADED) {
            return true;
        }
        tim_upload_images(buffer, image_size);
    }
    return false;
}

void screen_show_image_until_input(const char *path)
{
    s32 brightness = IMAGE_WAIT_INITIAL_BRIGHTNESS;
    bool released = false;
    std::size_t image_size;
    if (resource_file_load_into(game_graphics_runtime.display_state.asset_load_buffer,
            game_graphics_runtime.display_state.asset_load_capacity, path, &image_size) != KF_RESOURCE_LOADED) {
        return;
    }
    tim_upload_images(game_graphics_runtime.display_state.asset_load_buffer, image_size);
    const auto input_context = kf::host_set_input_context(kf::InputContext::Menu);
    for (;;) {
        if (brightness < IMAGE_WAIT_MAX_BRIGHTNESS) {
            brightness++;
        }
        display_present_system_screen(brightness);
        kf::host_wait_frame();
        if (released == false) {
            if (kf::host_read_buttons() == 0) {
                released = true;
            }
        } else if (kf::host_read_buttons() != 0) {
            kf::host_wait_buttons_released();
            break;
        }
    }
    kf::host_set_input_context(input_context);
}

void talk_show_dialogue_page(KfFloorId floor, u8 stage, KfCharacterId character_id, u8 page)
{
    char *directory_character = &talk_image_path_template[talk_directory_character_offset];

    talk_image_path_template[talk_character_tens_offset] = kf_enum_encode<s32>(character_id) / 10 + '0';
    directory_character[0] = kf_enum_encode<s32>(character_id) / 10 + '0';
    talk_image_path_template[talk_floor_offset] = kf_enum_encode<u8>(floor) + '0';
    directory_character[1] = talk_image_path_template[talk_character_ones_offset] =
        kf_enum_encode<s32>(character_id) % 10 + '0';
    talk_image_path_template[talk_stage_offset] = stage + '0';
    talk_image_path_template[talk_page_offset] = page + '0';
    screen_show_image_until_input(directory_character - talk_directory_character_offset);
}

void save_system_reset_module_state(void)
{
    kf::restore_initial_value<talk_image_path_template>();
    kf::restore_initial_value<save_catalog>();
}
