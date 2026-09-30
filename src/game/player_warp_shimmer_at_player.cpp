#include <kf/platform/prelude.h>
#include <kf/game/player.h>

void player_warp_shimmer_at_player(KfWarpShimmerMode shimmer_mode)
{
    VECTOR position{player_state.camera_position.vx, player_state.foot_height,
        player_state.camera_position.vz};
    player_warp_shimmer(shimmer_mode, &position);
}
