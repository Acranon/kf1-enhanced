#ifndef KF_GAME_SAVE_H
#define KF_GAME_SAVE_H
#include <kf/lib/types.h>
#include <kf/lib/enum.h>
#include <kf/lib/floor.h>

enum class KfSaveResult : s32 {
    KF_SAVE_RESULT_FAILED = 0,
    KF_SAVE_RESULT_OK = 1,
    KF_SAVE_RESULT_NO_SPACE = 3,
    KF_SAVE_RESULT_READ_FAILED = 13
}; using enum KfSaveResult;
enum { KF_SAVE_SLOT_COUNT = 3 };
enum class KfSaveSlotId : s16 {
    KF_SAVE_SLOT_FIRST = 1,
    KF_SAVE_SLOT_SECOND = 2,
    KF_SAVE_SLOT_THIRD = 3
}; using enum KfSaveSlotId;
using KfSaveSlotArgument = KfSaveSlotId;

enum class KfSaveSlotState : u8 { Empty, Ready, Damaged, Unavailable };
struct KfSaveSlotSummary {
    u32 experience;
    KfEnumStorage<KfFloorId, u32> current_floor;
    u32 current_hp;
    u32 maximum_hp;
    u32 current_mp;
    u32 maximum_mp;
    KfSaveSlotState state;
};
KfSaveResult save_system_read_catalog(KfSaveSlotSummary *summaries);
KfSaveResult save_system_read_slot(KfSaveSlotId slot);
KfSaveResult save_system_write_slot(KfSaveSlotId slot);
bool save_system_quick_write(void);
bool save_system_quick_read(void);
#endif
