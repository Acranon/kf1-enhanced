#ifndef KF_OPEN_OPENING_RENDER_H
#define KF_OPEN_OPENING_RENDER_H

#include <kf/cutscene/resources.h>
#include <kf/lib/geometry_types.h>
#include <kf/lib/render_types.h>
#include <kf/lib/types.h>

extern void opening_render_frame(
    const VECTOR *position_or_null, const SVECTOR *rotation_or_null);
extern void sprite_add_g4(
    const KfScreenRect *rectangle, const CVECTOR *color0, const CVECTOR *color1,
    const CVECTOR *color2, const CVECTOR *color3);
extern void opening_entity_render(KfOpeningEntity *entity);
extern void opening_render_entities(void);

#endif // KF_OPEN_OPENING_RENDER_H
