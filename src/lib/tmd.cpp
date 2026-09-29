#include <kf/platform/prelude.hpp>
#include <kf/lib/tmd.h>
#include <kf/lib/graphics.h>
#include <kf/lib/memory.h>

static KfTmdResource &tmd_slot(KfTmdContext context, KfTmdSlot slot)
{
    const auto slots = context.slots;
    const auto index = kf_enum_encode<u16>(slot);
    if (index >= slots.size())
        kf::host_fail("Invalid TMD slot");
    return slots[index];
}

KfTmdResource tmd_resource_view(u8 *data, std::size_t size)
{
    // Legacy typed consumers still read serialized words directly on these targets.
    static_assert(std::endian::native == std::endian::little);
    if (!data || size < KF_TMD_HEADER_BYTES ||
        reinterpret_cast<std::uintptr_t>(data) % alignof(KfTmdHeader))
        kf::host_fail("Truncated or unaligned TMD header");
    const auto objects = tmd_read_word(data + offsetof(KfTmdHeader, object_count));
    if (objects > (size - KF_TMD_HEADER_BYTES) / sizeof(KfTmdObject))
        kf::host_fail("Truncated TMD object table");
    return {reinterpret_cast<KfTmdHeader *>(data), size};
}

void tmd_select(KfTmdContext context, KfTmdSlot slot)
{
    const auto resource = tmd_slot(context, slot);
    if (!resource.data)
        kf::host_fail("Unregistered TMD slot");
    context.current_asset = resource;
}

static u8 *tmd_object_bytes(KfTmdContext context, u16 index)
{
    const auto resource = context.current_asset;
    if (!resource.data || resource.size < KF_TMD_HEADER_BYTES)
        kf::host_fail("No selected TMD resource");
    auto *bytes = reinterpret_cast<u8 *>(resource.data);
    if (index >= tmd_read_word(bytes + offsetof(KfTmdHeader, object_count)) ||
        index >= (resource.size - KF_TMD_HEADER_BYTES) / sizeof(KfTmdObject))
        kf::host_fail("TMD object index exceeds its resource");
    return bytes + KF_TMD_HEADER_BYTES + sizeof(KfTmdObject) * index;
}

KfTmdObject *tmd_get_object(KfTmdContext context, u16 index)
{
    return reinterpret_cast<KfTmdObject *>(tmd_object_bytes(context, index));
}

KfTmdObject tmd_read_object(KfTmdContext context, u16 index)
{
    const auto *bytes = tmd_object_bytes(context, index);
    return {tmd_read_word(bytes + offsetof(KfTmdObject, vertex_offset)),
        tmd_read_word(bytes + offsetof(KfTmdObject, vertex_count)),
        tmd_read_word(bytes + offsetof(KfTmdObject, normal_offset)),
        tmd_read_word(bytes + offsetof(KfTmdObject, normal_count)),
        tmd_read_word(bytes + offsetof(KfTmdObject, primitive_offset)),
        tmd_read_word(bytes + offsetof(KfTmdObject, primitive_count)),
        std::bit_cast<s32>(tmd_read_word(bytes + offsetof(KfTmdObject, scale)))};
}

static KfTmdBytes tmd_payload_from(KfTmdContext context, std::size_t offset)
{
    const auto resource = context.current_asset;
    if (!resource.data || resource.size < KF_TMD_HEADER_BYTES ||
        offset > resource.size - KF_TMD_HEADER_BYTES)
        kf::host_fail("TMD offset exceeds its resource");
    return {reinterpret_cast<const u8 *>(resource.data) + KF_TMD_HEADER_BYTES + offset,
        resource.size - KF_TMD_HEADER_BYTES - offset};
}

KfTmdPrimitiveStream tmd_primitive_stream(KfTmdContext context, const KfTmdObject &object)
{
    if (!object.primitive_count)
        return {};
    const auto bytes = tmd_payload_from(context, object.primitive_offset);
    if (object.primitive_count > bytes.size / KF_TMD_PACKET_HEADER_BYTES)
        kf::host_fail("TMD primitive count exceeds its resource");
    return {bytes, object.primitive_count};
}

