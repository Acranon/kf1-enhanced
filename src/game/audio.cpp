#include <kf/game/resources.h>
#include <kf/lib/null.h>

#include <kf/lib/math.h>
#include <kf/game/audio.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <kf/game/game.h>
#include <kf/platform/input.hpp>

enum {
    GAME_SEQUENCE_BUFFER_BYTES = 0x3000,
    GAME_SEQUENCE_VOLUME = 0x4b,
    GAME_REVERB_DEPTH = 0x10,
    GAME_SOUND_PAN_NARROWING_GAIN_BOOST = 36,
    GAME_SOUND_PAN_NARROW_THRESHOLD = 64,
    GAME_SOUND_PAN_DIVISOR = 3000
};

s32 audio_voice_slot_index = 9;

KfAudioState audio_state;

void audio_initialize(void)
{
    kf::sound_reset(kf::ReverbPreset::Studio, GAME_REVERB_DEPTH, GAME_REVERB_DEPTH);
    kf::sound_master_volume(KF_AUDIO_MAX_VOLUME, KF_AUDIO_MAX_VOLUME);
    audio_state.bank = nullptr;
    audio_state.sequence = nullptr;
    audio_voice_slot_index = KF_AUDIO_VOICE_SLOTS - 1;
    audio_state.sequence_buffer = (u8 *)memory_allocate(memory_arena, GAME_SEQUENCE_BUFFER_BYTES);
    audio_state.sequence_active = KF_AUDIO_SEQUENCE_INACTIVE;
    audio_reset_voice_slots(audio_state);
}

void audio_load_vab(KfAudioBankResource resource)
{
    audio_stop_sequence_fade();
    audio_close_vab(audio_state);
    audio_state.bank = kf::sound_bank_load(resource.header, resource.header_size, resource.body, resource.body_size);
    if (!audio_state.bank)
        kf::host_fail("Cannot decode sound bank");
}

static constexpr unsigned sequence_path_capacity = 20;
static constexpr unsigned sequence_number_offset = 6, sequence_floor_offset = 1;

void audio_play_map_sequence(u8 sequence_id)
{
    char path[sequence_path_capacity] = "B0/SND0.SEQ";
    std::size_t sequence_size;

    audio_stop_sequence_fade();
    if (player_state.audio_music_enabled != KF_PLAYER_OPTION_OFF) {
        path[sequence_number_offset] = sequence_id + '0';
        path[sequence_floor_offset] = kf_enum_encode<u8>(player_state.progress_state.current_floor) + '0';
        if (resource_file_load_into(audio_state.sequence_buffer, GAME_SEQUENCE_BUFFER_BYTES, path, &sequence_size) == KF_RESOURCE_LOADED) {
            audio_state.sequence = kf::sound_sequence_load(audio_state.sequence_buffer, sequence_size, audio_state.bank);
            if (!audio_state.sequence)
                kf::host_fail("Cannot decode music sequence");
            kf::sound_sequence_volume(audio_state.sequence, GAME_SEQUENCE_VOLUME, GAME_SEQUENCE_VOLUME);
            kf::sound_sequence_play(audio_state.sequence);
            audio_state.sequence_active = KF_AUDIO_SEQUENCE_ACTIVE;
        }
    }
}

void audio_stop_sequence_fade(void)
{
    s32 volume;

    if (audio_state.sequence_active == KF_AUDIO_SEQUENCE_ACTIVE) {
        volume = GAME_SEQUENCE_VOLUME;
        do {
            kf::host_wait_frame();
            kf::sound_sequence_volume(audio_state.sequence, volume, volume);
        } while (--volume >= 0);
        audio_release_sequence(audio_state);
    }
}

void audio_stop_sequence_master_fade(s32 fade_step)
{
    s32 volume;

    if (audio_state.sequence_active == KF_AUDIO_SEQUENCE_ACTIVE) {
        volume = GAME_SEQUENCE_VOLUME << KF_FIXED8_BITS;
        do {
            kf::host_wait_frame();
            kf::sound_master_volume(volume >> KF_FIXED8_BITS, volume >> KF_FIXED8_BITS);
            volume -= fade_step;
        } while (volume > 0);
        kf::sound_master_volume(0, 0);
        kf::sound_sequence_volume(audio_state.sequence, 0, 0);
        audio_release_sequence(audio_state);
    }
}

