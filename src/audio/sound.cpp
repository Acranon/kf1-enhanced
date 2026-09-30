#include <kf/audio/codec.h>
#include <kf/audio/sound.h>
#include <kf/platform/host.h>

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <vector>

namespace kf {
constexpr unsigned sample_rate = 44100;
constexpr unsigned voice_count = 24;
constexpr unsigned channel_count = 16;
constexpr unsigned buffered_frames = 4096;
constexpr unsigned mixer_batch_frames = 256;
constexpr unsigned reverb_comb_capacity = 2048;
constexpr unsigned reverb_diffuser_capacity = 600;
constexpr unsigned reverb_stereo_spread_frames = 23;
constexpr unsigned reverb_first_diffuser_frames = 225;
constexpr unsigned reverb_second_diffuser_frames = 556;
constexpr float reverb_comb_mix_gain = 0.25f;
constexpr float reverb_diffuser_feedback = 0.5f;
constexpr float reverb_hall_feedback = 0.82f;
constexpr float reverb_studio_feedback = 0.7f;
constexpr float reverb_damping = 0.3f;
constexpr int midi_value_max = 127;
constexpr float midi_value_scale = 127.0f;
constexpr int pan_center = 64;
constexpr float pan_half_range = 64.0f;
constexpr int pitch_bend_center = 64;
constexpr double pitch_bend_half_range = 64.0;
constexpr double pitch_shift_units_per_semitone = 128.0;
constexpr double semitones_per_octave = 12.0;
constexpr double maximum_playback_rate = 4.0;
constexpr double microseconds_per_second = 1000000.0;
constexpr std::uint64_t nanoseconds_per_second = 1000000000;
constexpr unsigned envelope_infinite_shift = 31;
constexpr unsigned envelope_infinite_step = 3;
constexpr unsigned envelope_rate_shift_pivot = 11;
constexpr s32 envelope_rising_step_base = 7;
constexpr s32 envelope_falling_step_base = -8;
constexpr s32 envelope_exponential_attack_threshold = 0x6000;
constexpr unsigned envelope_fraction_bits = 15;
constexpr u32 envelope_counter_period = 32768;
constexpr s32 envelope_level_max = 32767;
constexpr float envelope_level_scale = 32768.0f;
constexpr float pcm16_scale = 32768.0f;

struct SoundBank {
    KfAudioBankData data;
    std::array<KfAudioSampleInfo, KF_AUDIO_SAMPLE_COUNT> samples;
    std::vector<u8> body;
    unsigned references;
    SoundBank *next;
};
struct MusicChannel { u8 program, volume; };
struct MusicSequence {
    KfMusicInfo info;
    std::vector<KfMusicEvent> events;
    SoundBank *bank;
    MusicSequence *next;
    std::array<MusicChannel, channel_count> channels;
    std::size_t cursor;
    double frames_until_event;
    u32 tempo;
    float left, right;
    bool playing;
};
enum class EnvelopePhase : u8 { Off, Attack, Decay, Sustain, Release };
struct Envelope {
    EnvelopePhase phase;
    s32 level;
    u32 counter;
};
struct Voice {
    SoundVoice id;
    SoundBank *bank;
    MusicSequence *sequence;
    const KfAudioTone *tone;
    Envelope envelope;
    KfAudioPredictor predictor;
    std::array<s16, KF_AUDIO_ADPCM_BLOCK_FRAMES> decoded;
    unsigned decoded_cursor, block_offset;
    float current_sample, next_sample;
    double fraction, rate;
    float left, right;
    u8 program, note, velocity, priority;
    bool current_valid, next_valid;
    std::uint64_t age;
};
struct Comb {
    std::array<float, reverb_comb_capacity> values;
    unsigned cursor, length;
    float filtered;
};
struct Diffuser {
    std::array<float, reverb_diffuser_capacity> values;
    unsigned cursor, length;
};
struct Reverb {
    std::array<std::array<Comb, 4>, 2> combs;
    std::array<std::array<Diffuser, 2>, 2> diffusers;
    float feedback, damping, left, right;
};
struct SoundState {
    SDL_AudioStream *stream;
    SoundBank *banks;
    MusicSequence *sequences;
    std::array<Voice, voice_count> voices;
    SoundVoice next_voice;
    std::uint64_t age;
    std::uint64_t rendered_frame;
    Reverb reverb;
    float master_left, master_right;
    bool failed, paused;
    // SoundPause::KeepMusic: effects are frozen and the mix runs on real time,
    // which the game clock excludes; music_frames keeps that time afterwards.
    bool effects_frozen;
    std::uint64_t music_frames, music_since_ns;
};
static SoundState sound;

static float volume(s16 value) { return std::clamp<int>(value, 0, midi_value_max) / midi_value_scale; }
static void pan_levels(float &left, float &right, u8 pan) {
    if (pan < pan_center)
        right *= pan / pan_half_range;
    else
        left *= (midi_value_max - pan) / pan_half_range;
}
static void bank_unref(SoundBank *bank) {
    if (--bank->references != 0)
        return;
    auto **link = &sound.banks;
    while (*link && *link != bank)
        link = &(*link)->next;
    if (*link)
        *link = bank->next;
    delete bank;
}

static void release(Voice &voice) {
    if (voice.envelope.phase != EnvelopePhase::Off && voice.envelope.phase != EnvelopePhase::Release) {
        voice.envelope.phase = EnvelopePhase::Release;
        voice.envelope.counter = 0;
    }
}

static float envelope_frame(Voice &voice) {
    auto &state = voice.envelope;
    const auto &parameters = voice.tone->envelope;
    unsigned shift = 0, step_bits = 0;
    bool exponential = false, decreasing = false, infinite = false;
    switch (state.phase) {
    case EnvelopePhase::Off: return 0;
    case EnvelopePhase::Attack:
        shift = parameters.attack_shift;
        step_bits = parameters.attack_step;
        exponential = parameters.attack_exponential;
        infinite = shift == envelope_infinite_shift && step_bits == envelope_infinite_step;
        break;
    case EnvelopePhase::Decay:
        shift = parameters.decay_shift;
        exponential = decreasing = true;
        break;
    case EnvelopePhase::Sustain:
        shift = parameters.sustain_shift;
        step_bits = parameters.sustain_step;
        exponential = parameters.sustain_exponential;
        decreasing = parameters.sustain_decreasing;
        infinite = shift == envelope_infinite_shift && step_bits == envelope_infinite_step;
        break;
    case EnvelopePhase::Release:
        shift = parameters.release_shift;
        exponential = parameters.release_exponential;
        decreasing = true;
        infinite = shift == envelope_infinite_shift;
        break;
    }
    s32 step = decreasing ? envelope_falling_step_base + static_cast<s32>(step_bits) : envelope_rising_step_base - static_cast<s32>(step_bits);
    step *= 1 << (shift < envelope_rate_shift_pivot ? envelope_rate_shift_pivot - shift : 0);
    u32 increment = envelope_counter_period >> (shift > envelope_rate_shift_pivot ? shift - envelope_rate_shift_pivot : 0);
    if (exponential && !decreasing && state.level > envelope_exponential_attack_threshold) {
        if (shift < envelope_rate_shift_pivot - 1)
            step >>= 2;
        else if (shift >= envelope_rate_shift_pivot)
            increment >>= 2;
        else {
            step >>= 1;
            increment >>= 1;
        }
    } else if (exponential && decreasing) {
        step = (step * state.level) >> envelope_fraction_bits;
    }
    if (!infinite)
        increment = std::max(increment, 1u);
    state.counter += increment;
    if (state.counter >= envelope_counter_period) {
        state.counter &= envelope_counter_period - 1;
        state.level = std::clamp(state.level + step, 0, envelope_level_max);
        if (state.phase == EnvelopePhase::Attack && state.level == envelope_level_max) {
            state.phase = EnvelopePhase::Decay;
            state.counter = 0;
        } else if (state.phase == EnvelopePhase::Decay && state.level <= parameters.sustain_level) {
            state.phase = EnvelopePhase::Sustain;
            state.counter = 0;
        } else if (state.phase == EnvelopePhase::Release && state.level == 0) {
            state.phase = EnvelopePhase::Off;
        }
    }
    return state.level / envelope_level_scale;
}

static bool read_sample(Voice &voice, float &sample) {
    const auto index = voice.tone->sample_index;
    const auto &info = voice.bank->samples[index];
    const auto &range = voice.bank->data.samples[index];
    if (voice.decoded_cursor == KF_AUDIO_ADPCM_BLOCK_FRAMES) {
        if (voice.block_offset == info.frames / KF_AUDIO_ADPCM_BLOCK_FRAMES * KF_AUDIO_ADPCM_BLOCK_BYTES) {
            if (!info.loop_end)
                return false;
            voice.block_offset = info.loop_begin / KF_AUDIO_ADPCM_BLOCK_FRAMES * KF_AUDIO_ADPCM_BLOCK_BYTES;
        }
        // The validated, immutable block remains owned by the bank. Only the
        // voice's position jumps at a repeat; its two-sample history continues.
        if (kf_audio_decode_block({voice.bank->body.data() + range.offset + voice.block_offset,
                KF_AUDIO_ADPCM_BLOCK_BYTES},
                voice.predictor, voice.decoded) != KF_CODEC_OK) {
            sound.failed = true;
            return false;
        }
        voice.block_offset += KF_AUDIO_ADPCM_BLOCK_BYTES;
        voice.decoded_cursor = 0;
    }
    sample = voice.decoded[voice.decoded_cursor++] / pcm16_scale;
    return true;
}

static void voice_pitch(Voice &voice, int bend = pitch_bend_center) {
    double semitones = static_cast<int>(voice.note) - voice.tone->center_note
        + voice.tone->center_shift / pitch_shift_units_per_semitone;
    bend -= pitch_bend_center;
    semitones += bend * (bend < 0 ? voice.tone->bend_down : voice.tone->bend_up) / pitch_bend_half_range;
    voice.rate = std::clamp(std::exp2(semitones / semitones_per_octave), 0.0, maximum_playback_rate);
}

static SoundVoice play_tone(SoundBank *bank, unsigned program, unsigned tone_index, unsigned note,
    float left, float right, MusicSequence *sequence = nullptr, u8 velocity = midi_value_max) {
    if (!bank || program >= KF_AUDIO_PROGRAM_COUNT || note > midi_value_max
            || tone_index >= bank->data.programs[program].tone_count)
        return no_sound_voice;
    const auto &program_data = bank->data.programs[program];
    const auto &tone = program_data.tones[tone_index];
    Voice *chosen = nullptr;
    for (auto &voice : sound.voices) {
        if (voice.envelope.phase == EnvelopePhase::Off) {
            chosen = &voice;
            break;
        }
        if (!chosen || voice.priority < chosen->priority
                || (voice.priority == chosen->priority && voice.age < chosen->age))
            chosen = &voice;
    }
    auto &voice = *chosen;
    voice = {};
    do { ++sound.next_voice; } while (!sound.next_voice);
    voice.id = sound.next_voice;
    voice.bank = bank;
    voice.sequence = sequence;
    voice.tone = &tone;
    voice.program = program;
    voice.note = note;
    voice.velocity = velocity;
    voice.priority = std::max(tone.priority, program_data.priority);
    voice.age = ++sound.age;
    voice.envelope.phase = EnvelopePhase::Attack;
    voice.decoded_cursor = KF_AUDIO_ADPCM_BLOCK_FRAMES;
    voice.current_valid = read_sample(voice, voice.current_sample);
    voice.next_valid = read_sample(voice, voice.next_sample);
    voice_pitch(voice);
    const float gain = bank->data.volume / midi_value_scale * program_data.volume / midi_value_scale * tone.volume / midi_value_scale;
    voice.left = left * gain;
    voice.right = right * gain;
    // Authored tone and program pans attenuate in series; opposing pans do
    // not cancel into a centered, full-volume voice. The VAB header pan is unused.
    pan_levels(voice.left, voice.right, tone.pan);
    pan_levels(voice.left, voice.right, program_data.pan);
    if (sequence)
        pan_levels(voice.left, voice.right, pan_center);
    return voice.id;
}

static void play_note(SoundBank *bank, unsigned program, unsigned note, float left, float right,
    MusicSequence *sequence = nullptr, u8 velocity = midi_value_max) {
    if (!bank || program >= KF_AUDIO_PROGRAM_COUNT || note > midi_value_max)
        return;
    const auto &program_data = bank->data.programs[program];
    for (std::size_t i = 0; i < program_data.tone_count; ++i) {
        const auto &tone = program_data.tones[i];
        if (note >= tone.minimum_note && note <= tone.maximum_note)
            play_tone(bank, program, i, note, left, right, sequence, velocity);
    }
}

static double event_frames(const MusicSequence &sequence, u32 delta) {
    return static_cast<double>(delta) * sequence.tempo * sample_rate / (sequence.info.resolution * microseconds_per_second);
}

static void sequence_defaults(MusicSequence &sequence) {
    for (std::size_t i = 0; i < sequence.channels.size(); ++i)
        sequence.channels[i] = {static_cast<u8>(i), midi_value_max};
    sequence.tempo = sequence.info.tempo;
}

static void sequence_frame(MusicSequence &sequence) {
    if (!sequence.playing)
        return;
    std::size_t processed = 0;
    while (sequence.frames_until_event <= 0) {
        if (++processed > sequence.events.size() + 1) {
            sound.failed = true;
            sequence.playing = false;
            return;
        }
        const auto &event = sequence.events[sequence.cursor++];
        auto &channel = sequence.channels[event.channel];
        switch (event.kind) {
        case KfMusicEventKind::NoteOff:
            for (auto &voice : sound.voices)
                if (voice.sequence == &sequence
                        && voice.program == channel.program && voice.note == event.note)
                    release(voice);
            break;
        case KfMusicEventKind::NoteOn:
            play_note(sequence.bank, channel.program, event.note, 1, 1, &sequence,
                event.velocity * channel.volume / midi_value_max);
            break;
        case KfMusicEventKind::Volume: channel.volume = event.value; break;
        case KfMusicEventKind::Program: channel.program = event.value; break;
        case KfMusicEventKind::PitchBend:
            for (auto &voice : sound.voices)
                if (voice.sequence == &sequence && voice.program == channel.program)
                    voice_pitch(voice, event.value >> 7);
            break;
        case KfMusicEventKind::Tempo: sequence.tempo = event.value; break;
        case KfMusicEventKind::End:
            // The original callers request indefinite playback of the score.
            sequence.cursor = 0;
            break;
        }
        sequence.frames_until_event += event_frames(sequence, sequence.events[sequence.cursor].delta);
    }
    sequence.frames_until_event -= 1;
}

static float reverb_channel(unsigned channel, float input) {
    auto &effect = sound.reverb;
    float sum = 0;
    for (auto &comb : effect.combs[channel]) {
        const float delayed = comb.values[comb.cursor];
        comb.filtered = delayed * (1 - effect.damping) + comb.filtered * effect.damping;
        comb.values[comb.cursor] = input + comb.filtered * effect.feedback;
        comb.cursor = (comb.cursor + 1) % comb.length;
        sum += delayed * reverb_comb_mix_gain;
    }
    for (auto &diffuser : effect.diffusers[channel]) {
        const float delayed = diffuser.values[diffuser.cursor];
        diffuser.values[diffuser.cursor] = sum + delayed * reverb_diffuser_feedback;
        sum = delayed - sum;
        diffuser.cursor = (diffuser.cursor + 1) % diffuser.length;
    }
    return sum;
}

static void mix_frame(float &left, float &right) {
    left = right = 0;
    if (sound.paused || sound.failed)
        return;
    for (auto *sequence = sound.sequences; sequence; sequence = sequence->next)
        sequence_frame(*sequence);
    float send_left = 0, send_right = 0;
    for (auto &voice : sound.voices) {
        if (voice.envelope.phase == EnvelopePhase::Off)
            continue;
        if (sound.effects_frozen && !voice.sequence)
            continue;   // a sound effect, held until the game resumes
        if (!voice.current_valid) {
            voice.envelope.phase = EnvelopePhase::Off;
            continue;
        }
        const float source = voice.current_sample + (voice.next_sample - voice.current_sample) * voice.fraction;
        const float gain = envelope_frame(voice);
        float voice_left = voice.left, voice_right = voice.right;
        if (voice.sequence) {
            voice_left *= voice.sequence->left * (voice.velocity / midi_value_scale);
            voice_right *= voice.sequence->right * (voice.velocity / midi_value_scale);
        }
        // The authored volume scale is quadratic, with full voice level 0x3fff.
        // Master volume remains a separate linear gain after the voice mix.
        constexpr float voice_level = 16383.0f / 32768.0f;
        const float out_left = source * gain * voice_left * voice_left * voice_level;
        const float out_right = source * gain * voice_right * voice_right * voice_level;
        left += out_left;
        right += out_right;
        if (voice.tone->reverb) {
            send_left += out_left;
            send_right += out_right;
        }
        voice.fraction += voice.rate;
        while (voice.fraction >= 1) {
            voice.fraction -= 1;
            voice.current_sample = voice.next_sample;
            voice.current_valid = voice.next_valid;
            voice.next_sample = 0;
            voice.next_valid = read_sample(voice, voice.next_sample);
        }
    }
    left = std::clamp((left + reverb_channel(0, send_left) * sound.reverb.left) * sound.master_left, -1.0f, 1.0f);
    right = std::clamp((right + reverb_channel(1, send_right) * sound.reverb.right) * sound.master_right, -1.0f, 1.0f);
}

static std::uint64_t frames_of(std::uint64_t ns) {
    return ns / nanoseconds_per_second * sample_rate + ns % nanoseconds_per_second * sample_rate / nanoseconds_per_second;
}

static std::uint64_t clock_frame() {
    std::uint64_t frames = frames_of(host_clock_ns()) + sound.music_frames;
    if (sound.effects_frozen)
        frames += frames_of(SDL_GetTicksNS() - sound.music_since_ns);
    return frames;
}

static void render_elapsed() {
    if (sound.paused || sound.failed)
        return;
    const auto target = clock_frame();
    const auto keep_from = target > buffered_frames ? target - buffered_frames : 0;
    const auto queued = SDL_GetAudioStreamQueued(sound.stream);
    if (queued < 0) {
        sound.failed = true;
        return;
    }
    // A stalled main thread must not leave playback permanently seconds behind.
    // Advance all elapsed state, but retain only recent samples after starvation.
    if (sound.rendered_frame < keep_from || queued > static_cast<int>(buffered_frames * 2 * sizeof(float) * 2)) {
        if (!SDL_ClearAudioStream(sound.stream)) {
            sound.failed = true;
            return;
        }
    }
    std::array<float, mixer_batch_frames * 2> frames;
    while (sound.rendered_frame < target) {
        const unsigned count = std::min<std::uint64_t>(target - sound.rendered_frame, mixer_batch_frames);
        for (std::size_t i = 0; i < count; ++i)
            mix_frame(frames[i * 2], frames[i * 2 + 1]);
        const auto skip = sound.rendered_frame < keep_from
            ? std::min<std::uint64_t>(keep_from - sound.rendered_frame, count) : 0;
        const int bytes = (count - skip) * sizeof(float) * 2;
        if (bytes && !SDL_PutAudioStreamData(sound.stream, frames.data() + skip * 2, bytes)) {
            sound.failed = true;
            return;
        }
        sound.rendered_frame += count;
    }
}

static bool prime_stream() {
    // Device consumption trails the shared clock; never mix future game state.
    // Normal host waits service the stream at most eight milliseconds apart.
    static const std::array<float, buffered_frames * 2> silence{};
    sound.rendered_frame = clock_frame();
    return SDL_PutAudioStreamData(sound.stream, silence.data(), silence.size() * sizeof(float));
}

static void configure_reverb(ReverbPreset preset, s16 left, s16 right) {
    sound.reverb = {};
    constexpr std::array<unsigned, 4> studio = {970, 1116, 1277, 1356};
    constexpr std::array<unsigned, 4> hall = {1557, 1617, 1491, 1422};
    for (std::size_t side = 0; side < sound.reverb.combs.size(); ++side) {
        for (std::size_t i = 0; i < sound.reverb.combs[side].size(); ++i)
            sound.reverb.combs[side][i].length = (preset == ReverbPreset::Hall ? hall[i] : studio[i]) + side * reverb_stereo_spread_frames;
        sound.reverb.diffusers[side][0].length = reverb_first_diffuser_frames + side * reverb_stereo_spread_frames;
        sound.reverb.diffusers[side][1].length = reverb_second_diffuser_frames + side * reverb_stereo_spread_frames;
    }
    sound.reverb.feedback = preset == ReverbPreset::Hall ? reverb_hall_feedback : reverb_studio_feedback;
    sound.reverb.damping = reverb_damping;
    sound.reverb.left = volume(left);
    sound.reverb.right = volume(right);
}

bool sound_start() {
    const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, sample_rate};
    configure_reverb(ReverbPreset::Studio, 0, 0);
    sound.master_left = sound.master_right = 1;
    sound.failed = sound.paused = false;
    // The main thread renders elapsed samples before applying each command.
    // Device-sized demand callbacks can otherwise swallow a one-frame note gate.
    sound.stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!sound.stream || !prime_stream() || !SDL_ResumeAudioStreamDevice(sound.stream)) {
        std::fprintf(stderr, "Audio: %s\n", SDL_GetError());
        if (sound.stream)
            SDL_DestroyAudioStream(sound.stream);
        sound.stream = nullptr;
        return false;
    }
    return true;
}

