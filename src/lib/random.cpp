#include <kf/lib/random.h>

namespace kf {
static constexpr u32 random_multiplier = 1103515245u;
static constexpr u32 random_increment = 12345u;
static constexpr unsigned random_output_shift = 16;
// One stream spans opening/game/ending, as the original BIOS-owned state did.
// The portable application starts with a deterministic seed of one.
static u32 random_state = 1;

s32 random_next()
{
    random_state = random_state * random_multiplier + random_increment;
    return static_cast<s32>((random_state >> random_output_shift) & random_max);
}
}
