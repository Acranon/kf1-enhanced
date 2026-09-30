#include <kf/lib/codec.h>
#include <kf/audio/codec.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <memory>
#include <vector>

static void word(std::vector<uint8_t> &bytes, size_t at, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) bytes[at + i] = value >> (8 * i);
}

static void audio_records()
{
    std::vector<uint8_t> header(32 + 128 * 16 + 16 * 32 + 256 * 2);
    std::array<uint8_t, 8> body {};
    word(header, 0, 0x56414270); word(header, 4, 7);
    word(header, 12, header.size() + body.size());
    word(header, 16, 1u << 16); word(header, 20, (1u << 16) | 1);
    header[24] = 99; header[25] = 63;
    header[32] = 1; header[33] = 79; header[34] = 5; header[36] = 62;
    const size_t tone = 32 + 128 * 16;
    header[tone + 2] = 89; header[tone + 4] = 60;
    header[tone + 5] = 64; header[tone + 7] = 127;
    word(header, tone + 20, 1u << 16);
    header[tone + 16 * 32 + 2] = 1;
    auto bank = std::make_unique<KfAudioBankData>();
    for (size_t size = 0; size < 32; ++size)
        assert(kf_audio_bank_decode({header.data(), size}, body, *bank) == KF_CODEC_INVALID);
    header.insert(header.begin(), 0);
    assert(kf_audio_bank_decode({header.data() + 1, header.size() - 1}, body, *bank) == KF_CODEC_OK);
    assert(bank->volume == 99 && bank->pan == 63 && bank->sample_count == 1);
    assert(bank->programs[0].volume == 79 && bank->programs[0].priority == 5);
    assert(bank->programs[0].tones[0].volume == 89 && bank->programs[0].tones[0].sample_index == 0);
    assert(bank->samples[0].offset == 0 && bank->samples[0].size == body.size());
    word(header, 1 + tone + 16, 0xb52fdba7);
    assert(kf_audio_bank_decode({header.data() + 1, header.size() - 1}, body, *bank) == KF_CODEC_OK);
    const auto &envelope = bank->programs[0].tones[0].envelope;
    assert(envelope.attack_shift == 22 && envelope.attack_step == 3 && envelope.decay_shift == 10);
    assert(envelope.sustain_shift == 21 && envelope.sustain_step == 0 && envelope.release_shift == 15);
    assert(envelope.sustain_level == 16384 && envelope.attack_exponential == 1);
    assert(envelope.sustain_exponential == 1 && envelope.sustain_decreasing == 0 && envelope.release_exponential == 1);
    word(header, 1 + tone + 16, 0xffffffff);
    assert(kf_audio_bank_decode({header.data() + 1, header.size() - 1}, body, *bank) == KF_CODEC_OK);
    assert(envelope.attack_shift == 31 && envelope.attack_step == 3 && envelope.decay_shift == 15);
    assert(envelope.sustain_shift == 31 && envelope.sustain_step == 3 && envelope.release_shift == 31);
    assert(envelope.sustain_level == 32767 && envelope.sustain_decreasing == 1);

    // An odd source address, big-endian resolution and a three-byte tempo.
    const std::array<uint8_t, 20> sequence {
        0, 'p', 'Q', 'E', 'S', 0, 0, 0, 1, 0x01, 0x23,
        0x01, 0x23, 0x45, 4, 2, 1, 0xff, 0x2f, 0,
    };
    KfMusicInfo info {};
    std::vector<KfMusicEvent> events;
    for (size_t size = 0; size < sequence.size() - 1; ++size)
        assert(kf_music_decode({sequence.data() + 1, size}, events, info) == KF_CODEC_INVALID);
    assert(kf_music_decode({sequence.data() + 1, sequence.size() - 1}, events, info) == KF_CODEC_OK);
    assert(info.resolution == 0x0123 && info.tempo == 0x012345 && events.size() == 1);
    assert(events[0].kind == KfMusicEventKind::End && events[0].delta == 1);
    auto unsupported = sequence;
    unsupported[17] = 0xa0;
    assert(kf_music_decode({unsupported.data() + 1, unsupported.size() - 1}, events, info) == KF_CODEC_INVALID);
}

