#ifdef E_TYPEDEFS



typedef struct _E_Event_Compositor_Resize   E_Event_Compositor_Resize;

#else
#ifndef E_COMP_CANVAS_H
#define E_COMP_CANVAS_H

struct _E_Event_Compositor_Resize
{
   E_Comp *comp;
};

extern EAPI int E_EVENT_COMPOSITOR_RESIZE;

EAPI Eina_Bool e_comp_canvas_init(void);
EINTERN void e_comp_canvas_clear(void);
EAPI void e_comp_all_freeze(void);
EAPI void e_comp_all_thaw(void);
EAPI E_Zone * e_comp_zone_xy_get(Evas_Coord x, Evas_Coord y);
EAPI E_Zone * e_comp_zone_number_get(int num);
EAPI E_Zone * e_comp_zone_id_get(int id);
EAPI E_Desk * e_comp_desk_window_profile_get(const char *profile);
EAPI void e_comp_canvas_zone_update(E_Zone *zone);
EAPI void e_comp_canvas_update(void);
EAPI void e_comp_canvas_fake_layers_init(void);
EAPI void e_comp_canvas_fps_toggle(void);
EAPI E_Layer e_comp_canvas_layer_map_to(unsigned int layer);
EAPI unsigned int e_comp_canvas_layer_map(E_Layer layer);
EAPI unsigned int e_comp_canvas_client_layer_map(E_Layer layer);
EAPI E_Layer e_comp_canvas_client_layer_map_nearest(int layer);


/* the following functions are used for adjusting root window coordinates
 * to/from canvas coordinates.
 * this ensures correct positioning when running E as a nested compositor
 *
 * - use the "adjust" functions to go root->canvas
 * - use the "unadjust" functions to go canvas->root
 */
static inline int
e_comp_canvas_x_root_unadjust(int x)
{
   int cx;

   ecore_evas_geometry_get(e_comp->ee, &cx, NULL, NULL, NULL);
   return x + cx;
}

static inline int
e_comp_canvas_y_root_unadjust(int y)
{
   int cy;

   ecore_evas_geometry_get(e_comp->ee, NULL, &cy, NULL, NULL);
   return y + cy;
}

static inline int
e_comp_canvas_x_root_adjust(int x)
{
   int cx;

   ecore_evas_geometry_get(e_comp->ee, &cx, NULL, NULL, NULL);
   return x - cx;
}

static inline int
e_comp_canvas_y_root_adjust(int y)
{
   int cy;

   ecore_evas_geometry_get(e_comp->ee, NULL, &cy, NULL, NULL);
   return y - cy;
}

#endif
#endif