static void sequence_release(MusicSequence *sequence) {
    for (auto &voice : sound.voices)
        if (voice.sequence == sequence)
            voice = {};
    auto **link = &sound.sequences;
    while (*link && *link != sequence)
        link = &(*link)->next;
    if (*link)
        *link = sequence->next;
    bank_unref(sequence->bank);
    delete sequence;
}

static void clear() {
    sound.voices = {};
    while (sound.sequences)
        sequence_release(sound.sequences);
    while (sound.banks)
        bank_unref(sound.banks);
    SDL_ClearAudioStream(sound.stream);
}

void sound_shutdown() {
    if (!sound.stream)
        return;
    clear();
    SDL_DestroyAudioStream(sound.stream);
    sound.stream = nullptr;
}

void sound_poll() {
    if (!sound.stream)
        return;
    render_elapsed();
    if (sound.failed)
        host_fail("Audio playback failed");
}

void sound_set_paused(bool paused) {
    sound_set_pause(paused ? SoundPause::All : SoundPause::None);
}

void sound_set_pause(SoundPause pause) {
    if (!sound.stream)
        return;
    sound_poll();
    // Fold real time spent with music playing into the audio clock, so it
    // continues without a jump when the game clock resumes.
    if (sound.effects_frozen) {
        sound.music_frames += frames_of(SDL_GetTicksNS() - sound.music_since_ns);
        sound.effects_frozen = false;
    }
    if (pause == SoundPause::KeepMusic) {
        sound.effects_frozen = true;
        sound.music_since_ns = SDL_GetTicksNS();
    }
    const bool paused = pause == SoundPause::All;
    if (paused == sound.paused)
        return;
    sound.paused = paused;
    if (!(paused ? SDL_PauseAudioStreamDevice(sound.stream) : SDL_ResumeAudioStreamDevice(sound.stream)))
        host_fail("Cannot pause or resume audio playback");
}