KfTmdPacket tmd_next_packet(KfTmdPrimitiveStream &stream)
{
    if (!stream.remaining || stream.bytes.size < KF_TMD_PACKET_HEADER_BYTES)
        kf::host_fail("Truncated TMD primitive header");
    const auto *header = stream.bytes.data;
    const std::size_t body_size = header[KF_TMD_ILEN_BYTE] * KF_TMD_WORD_BYTES;
    if (body_size > stream.bytes.size - KF_TMD_PACKET_HEADER_BYTES)
        kf::host_fail("Truncated TMD primitive body");
    const KfTmdPacket packet {tmd_packet_mode(tmd_read_word(header)),
        {header + KF_TMD_PACKET_HEADER_BYTES, body_size}};
    stream.bytes.data += KF_TMD_PACKET_HEADER_BYTES + body_size;
    stream.bytes.size -= KF_TMD_PACKET_HEADER_BYTES + body_size;
    --stream.remaining;
    return packet;
}

static u16 tmd_read_halfword(const u8 *bytes)
{
    return static_cast<u16>(bytes[0]) | (static_cast<u16>(bytes[1]) << 8);
}

KfTmdFaceData tmd_decode_face(const KfTmdPacket &packet, u32 vertex_count)
{
    unsigned corners;
    bool textured;
    bool gouraud;
    // The caller's original mode switch decides which packets are rendered.
    switch (packet.mode & ~KF_TMD_MODE_SEMITRANS) {
    case KF_TMD_MODE_F3:  corners = 3; textured = false; gouraud = false; break;
    case KF_TMD_MODE_F4:  corners = 4; textured = false; gouraud = false; break;
    case KF_TMD_MODE_G3:  corners = 3; textured = false; gouraud = true;  break;
    case KF_TMD_MODE_G4:  corners = 4; textured = false; gouraud = true;  break;
    case KF_TMD_MODE_FT3: corners = 3; textured = true;  gouraud = false; break;
    case KF_TMD_MODE_FT4: corners = 4; textured = true;  gouraud = false; break;
    case KF_TMD_MODE_GT3: corners = 3; textured = true;  gouraud = true;  break;
    case KF_TMD_MODE_GT4: corners = 4; textured = true;  gouraud = true;  break;
    default: kf::host_fail("Unsupported TMD face layout");
    }
    // Textured faces start with one UV/control word per corner; others have RGB/code.
    const unsigned index_offset = textured ? corners * 4 : 4;
    const unsigned index_bytes = gouraud ? corners * 4 : (corners + 1) * 2;
    if (packet.body.size < index_offset + index_bytes)
        kf::host_fail("TMD primitive is shorter than its face layout");
    const auto *bytes = packet.body.data;
    KfTmdFaceData face {};
    if (textured) {
        for (unsigned i = 0; i < corners; ++i)
            face.uv[i] = tmd_read_halfword(bytes + i * 4);
        face.palette = tmd_read_halfword(bytes + 2);
        face.texture_page = tmd_read_halfword(bytes + 6);
    } else {
        face.color = {bytes[0], bytes[1], bytes[2], bytes[3]};
    }
    const auto *indices = bytes + index_offset;
    if (gouraud) {
        for (unsigned i = 0; i < corners; ++i) {
            face.normals[i] = tmd_read_halfword(indices + i * 4);
            face.vertices[i] = tmd_read_halfword(indices + i * 4 + 2);
        }
    } else {
        face.normals[0] = tmd_read_halfword(indices);
        for (unsigned i = 0; i < corners; ++i)
            face.vertices[i] = tmd_read_halfword(indices + 2 + i * 2);
    }
    for (unsigned i = 0; i < corners; ++i) {
        if (face.vertices[i] >= vertex_count || face.vertices[i] >= KF_PROJECTED_VERTEX_CAPACITY)
            kf::host_fail("TMD face vertex exceeds its projected array");
    }
    return face;
}

KfTmdBytes tmd_normal_bytes(KfTmdContext context, const KfTmdObject &object)
{
    if (!object.normal_count)
        return {};
    auto bytes = tmd_payload_from(context, object.normal_offset);
    constexpr unsigned normal_bytes = 8;
    if (object.normal_count > bytes.size / normal_bytes)
        kf::host_fail("TMD normals exceed their resource");
    bytes.size = static_cast<std::size_t>(object.normal_count) * normal_bytes;
    return bytes;
}

