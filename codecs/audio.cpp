#include <kf/audio/codec.h>

#include "bytes.h"

#include <algorithm>
#include <optional>

namespace {
using namespace kf::codec;

constexpr std::size_t vab_header_bytes = 32, vab_program_bytes = 16, vab_tone_bytes = 32;
constexpr std::size_t vab_tone_table = vab_header_bytes + KF_AUDIO_PROGRAM_COUNT * vab_program_bytes;
constexpr u8 tone_reverb = 1 << 2;

struct AdpcmHeader {
    s32 positive, negative;
    u8 shift;
    bool end, repeat, loop_start;
};

AdpcmHeader adpcm_header(Bytes bytes)
{
    Reader reader(bytes);
    const auto predictor_shift = reader.byte(), flags = reader.byte();
    constexpr u8 end = 1, repeat = 2, loop_start = 4;
    require((flags & ~(end | repeat | loop_start)) == 0, "unknown ADPCM flags");
    constexpr std::array<std::pair<s32, s32>, 5> filters {{{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}}};
    const auto filter = field<4, 4>(predictor_shift);
    require(filter < filters.size(), "invalid ADPCM filter");
    const auto [positive, negative] = filters[filter];
    const u8 shift = field<0, 4>(predictor_shift);
    return {positive, negative, shift <= 12 ? shift : u8(9),
        bool(flags & end), bool(flags & repeat), bool(flags & loop_start)};
}

KfAudioTone read_tone(Reader tone, u16 program, u16 sample_count)
{
    KfAudioTone result {};
    result.priority = tone.byte();
    const auto flags = tone.byte();
    require((flags & ~tone_reverb) == 0, "unknown VAB tone flags");
    result.reverb = bool(flags & tone_reverb);
    result.volume = tone.byte();
    result.pan = tone.byte();
    // This is a pitch reference. Retail banks use center notes above MIDI 127.
    result.center_note = tone.byte();
    result.center_shift = tone.byte();
    result.minimum_note = tone.byte();
    result.maximum_note = tone.byte();
    result.vibrato_width = tone.byte();
    result.vibrato_time = tone.byte();
    result.portamento_width = tone.byte();
    result.portamento_time = tone.byte();
    result.bend_down = tone.byte();
    result.bend_up = tone.byte();
    tone.skip(2);
    const auto adsr1 = tone.u16_le(), adsr2 = tone.u16_le();
    const auto encoded_program = tone.s16_le(), sample = tone.s16_le();
    tone.skip(8);
    require(encoded_program == program && sample >= 1 && sample <= sample_count &&
        result.volume <= 127 && result.pan <= 127 && result.center_shift <= 127 &&
        result.minimum_note <= result.maximum_note && result.maximum_note <= 127, "invalid VAB tone");
    result.sample_index = sample - 1;
    result.envelope = {
        field<10, 5>(adsr1), field<8, 2>(adsr1), field<4, 4>(adsr1),
        field<8, 5>(adsr2), field<6, 2>(adsr2), field<0, 5>(adsr2),
        static_cast<u16>(std::min((u32(field<0, 4>(adsr1)) + 1) * 2048, 32767u)),
        field<15, 1>(adsr1), field<15, 1>(adsr2), field<14, 1>(adsr2), field<5, 1>(adsr2)};
    return result;
}

u32 variable_length(Reader &input)
{
    u32 result = 0;
    for (u8 i = 0; i < 4; ++i) {
        const auto byte = input.byte();
        result = (result << 7) | (byte & 127);
        if (!(byte & 0x80))
            return result;
    }
    throw Error {KF_CODEC_INVALID, "SEQ variable-length value exceeds four bytes", std::source_location::current()};
}

u8 midi_data(Reader &input)
{
    const auto value = input.byte();
    require(value < 128, "invalid SEQ data byte");
    return value;
}
} // namespace

