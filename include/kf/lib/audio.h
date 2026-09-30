#ifndef KF_AUDIO_H
#define KF_AUDIO_H

#include <kf/audio/sound.h>
#include <kf/lib/enum.h>
#include <kf/lib/geometry_types.h>
#include <kf/lib/sound_types.h>
#include <kf/lib/types.h>

#include <array>

inline constexpr int KF_AUDIO_VOICE_SLOTS = 10;
inline constexpr int KF_AUDIO_MAX_VOLUME = 0x7f;
inline constexpr int KF_AUDIO_DEFAULT_MAX_DISTANCE = 16000;
inline constexpr int KF_AUDIO_DEFAULT_ATTENUATION_DISTANCE = 28000;
inline constexpr int KF_AUDIO_EXTENDED_MAX_DISTANCE = 20000;
inline constexpr int KF_AUDIO_EXTENDED_ATTENUATION_DISTANCE = 60000;

enum class KfAudioSequenceState : s32 {
    KF_AUDIO_SEQUENCE_INACTIVE = 0,
    KF_AUDIO_SEQUENCE_ACTIVE = 1
}; using enum KfAudioSequenceState;

enum class KfAudioStopMode : s32 {
    KF_AUDIO_STOP_IMMEDIATE = 0,
    KF_AUDIO_STOP_FADE = 1
}; using enum KfAudioStopMode;

enum class KfAudioPlaybackResult : u32 {
    KF_AUDIO_NOT_PLAYED = 0,
    KF_AUDIO_PLAYED = 1
}; using enum KfAudioPlaybackResult;

typedef struct KfAudioVoiceSlots {
    std::array<kf::SoundVoice, KF_AUDIO_VOICE_SLOTS> voice_ids;
} KfAudioVoiceSlots;

typedef struct KfAudioState {
    kf::SoundBank *bank;
    u8 *sequence_buffer;
    kf::MusicSequence *sequence;
    KfAudioSequenceState sequence_active;
    VECTOR listener_position;
    SVECTOR listener_rotation;
    KfAudioVoiceSlots voice_slots;
} KfAudioState;

struct KfAudioBankResource {
    const u8 *header;
    std::size_t header_size;
    const u8 *body;
    std::size_t body_size;
};

struct KfAudioPlayback {
    KfAudioState &state;
    s32 &voice_slot_index;
    bool effects_enabled;
};

KfAudioBankResource audio_bank_resource(const u8 *data, std::size_t size);
void audio_release_sequence(KfAudioState &state);
void audio_close_vab(KfAudioState &state);
void audio_reset_voice_slots(KfAudioState &state);
void audio_set_listener_transform(KfAudioState &state,
    const VECTOR *position_or_null, const SVECTOR *rotation_or_null);
void sound_ref_key_off_bank0(KfAudioState &state, const SoundRef *sound);
void sound_ref_play(KfAudioPlayback playback, const SoundRef *sound, s16 volume);
void audio_play_voice(KfAudioPlayback playback,
    kf::SoundBank *bank, s16 program, s16 tone, s16 note, s16 left_volume, s16 right_volume);

#endif // KF_AUDIO_H
