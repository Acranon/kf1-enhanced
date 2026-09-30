#include <kf/platform/prelude.h>
#include <kf/game/game.h>
#include <kf/game/menu.h>

namespace {
class MenuSession {
public:
    MenuSession() : previous_input(kf::host_set_input_context(kf::InputContext::Menu))
    {
        animation_cache_release_all();
    }
    MenuSession(const MenuSession &) = delete;
    MenuSession &operator=(const MenuSession &) = delete;
    ~MenuSession()
    {
        player_clear_motion();
        kf::host_set_input_context(previous_input);
    }
private:
    kf::InputContext previous_input;
};
}

KfMenuOutcome menu_open_root()
{
    const MenuSession session;
    return menu_root();
}

KfMenuResult menu_confirm_pickup(KfObjectId item)
{
    const MenuSession session;
    return item_pickup_confirm(item);
}

void menu_open_shop(KfItemStockBank bank)
{
    const MenuSession session;
    shop_menu_root(bank);
}
