// Automap: a north-up minimap in a corner and a full floor map on M, drawn from
// the snapshot the game publishes each update (kf/platform/automap.hpp).
#include "internal.hpp"
#include <imgui.h>
#include <kf/platform/automap.hpp>
#include <kf/platform/input.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace kf::ext {
namespace {
bool full_map_open;

constexpr int gameplay_context = static_cast<int>(InputContext::Gameplay);
constexpr int scripted_context = static_cast<int>(InputContext::Scripted);

bool walkable(AutomapCell cell) {
    return cell != AutomapCell::Void && cell != AutomapCell::Wall;
}

bool is_half(AutomapCell cell) {
    return cell >= AutomapCell::HalfXGeZ && cell <= AutomapCell::HalfSumGe;
}

struct MapView {
    ImDrawList *draw;
    float origin_x, origin_y;   // screen position of the map point (centre_x, centre_z)
    float centre_x, centre_z;   // in cells
    float scale;                // pixels per cell
    float alpha;
    ImVec2 at(float x, float z) const {
        // North (+z) is up.
        return ImVec2(origin_x + (x - centre_x) * scale, origin_y - (z - centre_z) * scale);
    }
    ImU32 colour(int r, int g, int b, float a = 1.0f) const {
        return IM_COL32(r, g, b, static_cast<int>(255 * a * alpha));
    }
};

bool cell_shown(const AutomapSnapshot &map, int x, int z) {
    if (x < 0 || z < 0 || x >= automap_columns || z >= automap_rows)
        return false;
    return !settings().map_fog || map.owns_map || automap_seen(map.floor, x, z);
}

AutomapCell cell_at(const AutomapSnapshot &map, int x, int z) {
    if (x < 0 || z < 0 || x >= automap_columns || z >= automap_rows)
        return AutomapCell::Void;
    return map.cells[z * automap_columns + x];
}

void draw_cells(const MapView &view, const AutomapSnapshot &map, int min_x, int min_z, int max_x, int max_z) {
    const ImU32 floor_colour = view.colour(78, 70, 56, 0.9f);
    const ImU32 wall_colour = view.colour(215, 200, 160);
    const ImU32 door_colour = view.colour(190, 120, 50);
    const float wall_width = std::max(1.0f, view.scale * 0.14f);
    for (int z = std::max(min_z, 0); z <= std::min(max_z, automap_rows - 1); ++z) {
        for (int x = std::max(min_x, 0); x <= std::min(max_x, automap_columns - 1); ++x) {
            const auto cell = cell_at(map, x, z);
            if (!walkable(cell) || !cell_shown(map, x, z))
                continue;
            // Corners: a = (x, z), b = (x+1, z), c = (x+1, z+1), d = (x, z+1).
            const ImVec2 a = view.at(x, z), b = view.at(x + 1, z), c = view.at(x + 1, z + 1), d = view.at(x, z + 1);
            ImU32 fill = floor_colour;
            switch (map.hazards[z * automap_columns + x]) {
            case AutomapHazard::Pit: fill = view.colour(120, 60, 40, 0.9f); break;
            case AutomapHazard::Poison: fill = view.colour(60, 110, 50, 0.9f); break;
            case AutomapHazard::Bottomless: fill = view.colour(20, 16, 14, 0.95f); break;
            case AutomapHazard::None: break;
            }
            switch (cell) {
            case AutomapCell::HalfXGeZ:
                view.draw->AddTriangleFilled(a, b, c, fill);
                view.draw->AddLine(a, c, wall_colour, wall_width);
                break;
            case AutomapCell::HalfSumLe:
                view.draw->AddTriangleFilled(a, b, d, fill);
                view.draw->AddLine(b, d, wall_colour, wall_width);
                break;
            case AutomapCell::HalfZGeX:
                view.draw->AddTriangleFilled(a, c, d, fill);
                view.draw->AddLine(a, c, wall_colour, wall_width);
                break;
            case AutomapCell::HalfSumGe:
                view.draw->AddTriangleFilled(b, c, d, fill);
                view.draw->AddLine(b, d, wall_colour, wall_width);
                break;
            case AutomapCell::Door:
                view.draw->AddRectFilled(d, b, door_colour);
                break;
            case AutomapCell::DoorOpen:
                view.draw->AddRectFilled(d, b, fill);
                view.draw->AddRect(d, b, door_colour, 0.0f, 0, std::max(1.0f, view.scale * 0.1f));
                break;
            default:
                view.draw->AddRectFilled(d, b, fill);
                break;
            }
            // Walls: a side facing a solid neighbour. Half cells only own the two
            // sides their walkable triangle touches.
            const bool south = cell != AutomapCell::HalfZGeX && cell != AutomapCell::HalfSumGe;
            const bool north = cell != AutomapCell::HalfXGeZ && cell != AutomapCell::HalfSumLe;
            const bool west = cell != AutomapCell::HalfXGeZ && cell != AutomapCell::HalfSumGe;
            const bool east = cell != AutomapCell::HalfZGeX && cell != AutomapCell::HalfSumLe;
            if (south && !walkable(cell_at(map, x, z - 1)))
                view.draw->AddLine(a, b, wall_colour, wall_width);
            if (north && !walkable(cell_at(map, x, z + 1)))
                view.draw->AddLine(d, c, wall_colour, wall_width);
            if (west && !walkable(cell_at(map, x - 1, z)))
                view.draw->AddLine(a, d, wall_colour, wall_width);
            if (east && !walkable(cell_at(map, x + 1, z)))
                view.draw->AddLine(b, c, wall_colour, wall_width);
        }
    }
}

bool marker_shown(const AutomapSnapshot &map, const AutomapMarker &marker) {
    const Settings &s = settings();
    switch (marker.kind) {
    case AutomapMarkerKind::Item: return s.map_items;
    case AutomapMarkerKind::Chest: return s.map_chests;
    case AutomapMarkerKind::Enemy: return s.map_enemies;
    case AutomapMarkerKind::Door: return false;   // the door cell itself is drawn
    default: return cell_shown(map, static_cast<int>(marker.x), static_cast<int>(marker.z));
    }
}

void draw_label(const MapView &view, ImVec2 centre, float size, const char *text, ImU32 colour) {
    ImFont *font = ImGui::GetFont();
    const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
    view.draw->AddText(font, size, ImVec2(centre.x - extent.x * 0.5f, centre.y - extent.y * 0.5f), colour, text);
}

void draw_markers(const MapView &view, const AutomapSnapshot &map, bool labels) {
    // The full map keeps markers legible even when a cell is only a few pixels.
    const float r = std::max(labels ? 5.0f : 2.5f, view.scale * 0.42f);
    const ImU32 outline = view.colour(0, 0, 0, 0.8f);
    for (int i = 0; i < map.marker_count; ++i) {
        const auto &marker = map.markers[i];
        if (!marker_shown(map, marker))
            continue;
        const ImVec2 p = view.at(marker.x, marker.z);
        switch (marker.kind) {
        case AutomapMarkerKind::LockedDoor:
            view.draw->AddRectFilled(ImVec2(p.x - r * 0.6f, p.y - r * 0.5f), ImVec2(p.x + r * 0.6f, p.y + r * 0.6f), view.colour(210, 60, 50));
            view.draw->AddCircle(ImVec2(p.x, p.y - r * 0.5f), r * 0.4f, view.colour(210, 60, 50), 12, std::max(1.0f, r * 0.2f));
            break;
        case AutomapMarkerKind::Warp: {
            view.draw->AddCircleFilled(p, r * 1.1f, view.colour(150, 90, 220));
            view.draw->AddCircle(p, r * 1.1f, outline, 16, 1.0f);
            if (labels || r >= 4.0f) {
                char text[4];
                if (marker.label == automap_warp_teleport)
                    std::snprintf(text, sizeof text, "T");
                else if (marker.label == automap_warp_exit)
                    std::snprintf(text, sizeof text, "X");
                else
                    std::snprintf(text, sizeof text, "%d", marker.label);
                draw_label(view, p, r * 1.7f, text, view.colour(255, 255, 255));
            }
            break;
        }
        case AutomapMarkerKind::SavePoint: {
            const ImVec2 top(p.x, p.y - r), right(p.x + r, p.y), bottom(p.x, p.y + r), left(p.x - r, p.y);
            view.draw->AddQuadFilled(top, right, bottom, left, view.colour(80, 220, 230));
            view.draw->AddQuad(top, right, bottom, left, outline, 1.0f);
            break;
        }
        case AutomapMarkerKind::Fountain:
            if (marker.label)
                view.draw->AddCircleFilled(p, r, view.colour(70, 140, 255));
            view.draw->AddCircle(p, r, view.colour(70, 140, 255), 16, std::max(1.0f, r * 0.3f));
            break;
        case AutomapMarkerKind::Npc:
            view.draw->AddCircleFilled(p, r * 0.8f, view.colour(120, 230, 120));
            view.draw->AddCircle(p, r * 0.8f, outline, 12, 1.0f);
            break;
        case AutomapMarkerKind::Item:
            view.draw->AddCircleFilled(p, r * 0.55f, view.colour(250, 220, 90));
            break;
        case AutomapMarkerKind::Chest:
            view.draw->AddRectFilled(ImVec2(p.x - r * 0.75f, p.y - r * 0.55f), ImVec2(p.x + r * 0.75f, p.y + r * 0.55f), view.colour(230, 170, 60));
            view.draw->AddRect(ImVec2(p.x - r * 0.75f, p.y - r * 0.55f), ImVec2(p.x + r * 0.75f, p.y + r * 0.55f), outline, 0.0f, 0, 1.0f);
            break;
        case AutomapMarkerKind::Enemy:
            view.draw->AddCircleFilled(p, r * 0.7f, view.colour(235, 50, 50));
            view.draw->AddCircle(p, r * 0.7f, outline, 12, 1.0f);
            break;
        case AutomapMarkerKind::Door:
            break;
        }
    }
}

void draw_player(const MapView &view, const AutomapSnapshot &map) {
    const ImVec2 p = view.at(map.player_x, map.player_z);
    // Facing (-sin yaw, +cos yaw) in map x/z; screen y points south.
    const float fx = -std::sin(map.player_yaw), fz = std::cos(map.player_yaw);
    const ImVec2 forward(fx, -fz), side(fz, fx);
    const float size = std::max(5.0f, view.scale * 0.9f);
    const ImVec2 tip(p.x + forward.x * size, p.y + forward.y * size);
    const ImVec2 back(p.x - forward.x * size * 0.45f, p.y - forward.y * size * 0.45f);
    const ImVec2 left(p.x - forward.x * size * 0.7f + side.x * size * 0.65f, p.y - forward.y * size * 0.7f + side.y * size * 0.65f);
    const ImVec2 right(p.x - forward.x * size * 0.7f - side.x * size * 0.65f, p.y - forward.y * size * 0.7f - side.y * size * 0.65f);
    view.draw->AddQuadFilled(tip, left, back, right, view.colour(255, 250, 220));
    view.draw->AddQuad(tip, left, back, right, view.colour(0, 0, 0), 1.5f);
}

void draw_minimap(const AutomapSnapshot &map) {
    const Settings &s = settings();
    const ViewRect game = game_view();
    const float size = game.h * static_cast<float>(s.minimap_size) / 100.0f;
    const float margin = game.h * 0.025f;
    float left = game.x + game.w - size - margin, top = game.y + game.h - size - margin;
    if (s.minimap_corner == 0 || s.minimap_corner == 2)
        top = game.y + margin;
    if (s.minimap_corner >= 2)
        left = game.x + margin;
    const ImVec2 lo(left, top), hi(left + size, top + size);
    MapView view{ImGui::GetBackgroundDrawList(), left + size * 0.5f, top + size * 0.5f, map.player_x, map.player_z,
                 size / (2.0f * s.minimap_zoom), static_cast<float>(s.minimap_opacity) / 100.0f};
    view.draw->AddRectFilled(lo, hi, view.colour(10, 9, 8, 0.75f), size * 0.04f);
    view.draw->PushClipRect(lo, hi, true);
    const int radius = s.minimap_zoom + 2;
    const int cx = static_cast<int>(map.player_x), cz = static_cast<int>(map.player_z);
    draw_cells(view, map, cx - radius, cz - radius, cx + radius, cz + radius);
    draw_markers(view, map, false);
    draw_player(view, map);
    view.draw->PopClipRect();
    view.draw->AddRect(lo, hi, view.colour(200, 180, 130, 0.8f), size * 0.04f, 0, 1.5f);
    draw_label(view, ImVec2(lo.x + size * 0.5f, lo.y + size * 0.06f), std::max(12.0f, size * 0.07f), "N", view.colour(230, 60, 50));
}

void draw_legend_entry(ImDrawList *draw, ImVec2 &at, float size, ImU32 colour, const char *text, bool square = false) {
    const ImVec2 centre(at.x + size * 0.5f, at.y + size * 0.55f);
    if (square)
        draw->AddRectFilled(ImVec2(centre.x - size * 0.35f, centre.y - size * 0.3f), ImVec2(centre.x + size * 0.35f, centre.y + size * 0.3f), colour);
    else
        draw->AddCircleFilled(centre, size * 0.33f, colour);
    draw->AddText(ImGui::GetFont(), size, ImVec2(at.x + size * 1.3f, at.y), IM_COL32(230, 220, 200, 255), text);
    at.y += size * 1.35f;
}

void draw_full_map(const AutomapSnapshot &map) {
    const Settings &s = settings();
    const ViewRect game = game_view();
    // Above the game HUD and mod HUDs; the map replaces the picture while open.
    ImDrawList *draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled(ImVec2(game.x, game.y), ImVec2(game.x + game.w, game.y + game.h), IM_COL32(6, 5, 4, 255));
    // Fit the shown part of the floor, with a little room around it.
    int min_x = automap_columns, min_z = automap_rows, max_x = -1, max_z = -1;
    for (int z = 0; z < automap_rows; ++z)
        for (int x = 0; x < automap_columns; ++x)
            if (walkable(cell_at(map, x, z)) && cell_shown(map, x, z)) {
                min_x = std::min(min_x, x); max_x = std::max(max_x, x);
                min_z = std::min(min_z, z); max_z = std::max(max_z, z);
            }
    const int px = static_cast<int>(map.player_x), pz = static_cast<int>(map.player_z);
    min_x = std::min(min_x, px) - 2; max_x = std::max(max_x, px) + 3;
    min_z = std::min(min_z, pz) - 2; max_z = std::max(max_z, pz) + 3;
    const float legend_width = game.h * 0.28f;
    const float area_w = game.w - legend_width - game.h * 0.08f, area_h = game.h * 0.86f;
    // Small explored areas are not blown up past a readable size.
    const float scale = std::min({area_w / (max_x - min_x), area_h / (max_z - min_z), game.h / 28.0f});
    const float area_x = game.x + game.h * 0.04f, area_y = game.y + game.h * 0.08f;
    MapView view{draw, area_x + area_w * 0.5f, area_y + area_h * 0.5f,
                  (min_x + max_x) * 0.5f, (min_z + max_z) * 0.5f, scale, 1.0f};
    draw->PushClipRect(ImVec2(area_x, area_y), ImVec2(area_x + area_w, area_y + area_h), true);
    draw_cells(view, map, min_x, min_z, max_x, max_z);
    draw_markers(view, map, true);
    draw_player(view, map);
    draw->PopClipRect();

    const float text = std::max(14.0f, game.h / 34.0f);
    char title[48];
    std::snprintf(title, sizeof title, "Floor %d", map.floor);
    draw->AddText(ImGui::GetFont(), text * 1.4f, ImVec2(game.x + game.h * 0.04f, game.y + game.h * 0.02f), IM_COL32(240, 220, 170, 255), title);
    ImVec2 at(game.x + game.w - legend_width, game.y + game.h * 0.1f);
    draw_legend_entry(draw, at, text, IM_COL32(150, 90, 220, 255), "Warp (number = floor)");
    draw_legend_entry(draw, at, text, IM_COL32(80, 220, 230, 255), "Save point");
    draw_legend_entry(draw, at, text, IM_COL32(70, 140, 255, 255), "Fountain");
    draw_legend_entry(draw, at, text, IM_COL32(190, 120, 50, 255), "Door", true);
    draw_legend_entry(draw, at, text, IM_COL32(210, 60, 50, 255), "Locked door", true);
    draw_legend_entry(draw, at, text, IM_COL32(120, 230, 120, 255), "Person");
    draw_legend_entry(draw, at, text, IM_COL32(120, 60, 40, 255), "Pit / trap", true);
    draw_legend_entry(draw, at, text, IM_COL32(60, 110, 50, 255), "Poison", true);
    if (s.map_items)
        draw_legend_entry(draw, at, text, IM_COL32(250, 220, 90, 255), "Item");
    if (s.map_chests)
        draw_legend_entry(draw, at, text, IM_COL32(230, 170, 60, 255), "Chest", true);
    if (s.map_enemies)
        draw_legend_entry(draw, at, text, IM_COL32(235, 50, 50, 255), "Enemy");
    at.y += text;
    draw->AddText(ImGui::GetFont(), text * 0.85f, at, IM_COL32(170, 160, 140, 255), "M closes the map");
}
}

