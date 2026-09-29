#ifndef KF_TMD_H
#define KF_TMD_H

#include <kf/lib/memory.h>
#include <kf/lib/render_types.h>

#include <kf/lib/types.h>
#include <kf/lib/enum.h>
#include <kf/renderer/projection.hpp>
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

enum class KfTmdMode : u8 {
    KF_TMD_MODE_NONE = 0,
    KF_TMD_MODE_SEMITRANS = 0x02,
    KF_TMD_MODE_F3 = 0x20,
    KF_TMD_MODE_FT3 = 0x24,
    KF_TMD_MODE_F4 = 0x28,
    KF_TMD_MODE_FT4 = 0x2c,
    KF_TMD_MODE_G3 = 0x30,
    KF_TMD_MODE_GT3 = 0x34,
    KF_TMD_MODE_G4 = 0x38,
    KF_TMD_MODE_GT4 = 0x3c
}; using enum KfTmdMode;
constexpr KfTmdMode operator|(KfTmdMode lhs, KfTmdMode rhs)
    { return static_cast<KfTmdMode>(static_cast<u8>(lhs) | static_cast<u8>(rhs)); }
    constexpr KfTmdMode operator&(KfTmdMode lhs, KfTmdMode rhs)
    { return static_cast<KfTmdMode>(static_cast<u8>(lhs) & static_cast<u8>(rhs)); }
    constexpr KfTmdMode operator^(KfTmdMode lhs, KfTmdMode rhs)
    { return static_cast<KfTmdMode>(static_cast<u8>(lhs) ^ static_cast<u8>(rhs)); }
    constexpr KfTmdMode operator~(KfTmdMode value)
    { return static_cast<KfTmdMode>(~static_cast<u8>(value)); }
    inline KfTmdMode& operator|=(KfTmdMode& lhs, KfTmdMode rhs) { return lhs = lhs | rhs; }
    inline KfTmdMode& operator&=(KfTmdMode& lhs, KfTmdMode rhs) { return lhs = lhs & rhs; }
    inline KfTmdMode& operator^=(KfTmdMode& lhs, KfTmdMode rhs) { return lhs = lhs ^ rhs; }

constexpr KfTmdMode tmd_packet_mode(u32 word)
{
    return kf_enum_decode<KfTmdMode>(word >> KF_TMD_MODE_SHIFT);
}
constexpr KfTmdMode tmd_packet_kind(u32 word)
{
    return kf_enum_decode<KfTmdMode>((word >> KF_TMD_MODE_SHIFT) & KF_TMD_MODE_MASK);
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
    u16 vertices[4];
    u16 normals[4];
    u16 uv[4];
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

#define TMD_PACKET_BODY(packet) ((packet) + KF_TMD_PACKET_HEADER_BYTES)
#define TMD_PACKET_BODY_BYTES(header) \
    (((header) >> KF_TMD_ILEN_TO_BYTES_SHIFT) & KF_TMD_BODY_BYTES_MASK)

typedef union KfTmdPacketHeader {
    u32 word;
    struct {
        u8 output_length;
        u8 input_length;
        u8 flag;
        KfTmdMode mode;
    } bytes;
} KfTmdPacketHeader;

typedef struct KfTmdF3 {
    u8 r;
    u8 g;
    u8 b;
    KfTmdMode mode;
    u16 n0;
    u16 v0;
    u16 v1;
    u16 v2;
} KfTmdF3;

typedef struct KfTmdG3 {
    u8 r;
    u8 g;
    u8 b;
    KfTmdMode mode;
    u16 n0;
    u16 v0;
    u16 n1;
    u16 v1;
    u16 n2;
    u16 v2;
} KfTmdG3;

typedef struct KfTmdF4 {
    u8 r;
    u8 g;
    u8 b;
    KfTmdMode mode;
    u16 n0;
    u16 v0;
    u16 v1;
    u16 v2;
    u16 v3;
    u16 pad;
} KfTmdF4;

typedef struct KfTmdG4 {
    u8 r;
    u8 g;
    u8 b;
    KfTmdMode mode;
    u16 n0;
    u16 v0;
    u16 n1;
    u16 v1;
    u16 n2;
    u16 v2;
    u16 n3;
    u16 v3;
} KfTmdG4;

typedef struct KfTmdFt3 {
    u8 tu0;
    u8 tv0;
    u16 cba;
    u8 tu1;
    u8 tv1;
    u16 tsb;
    u8 tu2;
    u8 tv2;
    u16 pad;
    u16 n0;
    u16 v0;
    u16 v1;
    u16 v2;
} KfTmdFt3;

typedef struct KfTmdGt3 {
    u8 tu0;
    u8 tv0;
    u16 cba;
    u8 tu1;
    u8 tv1;
    u16 tsb;
    u8 tu2;
    u8 tv2;
    u16 pad;
    u16 n0;
    u16 v0;
    u16 n1;
    u16 v1;
    u16 n2;
    u16 v2;
} KfTmdGt3;

typedef struct KfTmdFt4 {
    u8 tu0;
    u8 tv0;
    u16 cba;
    u8 tu1;
    u8 tv1;
    u16 tsb;
    u8 tu2;
    u8 tv2;
    u16 pad0;
    u8 tu3;
    u8 tv3;
    u16 pad1;
    u16 n0;
    u16 v0;
    u16 v1;
    u16 v2;
    u16 v3;
    u16 pad2;
} KfTmdFt4;

typedef struct KfTmdGt4 {
    u8 tu0;
    u8 tv0;
    u16 cba;
    u8 tu1;
    u8 tv1;
    u16 tsb;
    u8 tu2;
    u8 tv2;
    u16 pad0;
    u8 tu3;
    u8 tv3;
    u16 pad1;
    u16 n0;
    u16 v0;
    u16 n1;
    u16 v1;
    u16 n2;
    u16 v2;
    u16 n3;
    u16 v3;
} KfTmdGt4;

typedef union KfTmdPrimitive {
    KfTmdF3 f3;
    KfTmdF4 f4;
    KfTmdFt3 ft3;
    KfTmdFt4 ft4;
    KfTmdG3 g3;
    KfTmdG4 g4;
    KfTmdGt3 gt3;
    KfTmdGt4 gt4;
    CVECTOR color;
    struct {
        u16 uv0;
        u16 clut;
        u16 uv1;
        u16 tpage;
        u16 uv2;
        u16 pad0;
        u16 uv3;
        u16 pad1;
    } texture;
} KfTmdPrimitive;

typedef union KfScreenXY {
    s32 word;
    DVECTOR vector;
} KfScreenXY;

typedef struct KfScreenVertex {
    KfScreenXY sxy;
    s16 sz;
    s16 p2;
    // PC extension (see kf::ProjectedPoint): sub-pixel position and camera depth.
    // w = 0 marks orthographic vertices, which are drawn without perspective.
    float fx, fy, w;
    float cx, cy, cz;   // camera space (frame smoothing); cz = 0: not re-projectable
} KfScreenVertex;

struct KfTmdContext {
    std::span<KfTmdResource> slots;
    KfTmdResource &current_asset;
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

extern void tmd_project_vertices_shift(KfTmdContext context, s32 count, u8 shift,
    const MATRIX *model, const kf::Projection &projection);
extern void tmd_transform_vertices(KfTmdContext context, s32 count, const MATRIX *model);

#endif
