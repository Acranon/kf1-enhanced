#ifndef KF_GAME_AUDIO_H
#define KF_GAME_AUDIO_H

#include <kf/lib/audio.h>

extern KfAudioState audio_state;
extern s32 audio_voice_slot_index;
KfAudioPlayback audio_playback();

extern void audio_initialize(void);
extern void audio_load_vab(KfAudioBankResource resource);
extern void audio_play_map_sequence(u8 sequence_index);
extern void audio_play_current_map_sequence(void);
extern KfAudioPlaybackResult audio_play_spatial(
    const SoundRef *sound, const VECTOR *position, s16 volume, s32 max_distance,
    s32 attenuation_distance);
extern KfAudioPlaybackResult audio_play_spatial_default_range(
    const SoundRef *sound, const VECTOR *position, s16 volume);
extern KfAudioPlaybackResult audio_play_spatial_range(
    const SoundRef *sound, const VECTOR *position, s16 volume,
    s32 max_distance, s32 attenuation_distance);
extern void audio_stop_sequence_fade(void);
extern void audio_stop_sequence_master_fade(s32 fade_step);

#endif // KF_GAME_AUDIO_H