void automap_toggle_full() {
    full_map_open = !full_map_open;
}

void automap_draw(int input_context) {
    const Settings &s = settings();
    const auto &map = automap_snapshot();
    const bool in_game = input_context == gameplay_context || input_context == scripted_context;
    if (!in_game || map.floor == 0) {
        full_map_open = false;
        return;
    }
    // KF_TEST_FULLMAP: show the full map for automated captures.
    static const bool test_full = std::getenv("KF_TEST_FULLMAP") != nullptr;
    if (full_map_open || test_full)
        draw_full_map(map);
    else if (s.minimap)
        draw_minimap(map);
}

void automap_settings_page(bool &changed) {
    Settings &s = settings();
    ImGui::SeparatorText("Minimap");
    changed |= ImGui::Checkbox("Show minimap", &s.minimap);
    if (s.minimap) {
        const char *corners[] = {"Top right", "Bottom right", "Top left", "Bottom left"};
        changed |= ImGui::Combo("Corner", &s.minimap_corner, corners, 4);
        changed |= slider_input("Size", &s.minimap_size, 10, 50, "%d%%");
        ImGui::SetItemTooltip("Minimap size as a percentage of the picture's height.");
        changed |= slider_input("Zoom", &s.minimap_zoom, 4, 30, "%d cells");
        ImGui::SetItemTooltip("How many map cells the minimap shows from the centre to its edge.");
        changed |= slider_input("Opacity", &s.minimap_opacity, 20, 100, "%d%%");
    }
    ImGui::TextDisabled("Press M in game for the full floor map.");
    ImGui::SeparatorText("Exploration");
    changed |= ImGui::Checkbox("Fog of war", &s.map_fog);
    ImGui::SetItemTooltip("On: only places you have seen are drawn (the Watchman's or Sorcerer's map\nreveals the whole floor). Off: the whole floor is always shown.");
    ImGui::SeparatorText("Extra markers (cheats)");
    changed |= ImGui::Checkbox("Items on the ground", &s.map_items);
    changed |= ImGui::Checkbox("Chests with something inside", &s.map_chests);
    changed |= ImGui::Checkbox("Enemies", &s.map_enemies);
    ImGui::TextDisabled("Doors, warps, save points, fountains and people always show once seen.");
}
}
