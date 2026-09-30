#ifndef KF_GAME_SAVE_H
#define KF_GAME_SAVE_H

#include <kf/lib/enum.h>
#include <kf/lib/floor.h>
#include <kf/lib/types.h>
#include <kf/platform/saves.h>

#include <array>

enum class KfSaveResult : s32 {
    KF_SAVE_RESULT_FAILED = 0,
    KF_SAVE_RESULT_OK = 1,
    KF_SAVE_RESULT_NO_SPACE = 3,
    KF_SAVE_RESULT_READ_FAILED = 13
}; using enum KfSaveResult;
enum { KF_SAVE_SLOT_COUNT = 3 };
enum class KfSaveSlotState : u8 { Empty, Ready, Damaged, Unavailable };
struct KfSaveSlotSummary {
    u32 experience;
    KfFloorId current_floor;
    u32 current_hp;
    u32 maximum_hp;
    u32 current_mp;
    u32 maximum_mp;
    KfSaveSlotState state;
};
KfSaveResult save_system_read_catalog(std::array<KfSaveSlotSummary, KF_SAVE_SLOT_COUNT> &summaries);
KfSaveResult save_system_read_slot(kf::SaveSlot slot);
KfSaveResult save_system_write_slot(kf::SaveSlot slot);
bool save_system_quick_write(void);
bool save_system_quick_read(void);

#endif // KF_GAME_SAVE_H
