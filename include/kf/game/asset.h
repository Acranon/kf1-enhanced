#ifndef KF_GAME_ASSET_H
#define KF_GAME_ASSET_H

#include <kf/lib/types.h>
#include <kf/lib/codec.h>

enum {
    KF_ASSET_ARCHIVE_HEADER_BYTES = 4,
    KF_ASSET_ACTOR_FIRST = 0,
    KF_ASSET_MAP_EVENT_FIRST = 10,
    KF_ASSET_WEAPON = 20,
    KF_ASSET_HUD_MODELS = 21,
    KF_ASSET_EFFECT_FIRST = 30,
    KF_ASSET_REGISTRY_KNOWN_ENTRIES = 48,
    KF_WEAPON_ASSET_BUFFER_BYTES = 49152
};

extern void asset_registry_load_tmd_archive(
    u16 first_asset_id, u8 *archive, std::size_t size);
extern void asset_registry_select(u16 asset_id);
extern void asset_registry_set(u16 asset_id, void *data, std::size_t size);

#endif // KF_GAME_ASSET_H
