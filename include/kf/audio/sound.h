#ifndef KF_AUDIO_SOUND_H
#define KF_AUDIO_SOUND_H

#include <kf/lib/types.h>

#include <cstddef>

namespace kf {
struct SoundBank;
struct MusicSequence;
using SoundVoice = u32;
inline constexpr SoundVoice no_sound_voice = 0;
enum class ReverbPreset : u8 { Studio, Hall };

bool sound_start();
void sound_shutdown();
void sound_poll();
void sound_set_paused(bool paused);
// PC extension: while the game is paused, keep the music playing (on real time)
// and freeze only sound effects, which resume where they left off.
enum class SoundPause : u8 { None, KeepMusic, All };
void sound_set_pause(SoundPause pause);
void sound_set_output_gain(float gain);
void sound_reset(ReverbPreset preset, s16 depth_left, s16 depth_right);
void sound_master_volume(s16 left, s16 right);
SoundBank *sound_bank_load(const u8 *header, std::size_t header_size,
    const u8 *body, std::size_t body_size);
void sound_bank_release(SoundBank *bank);
SoundVoice sound_voice_play(SoundBank *bank, s16 program, s16 tone, s16 note, s16 left, s16 right);
void sound_voice_release(SoundVoice voice);
void sound_note_play(SoundBank *bank, s16 program, s16 note, s16 left, s16 right);
void sound_note_release(SoundBank *bank, s16 program, s16 note);
MusicSequence *sound_sequence_load(const u8 *data, std::size_t size, SoundBank *bank);
void sound_sequence_play(MusicSequence *sequence);
void sound_sequence_volume(MusicSequence *sequence, s16 left, s16 right);
void sound_sequence_release(MusicSequence *sequence);
}

#endif // KF_AUDIO_SOUND_H
