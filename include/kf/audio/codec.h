#ifndef KF_AUDIO_CODEC_H
#define KF_AUDIO_CODEC_H

#include <kf/lib/codec.h>

enum { KF_AUDIO_PROGRAM_COUNT = 128, KF_AUDIO_TONE_COUNT = 16, KF_AUDIO_SAMPLE_COUNT = 256 };
enum { KF_AUDIO_ADPCM_BLOCK_BYTES = 16, KF_AUDIO_ADPCM_BLOCK_FRAMES = 28 };
struct KfAudioEnvelope {
    u8 attack_shift, attack_step, decay_shift, sustain_shift, sustain_step, release_shift;
    u16 sustain_level;
    u8 attack_exponential, sustain_exponential, sustain_decreasing, release_exponential;
};
struct KfAudioTone {
    u8 priority, reverb, volume, pan, center_note, center_shift, minimum_note, maximum_note;
    u8 vibrato_width, vibrato_time, portamento_width, portamento_time, bend_down, bend_up;
    u16 sample_index;
    KfAudioEnvelope envelope;
};
struct KfAudioProgram {
    u8 tone_count, volume, pan, priority;
    std::array<KfAudioTone, KF_AUDIO_TONE_COUNT> tones;
};
struct KfAudioSampleRange { u32 offset, size; };
struct KfAudioBankData {
    u8 volume, pan;
    u16 sample_count;
    std::array<KfAudioProgram, KF_AUDIO_PROGRAM_COUNT> programs;
    std::array<KfAudioSampleRange, KF_AUDIO_SAMPLE_COUNT> samples;
};
struct KfAudioSampleInfo {
    u32 frames, loop_begin, loop_end;
};
struct KfAudioPredictor { s32 previous, older; };

enum class KfMusicEventKind : u32 {
    NoteOff, NoteOn, Volume, Program, PitchBend, Tempo, End
};

struct KfMusicEvent {
    u32 delta;
    KfMusicEventKind kind;
    u32 value;
    u8 channel, note, velocity;
};
struct KfMusicInfo { u32 resolution, tempo; };

// Inputs and outputs are disjoint. No result retains input pointers.
KfCodecResult kf_audio_bank_decode(std::span<const u8> header, std::span<const u8> body,
    KfAudioBankData &output);
// Positions are mono frames; zero loop_end denotes a one-shot sample.
KfCodecResult kf_audio_sample_info(std::span<const u8> data, KfAudioSampleInfo &info);
// Zero predictor state at key-on, not at loop jumps. Decode 16 bytes to 28 frames.
KfCodecResult kf_audio_decode_block(std::span<const u8> data,
    KfAudioPredictor &predictor, std::span<s16> pcm);
KfCodecResult kf_music_decode(std::span<const u8> data,
    std::vector<KfMusicEvent> &events, KfMusicInfo &info);

#endif // KF_AUDIO_CODEC_H
