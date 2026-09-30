#ifndef KF_CUTSCENE_AUDIO_H
#define KF_CUTSCENE_AUDIO_H

#include <kf/lib/audio.h>

extern KfAudioState cutscene_audio_state;
extern s32 cutscene_audio_voice_slot_index;
KfAudioPlayback cutscene_audio_playback();

extern void cutscene_audio_initialize(void);
extern void cutscene_audio_load_vab(KfAudioBankResource resource);
extern void audio_play_sequence_file(const char *path);
extern KfAudioPlaybackResult cutscene_audio_play_spatial(
    const SoundRef *sound, const VECTOR *position, s16 volume, s32 max_distance,
    s32 attenuation_distance);
extern void audio_stop_sequence(KfAudioStopMode stop_mode);

#endif // KF_CUTSCENE_AUDIO_H