// Port extension: player volume, applied after the game's own mix.
void sound_set_output_gain(float gain) {
    if (sound.stream)
        SDL_SetAudioStreamGain(sound.stream, gain);
}

void sound_reset(ReverbPreset preset, s16 depth_left, s16 depth_right) {
    sound_poll();
    clear();
    configure_reverb(preset, depth_left, depth_right);
    sound.master_left = sound.master_right = 1;
    if (!prime_stream())
        host_fail("Cannot initialize audio playback");
}

void sound_master_volume(s16 left, s16 right) {
    sound_poll();
    sound.master_left = volume(left);
    sound.master_right = volume(right);
}

SoundBank *sound_bank_load(const u8 *header, std::size_t header_size, const u8 *body, std::size_t body_size) try {
    auto bank = std::make_unique<SoundBank>();
    if (kf_audio_bank_decode({header, header_size}, {body, body_size}, bank->data) != KF_CODEC_OK)
        return nullptr;
    for (std::size_t i = 0; i < bank->data.sample_count; ++i) {
        const auto &range = bank->data.samples[i];
        if (kf_audio_sample_info({body + range.offset, range.size}, bank->samples[i]) != KF_CODEC_OK)
            return nullptr;
    }
    if (body_size > bank->body.max_size())
        return nullptr;
    bank->body.assign(body, body + body_size);
    bank->references = 1;
    sound_poll();
    bank->next = sound.banks;
    sound.banks = bank.release();
    return sound.banks;
} catch (const std::bad_alloc &) {
    return nullptr;
}

