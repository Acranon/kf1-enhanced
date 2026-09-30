#include "../src/game/menu_enter_mode.cpp"
#include <cassert>
#include <type_traits>

static_assert(!std::is_constructible_v<KfMenuOutcome, KfMenuAction, KfObjectId>);
static_assert(!std::is_copy_constructible_v<MenuSession>);

static kf::InputContext context = kf::InputContext::Gameplay;
static unsigned releases, clears;
static KfMenuOutcome next_outcome{KfMenuAction::Close};
static KfMenuResult pickup_result;
static KfObjectId picked_item;
static KfItemStockBank opened_shop;

namespace kf {
InputContext host_set_input_context(InputContext next)
{
    const auto previous = context;
    context = next;
    return previous;
}
}
void animation_cache_release_all()
{
    assert(context == kf::InputContext::Menu);
    ++releases;
}
void player_clear_motion()
{
    assert(context == kf::InputContext::Menu);
    ++clears;
}
KfMenuOutcome menu_root()
{
    assert(context == kf::InputContext::Menu);
    assert(releases == clears + 1);
    return next_outcome;
}
KfMenuResult item_pickup_confirm(KfObjectId item)
{
    assert(context == kf::InputContext::Menu);
    picked_item = item;
    return pickup_result;
}
void shop_menu_root(KfItemStockBank bank)
{
    assert(context == kf::InputContext::Menu);
    opened_shop = bank;
}

int main()
{
    for (auto previous : {kf::InputContext::Gameplay, kf::InputContext::Scripted}) {
        context = previous;
        for (const KfMenuOutcome outcome : {KfMenuOutcome{KF_ITEM_SHORT_SWORD}, KfMenuOutcome{KfMenuAction::Close},
                KfMenuOutcome{KfMenuAction::GameLoaded}, KfMenuOutcome{KfMenuAction::ReturnToIntro}}) {
            next_outcome = outcome;
            const auto result = menu_open_root();
            assert(result == outcome);
            assert(context == previous && releases == clears);
        }
        for (auto result : {KF_MENU_RESULT_ACCEPTED, KF_MENU_RESULT_CANCELLED,
                            KF_MENU_RESULT_DECLINED, KF_MENU_RESULT_STACK_FULL}) {
            pickup_result = result;
            assert(menu_confirm_pickup(KF_ITEM_SHORT_SWORD) == result);
            assert(picked_item == KF_ITEM_SHORT_SWORD);
            assert(context == previous && releases == clears);
        }
        for (auto bank : {KF_ITEM_STOCK_FIRST_SHOP, KF_ITEM_STOCK_SECOND_SHOP}) {
            menu_open_shop(bank);
            assert(opened_shop == bank);
            assert(context == previous && releases == clears);
        }
    }
}