KfCodecResult kf_audio_bank_decode(std::span<const u8> header, std::span<const u8> body,
    KfAudioBankData &output)
{
    return decode([&] {
        Reader input(header);
        require(input.u32_le() == 0x56414270, "invalid VAB magic");
        input.skip(8);
        const auto file_size = input.u32_le();
        input.skip(2);
        const auto programs = input.u16_le(), tones = input.u16_le(), samples = input.u16_le();
        const auto volume = input.byte(), pan = input.byte();
        input.skip(6);
        require(programs <= KF_AUDIO_PROGRAM_COUNT && tones <= programs * KF_AUDIO_TONE_COUNT &&
            samples < KF_AUDIO_SAMPLE_COUNT, "invalid VAB counts");
        const std::size_t tone_bytes = programs * KF_AUDIO_TONE_COUNT * vab_tone_bytes;
        require(header.size() == vab_tone_table + tone_bytes + KF_AUDIO_SAMPLE_COUNT * 2, "VAB header size mismatch");
        require(header.size() <= file_size && body.size() == file_size - header.size(), "VAB file size mismatch");
        require(volume <= 127 && pan <= 127, "invalid VAB volume or pan");

        // Initialize the existing allocation; a bank is too large for a browser stack temporary.
        output.volume = volume;
        output.pan = pan;
        output.sample_count = samples;
        std::ranges::fill(output.programs, KfAudioProgram {});
        output.samples = {};
        u16 packed_program = 0;
        for (u16 slot = 0; slot < KF_AUDIO_PROGRAM_COUNT; ++slot) {
            Reader program(input.take(vab_program_bytes));
            const auto count = program.byte();
            if (count == 0)
                continue;
            auto &target = output.programs[slot];
            target.tone_count = count;
            target.volume = program.byte();
            target.priority = program.byte();
            program.skip(1);
            target.pan = program.byte();
            require(count <= KF_AUDIO_TONE_COUNT && target.volume <= 127 && target.pan <= 127 &&
                packed_program < programs, "invalid VAB program");
            auto table = reader_at(header, vab_tone_table + packed_program * KF_AUDIO_TONE_COUNT * vab_tone_bytes);
            for (u8 ordinal = 0; ordinal < count; ++ordinal)
                target.tones[ordinal] = read_tone(Reader(table.take(vab_tone_bytes)), slot, samples);
            ++packed_program;
        }
        require(packed_program == programs, "VAB program count mismatch");
        auto offsets = reader_at(header, vab_tone_table + tone_bytes);
        require(offsets.u16_le() == 0, "invalid VAB first sample offset");
        u32 body_offset = 0;
        for (u16 sample = 0; sample < samples; ++sample) {
            const u32 size = u32(offsets.u16_le()) * 8;
            output.samples[sample] = {body_offset, size};
            // At most 255 samples of 65535 eight-byte units fit in u32.
            body_offset += size;
        }
        require(body_offset == body.size(), "VAB body size mismatch");
    });
}

KfCodecResult kf_audio_sample_info(std::span<const u8> data, KfAudioSampleInfo &info)
{
    return decode([&] {
        require(!data.empty() && data.size() % KF_AUDIO_ADPCM_BLOCK_BYTES == 0 &&
            data.size() / KF_AUDIO_ADPCM_BLOCK_BYTES <= UINT32_MAX / KF_AUDIO_ADPCM_BLOCK_FRAMES,
            "invalid ADPCM size");
        Reader input(data);
        u32 frames = 0, loop_begin = 0;
        while (input.remaining()) {
            const auto header = adpcm_header(input.take(KF_AUDIO_ADPCM_BLOCK_BYTES));
            if (header.loop_start)
                loop_begin = frames;
            frames += KF_AUDIO_ADPCM_BLOCK_FRAMES;
            if (header.end) {
                info = {frames, loop_begin, header.repeat ? frames : 0};
                return;
            }
        }
        info = {frames, 0, 0};
    });
}

