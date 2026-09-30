#ifndef KF_OPEN_SCENE0_H
#define KF_OPEN_SCENE0_H

#include <kf/cutscene/audio.h>
#include <kf/lib/map.h>

extern void opening_render_map_cells(void);
extern void opening_render_entities_and_items(void);
extern void opening_scene0_render_frame(
    const VECTOR *position_or_null, const SVECTOR *rotation_or_null);

#endif // KF_OPEN_SCENE0_H
