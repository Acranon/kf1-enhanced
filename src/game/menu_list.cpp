#include <kf/platform/prelude.h>
#include <kf/game/menu.h>
#include <kf/platform/input.h>

void menu_list_previous(KfMenuList *list)
{
    if (list->selected_index != 0) {
        list->selected_index--;
        if (list->cursor_row == 0)
            list->scroll_offset--;
        else
            list->cursor_row--;
    } else {
        list->selected_index = list->entry_count - 1;
        if (list->entry_count < list->visible_rows) {
            list->scroll_offset = 0;
            list->cursor_row = list->entry_count - 1;
        } else {
            list->scroll_offset = list->entry_count - list->visible_rows;
            list->cursor_row = list->visible_rows - 1;
        }
    }
}

void menu_list_next(KfMenuList *list)
{
    if (list->selected_index < list->entry_count - 1) {
        list->selected_index++;
        if (list->cursor_row == list->visible_rows - 1)
            list->scroll_offset++;
        else
            list->cursor_row++;
    } else {
        list->selected_index = 0;
        list->scroll_offset = 0;
        list->cursor_row = 0;
    }
}

bool menu_list_handle_navigation(KfMenuList &list, u32 input, u32 previous)
{
    if (kf::button_pressed(input, previous, kf::Button::Up)) {
        menu_play_input_sound(MENU_SOUND_CURSOR);
        menu_list_previous(&list);
    } else if (kf::button_pressed(input, previous, kf::Button::Down)) {
        menu_play_input_sound(MENU_SOUND_CURSOR);
        menu_list_next(&list);
    } else {
        return false;
    }
    return true;
}