KfCodecResult kf_audio_decode_block(std::span<const u8> data,
    KfAudioPredictor &predictor, std::span<s16> pcm)
{
    return decode([&] {
        require(data.size() == KF_AUDIO_ADPCM_BLOCK_BYTES, "invalid ADPCM block size");
        output_fits(pcm.size() >= KF_AUDIO_ADPCM_BLOCK_FRAMES);
        const auto header = adpcm_header(data);
        require(predictor.previous >= INT16_MIN && predictor.previous <= INT16_MAX &&
            predictor.older >= INT16_MIN && predictor.older <= INT16_MAX, "invalid ADPCM predictor");
        for (std::size_t n = 0; n < KF_AUDIO_ADPCM_BLOCK_FRAMES; ++n) {
            const s32 nibble = field<0, 4>(data[2 + n / 2] >> ((n % 2) * 4));
            const s32 signed_nibble = nibble < 8 ? nibble : nibble - 16;
            const auto value = std::clamp(((signed_nibble * 4096) >> header.shift) +
                ((predictor.previous * header.positive + predictor.older * header.negative + 32) >> 6),
                s32(INT16_MIN), s32(INT16_MAX));
            pcm[n] = static_cast<s16>(value);
            predictor.older = predictor.previous;
            predictor.previous = value;
        }
    });
}

KfCodecResult kf_music_decode(std::span<const u8> data,
    std::vector<KfMusicEvent> &events, KfMusicInfo &info)
{
    return decode([&] {
        require(data.size() <= UINT32_MAX, "SEQ exceeds format size limit");
        Reader input(data);
        require(input.u32_le() == 0x53455170, "invalid SEQ magic");
        const auto version = input.u32_be();
        const auto resolution = input.u16_be();
        const auto tempo = input.u24_be();
        input.skip(2);
        require(version == 1 && resolution != 0 && tempo != 0, "unsupported SEQ header");
        std::vector<KfMusicEvent> decoded;
        std::optional<u8> running_status;
        std::uint64_t duration = 0;
        while (input.remaining()) {
            KfMusicEvent event {};
            event.delta = variable_length(input);
            duration += event.delta;
            const auto first = input.byte();
            u8 status = first;
            std::optional<u8> first_data;
            if (first < 128) {
                require(running_status.has_value(), "missing SEQ running status");
                status = *running_status;
                first_data = first;
            }
            if (status >= 0x80 && status <= 0xef) {
                running_status = status;
                event.channel = field<0, 4>(status);
                const auto message = field<4, 4>(status);
                const auto data1 = first_data ? *first_data : midi_data(input);
                const u8 data2 = message == 0xc || message == 0xd ? 0 : midi_data(input);
                switch (message) {
                case 0x8: case 0x9:
                    event.kind = message == 0x9 && data2 != 0 ? KfMusicEventKind::NoteOn : KfMusicEventKind::NoteOff;
                    event.note = data1;
                    event.velocity = data2;
                    break;
                case 0xb:
                    require(data1 == 7, "unsupported SEQ controller");
                    event.kind = KfMusicEventKind::Volume;
                    event.value = data2;
                    break;
                case 0xc:
                    event.kind = KfMusicEventKind::Program;
                    event.value = data1;
                    break;
                case 0xe:
                    event.kind = KfMusicEventKind::PitchBend;
                    event.value = u32(data1) | (u32(data2) << 7);
                    break;
                default: require(false, "unsupported SEQ channel message");
                }
            } else {
                require(status == 0xff, "unsupported SEQ event");
                running_status.reset();
                const auto type = input.byte();
                const auto length = variable_length(input);
                Reader payload(input.take(length));
                if (type == 0x51 && length == 3) {
                    event.kind = KfMusicEventKind::Tempo;
                    event.value = payload.u24_be();
                    require(event.value != 0, "zero SEQ tempo");
                } else {
                    require(type == 0x2f && length == 0, "unsupported SEQ event");
                    event.kind = KfMusicEventKind::End;
                }
            }
            decoded.push_back(event);
            if (event.kind == KfMusicEventKind::End) {
                require(duration != 0, "zero SEQ duration");
                info = {resolution, tempo};
                events = std::move(decoded);
                return;
            }
        }
        require(false, "SEQ is missing an end event");
    });
}