static void tim_records()
{
    std::vector<uint8_t> tim(24);
    word(tim, 0, 0x10); word(tim, 4, 2); word(tim, 8, 16); word(tim, 16, (1u << 16) | 2);
    KfTimInfo info {};
    assert(kf_tim_info(tim, 0, info) == KF_CODEC_OK);
    assert(kf_tim_info({tim.data(), tim.size() - 1}, 0, info) == KF_CODEC_INVALID);
    assert(kf_tim_info(tim, tim.size(), info) == KF_CODEC_END);
    std::array<uint8_t, 8> rgba {};
    assert(kf_tim_rgba(tim, 0, 0, {rgba.data(), rgba.size() - 1}) == KF_CODEC_OUTPUT_FULL);
    for (uint32_t mode : {0u, 1u, 2u}) {
        word(tim, 4, mode);
        assert(kf_tim_info(tim, 0, info) == KF_CODEC_OK);
        assert(info.mode == mode && info.width == (8u >> mode) && info.height == 1);
    }
    for (uint32_t mode : {4u, 7u, 16u, 0xffffffffu}) {
        word(tim, 4, mode);
        assert(kf_tim_info(tim, 0, info) == KF_CODEC_INVALID);
    }
    word(tim, 4, 2);
    word(tim, 16, 0x0001ffff);
    assert(kf_tim_info(tim, 0, info) == KF_CODEC_INVALID);
    word(tim, 16, 0xffff0001);
    assert(kf_tim_info(tim, 0, info) == KF_CODEC_INVALID);
    tim.resize(28);
    word(tim, 4, 3); word(tim, 8, 20); word(tim, 12, 0xfffcfffd); word(tim, 16, 0x00010003);
    assert(kf_tim_info(tim, 0, info) == KF_CODEC_OK);
    assert(info.width == 2 && info.height == 1 && info.encoded_bytes == tim.size());
    assert(info.image_x == -3 && info.image_y == -4);
}

static void adpcm_headers()
{
    std::array<uint8_t, KF_AUDIO_ADPCM_BLOCK_BYTES> block {};
    std::fill(block.begin() + 2, block.end(), 0x87);
    std::array<int16_t, KF_AUDIO_ADPCM_BLOCK_FRAMES> pcm {};
    constexpr int32_t filters[][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};
    KfAudioSampleInfo info {};
    for (uint8_t filter = 0; filter < 5; ++filter) {
        for (uint8_t shift = 0; shift < 16; ++shift) {
            block[0] = (filter << 4) | shift;
            for (uint8_t flags = 0; flags < 8; ++flags) {
                block[1] = flags;
                KfAudioPredictor predictor {1000, -500};
                assert(kf_audio_sample_info(block, info) == KF_CODEC_OK);
                assert(info.frames == pcm.size() && info.loop_end == ((flags & 3) == 3 ? pcm.size() : 0));
                assert(kf_audio_decode_block(block, predictor, pcm) == KF_CODEC_OK);
                int32_t previous = 1000, older = -500;
                for (size_t i = 0; i < pcm.size(); ++i) {
                    const int32_t signed_nibble = i % 2 == 0 ? 7 : -8;
                    const int32_t value = std::clamp(
                        ((signed_nibble * 4096) >> (shift <= 12 ? shift : 9))
                            + ((previous * filters[filter][0] + older * filters[filter][1] + 32) >> 6),
                        -32768, 32767);
                    assert(pcm[i] == value);
                    older = previous;
                    previous = value;
                }
                assert(predictor.previous == previous && predictor.older == older);
            }
        }
    }
    for (uint8_t flags : {8, 16, 32, 64, 128, 255}) {
        block[0] = 0; block[1] = flags;
        KfAudioPredictor predictor {};
        assert(kf_audio_sample_info(block, info) == KF_CODEC_INVALID);
        assert(kf_audio_decode_block(block, predictor, pcm) == KF_CODEC_INVALID);
    }
    for (uint8_t filter = 5; filter < 16; ++filter) {
        block[0] = filter << 4; block[1] = 0;
        KfAudioPredictor predictor {};
        assert(kf_audio_sample_info(block, info) == KF_CODEC_INVALID);
        assert(kf_audio_decode_block(block, predictor, pcm) == KF_CODEC_INVALID);
    }
}

