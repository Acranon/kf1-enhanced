#ifndef KF_TMD_H
#define KF_TMD_H

#include <kf/lib/enum.h>
#include <kf/lib/memory.h>
#include <kf/lib/render_types.h>
#include <kf/lib/types.h>
#include <kf/renderer/projection.h>

#include <array>
#include <span>

enum class KfTmdSlot : u16 {
    KF_TMD_SLOT_MAP = 0,
    KF_TMD_SLOT_ENTITIES = 1,
    KF_TMD_SLOT_MENU_ITEM = 4
}; using enum KfTmdSlot;

enum {
    KF_TMD_HEADER_BYTES = 12,
    KF_TMD_PACKET_HEADER_BYTES = 4,
    KF_TMD_WORD_BYTES = 4,
    KF_TMD_ILEN_BYTE = 1,
    KF_TMD_MODE_SHIFT = 24,
    KF_TMD_MODE_MASK = 0xfd,
    KF_TMD_ILEN_TO_BYTES_SHIFT = 6,
    KF_TMD_BODY_BYTES_MASK = 0x3fc,
    KF_TMD_DEFAULT_PERSPECTIVE_SHIFT = 1
};

// Whole encoded polygon modes, not combinable flags.
enum class KfTmdMode : u8 {
    KF_TMD_MODE_F3 = 0x20,
    KF_TMD_MODE_F3_SEMITRANS = 0x22,
    KF_TMD_MODE_FT3 = 0x24,
    KF_TMD_MODE_FT3_SEMITRANS = 0x26,
    KF_TMD_MODE_F4 = 0x28,
    KF_TMD_MODE_F4_SEMITRANS = 0x2a,
    KF_TMD_MODE_FT4 = 0x2c,
    KF_TMD_MODE_FT4_SEMITRANS = 0x2e,
    KF_TMD_MODE_G3 = 0x30,
    KF_TMD_MODE_G3_SEMITRANS = 0x32,
    KF_TMD_MODE_GT3 = 0x34,
    KF_TMD_MODE_GT3_SEMITRANS = 0x36,
    KF_TMD_MODE_G4 = 0x38,
    KF_TMD_MODE_G4_SEMITRANS = 0x3a,
    KF_TMD_MODE_GT4 = 0x3c,
    KF_TMD_MODE_GT4_SEMITRANS = 0x3e
}; using enum KfTmdMode;

constexpr KfTmdMode tmd_opaque_mode(KfTmdMode mode)
{
    return kf_enum_decode<KfTmdMode>(kf_enum_encode<u8>(mode) & KF_TMD_MODE_MASK);
}

constexpr KfTmdMode tmd_packet_mode(u32 word)
{
    return kf_enum_decode<KfTmdMode>(word >> KF_TMD_MODE_SHIFT);
}
constexpr KfTmdMode tmd_packet_kind(u32 word)
{
    return tmd_opaque_mode(tmd_packet_mode(word));
}

typedef struct KfTmdHeader {
    u32 id;
    u32 flags;
    u32 object_count;
} KfTmdHeader;

// Borrowed resource storage; selection copies the pointer and its extent together.
struct KfTmdResource {
    KfTmdHeader *data;
    std::size_t size;
};

struct KfTmdBytes {
    const u8 *data;
    std::size_t size;
};

struct KfTmdPrimitiveStream {
    KfTmdBytes bytes;
    u32 remaining;
};

struct KfTmdPacket {
    KfTmdMode mode;
    KfTmdBytes body;
};

// Decoded values, not a view of a serialized packet or a GPU command.
struct KfTmdFaceData {
    std::array<u16, 4> vertices;
    std::array<u16, 4> normals;
    std::array<u16, 4> uv;
    u16 texture_page;
    u16 palette;
    CVECTOR color;
};

typedef struct KfTmdObject {
    u32 vertex_offset;
    u32 vertex_count;
    u32 normal_offset;
    u32 normal_count;
    u32 primitive_offset;
    u32 primitive_count;
    s32 scale;
} KfTmdObject;

static_assert(sizeof(KfTmdHeader) == KF_TMD_HEADER_BYTES && sizeof(KfTmdObject) == 28);
static_assert(offsetof(KfTmdHeader, object_count) == 8);
static_assert(offsetof(KfTmdObject, vertex_offset) == 0 && offsetof(KfTmdObject, vertex_count) == 4);
static_assert(offsetof(KfTmdObject, normal_offset) == 8 && offsetof(KfTmdObject, normal_count) == 12);
static_assert(offsetof(KfTmdObject, primitive_offset) == 16 && offsetof(KfTmdObject, primitive_count) == 20);
static_assert(offsetof(KfTmdObject, scale) == 24);

inline u32 tmd_read_word(const u8 *bytes)
{
    return static_cast<u32>(bytes[0]) | (static_cast<u32>(bytes[1]) << 8)
        | (static_cast<u32>(bytes[2]) << 16) | (static_cast<u32>(bytes[3]) << 24);
}

typedef struct KfScreenVertex {
    DVECTOR position;
    s16 sz;
    s16 p2;
    // PC extension (see kf::ProjectedPoint): sub-pixel position and camera depth.
    // w = 0 marks orthographic vertices, which are drawn without perspective.
    float fx, fy, w;
    float cx, cy, cz;   // camera space (frame smoothing); cz = 0: not re-projectable
} KfScreenVertex;

struct KfTmdContext {
    std::span<KfTmdResource> slots;
    KfTmdResource &current_tmd;
    SVECTOR *&current_vertices;
    std::span<KfScreenVertex, KF_PROJECTED_VERTEX_CAPACITY> projected_vertices;
};

// TMD vertex/normal indices remain file element indices; projected storage may
// change independently without rewriting the loaded resource.

extern KfTmdObject *tmd_get_object(KfTmdContext context, u16 object_index);
extern KfTmdObject tmd_read_object(KfTmdContext context, u16 object_index);
extern KfTmdPrimitiveStream tmd_primitive_stream(KfTmdContext context, const KfTmdObject &object);
extern KfTmdPacket tmd_next_packet(KfTmdPrimitiveStream &stream);
extern KfTmdFaceData tmd_decode_face(const KfTmdPacket &packet, u32 vertex_count);
extern KfTmdBytes tmd_normal_bytes(KfTmdContext context, const KfTmdObject &object);
extern SVECTOR tmd_read_normal(KfTmdBytes normals, u16 index);
extern KfTmdResource tmd_resource_view(u8 *data, std::size_t size);
extern void tmd_register(KfTmdContext context, KfTmdSlot slot, u8 *data, std::size_t size);
extern void tmd_release_last_allocation(KfTmdContext context, KfMemoryArena &arena, KfTmdSlot slot);
extern void tmd_select(KfTmdContext context, KfTmdSlot slot);
extern void tmd_select_object_vertices(KfTmdContext context, u16 object_index);
extern void tmd_set_current_vertices(KfTmdContext context, SVECTOR *vertices);

extern void tmd_project_vertices_depth_shift(KfTmdContext context, s32 count, u8 depth_shift,
    const MATRIX *model, const kf::Projection &projection);
extern void tmd_transform_vertices(KfTmdContext context, s32 count, const MATRIX *model);

#endif // KF_TMD_H