KfAudioPlaybackResult audio_play_spatial(
    const SoundRef *sound,
    const VECTOR *position,
    s16 volume,
    s32 max_distance,
    s32 attenuation_distance)
{
    s32 delta_x = position->vx - audio_state.listener_position.vx;
    s32 delta_y = position->vy - audio_state.listener_position.vy;
    s32 delta_z = position->vz - audio_state.listener_position.vz;
    s32 distance_gain_q7;
    s32 attenuated_volume;
    s32 angle;
    s32 left;
    s32 right;

    const s32 listener_distance = fixed_vector3_length(delta_x, delta_y, delta_z);
    if (listener_distance >= max_distance) {
        return KF_AUDIO_NOT_PLAYED;
    }
    distance_gain_q7 = ((attenuation_distance - listener_distance) << KF_FIXED7_BITS) / attenuation_distance;
    attenuated_volume = (distance_gain_q7 * volume) >> KF_FIXED7_BITS;
    if (attenuated_volume < 0) {
        attenuated_volume = 0;
    } else if (attenuated_volume >= KF_AUDIO_MAX_VOLUME + 1) {
        attenuated_volume = KF_AUDIO_MAX_VOLUME;
    }
    angle = vector_xz_to_angle(
        position->vx - audio_state.listener_position.vx,
        audio_state.listener_position.vz - position->vz);
    angle = (angle - audio_state.listener_rotation.vy + KF_ANGLE_QUARTER_TURN)
        & KF_ANGLE_WRAP_MASK;
    if (angle >= KF_ANGLE_HALF_TURN) {
        angle = KF_ANGLE_FULL_TURN - angle;
    }
    angle >>= 1;
    // Retail compares the masked high bit to 1, so this branch never runs. The
    // port option tests it for nonzero instead (upstream sushi-shi #51).
    if (kf::fix_spatial_panning ? (sound->tone_and_flags & KF_SOUND_PAN_NARROWING_FLAG) != 0
                                : (sound->tone_and_flags & KF_SOUND_PAN_NARROWING_FLAG) == 1) {
        distance_gain_q7 += GAME_SOUND_PAN_NARROWING_GAIN_BOOST;
        if (distance_gain_q7 >= KF_AUDIO_MAX_VOLUME + 1) {
            distance_gain_q7 = KF_AUDIO_MAX_VOLUME;
        }
    }
    if (distance_gain_q7 >= GAME_SOUND_PAN_NARROW_THRESHOLD) {
        angle = (((angle - KF_ANGLE_EIGHTH_TURN)
            * (KF_FIXED7_ONE * 2 - distance_gain_q7 * 2)) >> KF_FIXED7_BITS)
            + KF_ANGLE_EIGHTH_TURN;
    }
    left = (attenuated_volume * kf::angle_sine(angle)) / GAME_SOUND_PAN_DIVISOR;
    if (left >= KF_AUDIO_MAX_VOLUME + 1) {
        left = KF_AUDIO_MAX_VOLUME;
    }
    right = (attenuated_volume * kf::angle_cosine(angle)) / GAME_SOUND_PAN_DIVISOR;
    if (right >= KF_AUDIO_MAX_VOLUME + 1) {
        right = KF_AUDIO_MAX_VOLUME;
    }
    audio_play_voice(audio_playback(),
        audio_state.bank,
        sound->program,
        sound->tone_and_flags & KF_SOUND_TONE_INDEX_MASK,
        sound->note,
        left,
        right);
    return KF_AUDIO_PLAYED;
}

void audio_reset_module_state(void)
{
    kf::restore_initial_value<audio_voice_slot_index>();
    kf::restore_initial_value<audio_state>();
}

KfAudioPlaybackResult audio_play_spatial_default_range(
    const SoundRef *sound,
    const VECTOR *position,
    s16 volume)
{
    return audio_play_spatial(sound, position, volume,
        KF_AUDIO_DEFAULT_MAX_DISTANCE, KF_AUDIO_DEFAULT_ATTENUATION_DISTANCE);
}

KfAudioPlaybackResult audio_play_spatial_range(
    const SoundRef *sound,
    const VECTOR *position,
    s16 volume,
    s32 max_distance,
    s32 attenuation_distance)
{
    return audio_play_spatial(
        sound,
        position,
        volume,
        max_distance,
        attenuation_distance);
}

KfAudioPlayback audio_playback()
{
    return {audio_state, audio_voice_slot_index, player_state.audio_effects_enabled != KF_PLAYER_OPTION_OFF};
}