static void music_events()
{
    const std::vector<u8> sequence {
        'p', 'Q', 'E', 'S', 0, 0, 0, 1, 0, 96, 7, 0xa1, 0x20, 4, 2,
        0, 0xc2, 5, 1, 6, // Program changes, including running status.
        0, 0x92, 60, 99, 2, 60, 0, // Note-on; zero velocity becomes note-off.
        3, 0xb2, 7, 80, 4, 0xe2, 127, 63,
        0, 0xff, 0x51, 3, 7, 0xa1, 0x20,
        1, 0xff, 0x2f, 0,
    };
    std::vector<KfMusicEvent> events;
    KfMusicInfo info {};
    assert(kf_music_decode(sequence, events, info) == KF_CODEC_OK);
    assert(events.size() == 8 && info.resolution == 96 && info.tempo == 500000);
    assert(events[0].kind == KfMusicEventKind::Program && events[0].value == 5 && events[0].channel == 2);
    assert(events[1].kind == KfMusicEventKind::Program && events[1].value == 6 && events[1].delta == 1);
    assert(events[2].kind == KfMusicEventKind::NoteOn && events[2].note == 60 && events[2].velocity == 99);
    assert(events[3].kind == KfMusicEventKind::NoteOff && events[3].velocity == 0);
    assert(events[4].kind == KfMusicEventKind::Volume && events[4].value == 80);
    assert(events[5].kind == KfMusicEventKind::PitchBend && events[5].value == 8191);
    assert(events[6].kind == KfMusicEventKind::Tempo && events[6].value == 500000);
    assert(events[7].kind == KfMusicEventKind::End && events[7].delta == 1);
    for (const std::vector<u8> &tail : {
            std::vector<u8>{0, 60, 0}, // Missing running status.
            std::vector<u8>{0x80, 0x80, 0x80, 0x80, 0}, // Oversized VLQ.
            std::vector<u8>{0, 0x90, 128, 0}, // Status byte used as note data.
            std::vector<u8>{0, 0xff, 0x51, 3, 0, 0, 0}, // Zero tempo.
            std::vector<u8>{0, 0xff, 0x2f, 0}, // Zero duration.
            std::vector<u8>{0, 0xc0, 1, 0, 0xff, 0x51, 3, 7, 0xa1, 0x20, 1, 2}, // Meta resets running status.
        }) {
        std::vector<u8> bad(sequence.begin(), sequence.begin() + 15);
        bad.insert(bad.end(), tail.begin(), tail.end());
        assert(kf_music_decode(bad, events, info) == KF_CODEC_INVALID);
        assert(events.size() == 8 && events[5].value == 8191); // Owned output stays intact.
    }
}

static void tim_pixels()
{
    constexpr std::array<u8, 16> expected {255, 0, 0, 255, 0, 255, 0, 255,
        0, 0, 255, 255, 0, 0, 0, 0};
    for (u32 mode : {0u, 1u}) {
        const u32 colors = mode == 0 ? 16 : 256;
        const u32 clut_size = 12 + colors * 4;
        const std::size_t image = 8 + clut_size;
        std::vector<u8> tim(image + 16);
        word(tim, 0, 0x10); word(tim, 4, mode | 8); word(tim, 8, clut_size);
        word(tim, 12, (511u << 16) | 1023); word(tim, 16, (2u << 16) | colors);
        word(tim, 20 + colors * 2, (0x03e0u << 16) | 0x001f);
        word(tim, 24 + colors * 2, 0x7c00);
        word(tim, image, 16); word(tim, image + 4, (20u << 16) | 10);
        word(tim, image + 8, (1u << 16) | (mode == 0 ? 1 : 2));
        word(tim, image + 12, mode == 0 ? 0x3210 : 0x03020100);
        std::array<u8, 16> rgba {};
        assert(kf_tim_rgba(tim, 0, 1, rgba) == KF_CODEC_OK && rgba == expected);
        assert(kf_tim_rgba(tim, 0, 2, rgba) == KF_CODEC_INVALID);
        std::vector<u16> words(1024 * 512);
        assert(kf_tim_compose(tim, words) == KF_CODEC_OK);
        assert(words[1023] == 0x001f && words[0] == 0x03e0 && words[1] == 0x7c00);
        // A later malformed image must not partially overwrite the texture.
        word(tim, image + 8, 0xffffffff);
        std::fill(words.begin(), words.end(), 42);
        assert(kf_tim_compose(tim, words) == KF_CODEC_INVALID);
        assert(std::ranges::all_of(words, [](u16 value) { return value == 42; }));
    }
    std::vector<u8> direct(28);
    word(direct, 0, 0x10); word(direct, 4, 2); word(direct, 8, 20);
    word(direct, 16, (1u << 16) | 4);
    word(direct, 20, (0x03e0u << 16) | 0x001f); word(direct, 24, 0x7c00);
    std::array<u8, 16> rgba {};
    assert(kf_tim_rgba(direct, 0, 0, rgba) == KF_CODEC_OK && rgba == expected);
    word(direct, 4, 3); word(direct, 16, (1u << 16) | 3);
    const std::array<u8, 6> rgb {1, 2, 3, 4, 5, 6};
    std::ranges::copy(rgb, direct.begin() + 20);
    assert(kf_tim_rgba(direct, 0, 0, rgba) == KF_CODEC_OK);
    assert((std::array<u8, 8>{rgba[0], rgba[1], rgba[2], rgba[3], rgba[4], rgba[5], rgba[6], rgba[7]} ==
        std::array<u8, 8>{1, 2, 3, 255, 4, 5, 6, 255}));
}

int main()
{
    audio_records();
    tim_records();
    adpcm_headers();
    music_events();
    tim_pixels();
}