SVECTOR tmd_read_normal(KfTmdBytes normals, u16 index)
{
    constexpr unsigned normal_bytes = 8;
    if (index >= normals.size / normal_bytes)
        kf::host_fail("TMD face normal exceeds its array");
    const auto *bytes = normals.data + static_cast<std::size_t>(index) * normal_bytes;
    return {std::bit_cast<s16>(tmd_read_halfword(bytes)),
        std::bit_cast<s16>(tmd_read_halfword(bytes + 2)),
        std::bit_cast<s16>(tmd_read_halfword(bytes + 4)),
        std::bit_cast<s16>(tmd_read_halfword(bytes + 6))};
}

void tmd_set_current_vertices(KfTmdContext context, SVECTOR *vertices)
{
    context.current_vertices = vertices;
}

void tmd_select_object_vertices(KfTmdContext context, u16 index)
{
    const auto *object = tmd_get_object(context, index);
    const auto resource = context.current_asset;
    const std::size_t offset = object->vertex_offset;
    const auto payload_size = resource.size - KF_TMD_HEADER_BYTES;
    if (offset > payload_size || object->vertex_count > (payload_size - offset) / sizeof(SVECTOR))
        kf::host_fail("TMD vertices exceed their resource");
    auto *vertices = reinterpret_cast<u8 *>(resource.data) + KF_TMD_HEADER_BYTES + offset;
    if (reinterpret_cast<std::uintptr_t>(vertices) % alignof(SVECTOR))
        kf::host_fail("Unaligned TMD vertices");
    context.current_vertices = reinterpret_cast<SVECTOR *>(vertices);
}

void tmd_register(KfTmdContext context, KfTmdSlot slot, u8 *data, std::size_t size)
{
    const auto resource = tmd_resource_view(data, size);
    tmd_slot(context, slot) = resource;
    context.current_asset = resource;
}

void tmd_release_last_allocation(KfTmdContext context, KfMemoryArena &arena, KfTmdSlot slot)
{
    auto &resource = tmd_slot(context, slot);
    if (context.current_asset.data == resource.data) {
        context.current_asset = {};
        context.current_vertices = NULL;
    }
    resource = {};
    memory_release_last(arena);
}

void tmd_project_vertices_shift(KfTmdContext context, s32 count, u8 shift, const MATRIX *model, const kf::Projection &projection)
{
    if (count < 0 || count > KF_PROJECTED_VERTEX_CAPACITY)
        kf::host_fail("Model exceeds projected vertex capacity.");
    KfScreenVertex *projected;
    SVECTOR *vertex;

    projected = context.projected_vertices.data();
    vertex = context.current_vertices;
    for (count--; count != -1; count--) {
        const auto point = kf::render_project_point(*model, projection, *vertex);
        projected->sxy.vector = {point.x, point.y};
        projected->p2 = point.fog << KF_TMD_DEFAULT_PERSPECTIVE_SHIFT;
        projected->sz = point.depth >> shift;
        projected->fx = point.fx;
        projected->fy = point.fy;
        projected->w = point.w;
        projected->cx = point.cx;
        projected->cy = point.cy;
        projected->cz = point.cz;
        projected++;
        vertex++;
    }
}

void tmd_transform_vertices(KfTmdContext context, s32 count, const MATRIX *model)
{
    if (count < 0 || count > KF_PROJECTED_VERTEX_CAPACITY)
        kf::host_fail("Model exceeds projected vertex capacity.");
    KfScreenVertex *projected;
    SVECTOR *vertex;
    VECTOR transformed;
    s32 remaining;

    projected = context.projected_vertices.data();
    vertex = context.current_vertices;
    for (remaining = count - 1; remaining != -1; remaining--) {
        transformed = kf::render_transform_point(*model, *vertex);
        projected->sxy.vector.vx = transformed.vx;
        projected->sxy.vector.vy = transformed.vy;
        projected->p2 = transformed.vz;
        projected->sz = transformed.vz;
        projected->fx = projected->sxy.vector.vx;
        projected->fy = projected->sxy.vector.vy;
        projected->w = 0;
        projected->cx = projected->cy = projected->cz = 0;
        projected++;
        vertex++;
    }
}
