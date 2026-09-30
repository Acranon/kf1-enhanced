#include <kf/platform/prelude.h>
#include <kf/lib/audio.h>

static constexpr std::size_t sound_chunk_header_bytes = 4;

static u32 audio_chunk_size(const u8 *data)
{
    return static_cast<u32>(data[0]) | (static_cast<u32>(data[1]) << 8)
        | (static_cast<u32>(data[2]) << 16) | (static_cast<u32>(data[3]) << 24);
}

KfAudioBankResource audio_bank_resource(const u8 *data, std::size_t size)
{
    if (size < sound_chunk_header_bytes)
        kf::host_fail("Truncated sound-bank resource");
    const std::size_t header_size = audio_chunk_size(data);
    if (header_size > size - sound_chunk_header_bytes || size - sound_chunk_header_bytes - header_size < sound_chunk_header_bytes)
        kf::host_fail("Truncated sound-bank header");
    const u8 *body_chunk = data + sound_chunk_header_bytes + header_size;
    const std::size_t body_size = audio_chunk_size(body_chunk);
    if (body_size > size - 2 * sound_chunk_header_bytes - header_size)
        kf::host_fail("Truncated sound-bank samples");
    return {data + sound_chunk_header_bytes, header_size, body_chunk + sound_chunk_header_bytes, body_size};
}

void audio_release_sequence(KfAudioState &state)
{
    kf::sound_sequence_release(state.sequence);
    state.sequence = nullptr;
    state.sequence_active = KF_AUDIO_SEQUENCE_INACTIVE;
}

void sound_ref_key_off_bank0(KfAudioState &state, const SoundRef *sound)
{
    kf::sound_note_release(state.bank, sound->program, sound->note);
}

void audio_set_listener_transform(KfAudioState &state,
    const VECTOR *position_or_null,
    const SVECTOR *rotation_or_null)
{
    if (position_or_null != NULL) {
        state.listener_position = *position_or_null;
    }
    if (rotation_or_null != NULL) {
        state.listener_rotation = *rotation_or_null;
    }
}

void sound_ref_play(KfAudioPlayback playback, const SoundRef *sound, s16 volume)
{
    audio_play_voice(playback,
        playback.state.bank,
        sound->program,
        sound->tone_and_flags,
        sound->note,
        volume,
        volume);
}

void audio_close_vab(KfAudioState &state)
{
    audio_release_sequence(state);
    kf::sound_bank_release(state.bank);
    state.bank = nullptr;
}

void audio_reset_voice_slots(KfAudioState &state)
{
    state.voice_slots.voice_ids.fill(kf::no_sound_voice);
}

void audio_play_voice(KfAudioPlayback playback,
    kf::SoundBank *bank, s16 program, s16 tone, s16 note, s16 left_volume, s16 right_volume)
{
    if (program == 0 && tone == 0 && note == 0)
        return;
    if (!playback.effects_enabled)
        return;
    auto &index = playback.voice_slot_index;
    if (++index == KF_AUDIO_VOICE_SLOTS)
        index = 0;
    auto &voice = playback.state.voice_slots.voice_ids[index];
    kf::sound_voice_release(voice);
    voice = kf::sound_voice_play(bank, program, tone, note, left_volume, right_volume);
}
