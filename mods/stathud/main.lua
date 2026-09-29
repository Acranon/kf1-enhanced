-- Stat HUD: example of on_draw (drawing over the game), on_settings and config.
-- config is saved automatically when a settings widget changes.
-- Offsets from the bottom-left corner of the game picture.
config.x = config.x or 16
config.y = config.y or 16
config.show_gold = config.show_gold ~= false
config.bars = config.bars ~= false

local function bar(x, y, w, value, maximum, r, g, b)
  local fill = maximum > 0 and w * math.min(value / maximum, 1) or 0
  ui.draw_rect(x, y, w, 8, 0, 0, 0, 0.6)
  ui.draw_rect(x, y, fill, 8, r, g, b, 0.9)
end

function on_draw()
  if kf.context() == "opening" then return end
  local p = kf.player
  local vx, vy, vw, vh = kf.view()
  local x = vx + config.x
  local y = vy + vh - config.y - (config.show_gold and 62 or 42)
  ui.draw_rect(x - 6, y - 4, 190, config.show_gold and 70 or 50, 0, 0, 0, 0.45)
  ui.draw_text(x, y, string.format("HP %d / %d", p.hp, p.max_hp), 1, 0.55, 0.5)
  if config.bars then bar(x + 110, y + 5, 60, p.hp, p.max_hp, 0.9, 0.2, 0.2) end
  ui.draw_text(x, y + 20, string.format("MP %d / %d", p.mp, p.max_mp), 0.55, 0.7, 1)
  if config.bars then bar(x + 110, y + 25, 60, p.mp, p.max_mp, 0.25, 0.45, 1) end
  if config.show_gold then
    ui.draw_text(x, y + 40, string.format("Gold %d", p.gold), 1, 0.85, 0.3)
  end
end

function on_settings()
  config.x = ui.slider_int("From left", config.x, 0, 1200)
  config.y = ui.slider_int("From bottom", config.y, 0, 800)
  config.bars = ui.checkbox("Bars", config.bars)
  config.show_gold = ui.checkbox("Show gold", config.show_gold)
end