void sound_bank_release(SoundBank *bank) {
    if (!bank)
        return;
    sound_poll();
    for (auto &voice : sound.voices)
        if (voice.bank == bank)
            voice = {};
    for (auto *sequence = sound.sequences; sequence; sequence = sequence->next)
        if (sequence->bank == bank)
            sequence->playing = false;
    bank_unref(bank);
}

SoundVoice sound_voice_play(SoundBank *bank, s16 program, s16 tone, s16 note, s16 left, s16 right) {
    sound_poll();
    const auto id = play_tone(bank, program, tone, note, volume(left), volume(right));
    return id;
}

void sound_voice_release(SoundVoice id) {
    if (!id)
        return;
    sound_poll();
    for (auto &voice : sound.voices)
        if (voice.id == id)
            release(voice);
}

void sound_note_play(SoundBank *bank, s16 program, s16 note, s16 left, s16 right) {
    sound_poll();
    play_note(bank, program, note, volume(left), volume(right));
}

void sound_note_release(SoundBank *bank, s16 program, s16 note) {
    sound_poll();
    for (auto &voice : sound.voices)
        if (!voice.sequence && voice.bank == bank && voice.program == program && voice.note == note)
            release(voice);
}

MusicSequence *sound_sequence_load(const u8 *data, std::size_t size, SoundBank *bank) try {
    if (!bank)
        return nullptr;
    auto sequence = std::make_unique<MusicSequence>();
    if (kf_music_decode({data, size}, sequence->events, sequence->info) != KF_CODEC_OK)
        return nullptr;
    sequence->bank = bank;
    sequence->left = sequence->right = 1;
    sound_poll();
    ++bank->references;
    sequence->next = sound.sequences;
    sound.sequences = sequence.release();
    return sound.sequences;
} catch (const std::bad_alloc &) {
    return nullptr;
} catch (const std::length_error &) {
    return nullptr;
}

void sound_sequence_play(MusicSequence *sequence) {
    if (!sequence)
        return;
    sound_poll();
    for (auto &voice : sound.voices)
        if (voice.sequence == sequence)
            voice = {};
    sequence_defaults(*sequence);
    sequence->cursor = 0;
    sequence->frames_until_event = event_frames(*sequence, sequence->events[0].delta);
    sequence->playing = true;
}

void sound_sequence_volume(MusicSequence *sequence, s16 left, s16 right) {
    if (!sequence)
        return;
    sound_poll();
    sequence->left = volume(left);
    sequence->right = volume(right);
}

void sound_sequence_release(MusicSequence *sequence) {
    if (!sequence)
        return;
    sound_poll();
    sequence_release(sequence);
}
}
