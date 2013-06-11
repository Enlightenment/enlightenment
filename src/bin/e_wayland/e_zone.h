#ifdef E_TYPEDEFS

typedef enum _E_Zone_Edge 
{
   E_ZONE_EDGE_NONE,
   E_ZONE_EDGE_LEFT,
   E_ZONE_EDGE_RIGHT,
   E_ZONE_EDGE_TOP,
   E_ZONE_EDGE_BOTTOM,
   E_ZONE_EDGE_TOP_LEFT,
   E_ZONE_EDGE_TOP_RIGHT,
   E_ZONE_EDGE_BOTTOM_RIGHT,
   E_ZONE_EDGE_BOTTOM_LEFT
} E_Zone_Edge;

typedef struct _E_Zone E_Zone;
typedef struct _E_Event_Zone_Edge E_Event_Zone_Edge;

#else
# ifndef E_ZONE_H
#  define E_ZONE_H

#  define E_ZONE_TYPE 0xE0b0100d

struct _E_Zone
{
   E_Object e_obj_inherit;

   E_Container *container;
   int x, y, w, h;
   unsigned int num;
   const char *name;

   Evas_Object *o_bg, *o_prev_bg, *o_bg_clip, *o_bg_event;
   Evas_Object *o_trans;

   E_Desk **desks;

   Eina_Bool visible : 1;

   int desk_x_count, desk_y_count;
   int desk_x_current, desk_y_current;

   Eina_List *popups;
};

struct _E_Event_Zone_Edge
{
   E_Zone *zone;
   E_Zone_Edge edge;
   int x, y, modifiers;
};

EINTERN int e_zone_init(void);
EINTERN int e_zone_shutdown(void);

EAPI E_Zone *e_zone_new(E_Container *con, int num, int x, int y, int w, int h);
EAPI void e_zone_desk_count_set(E_Zone *zone, int x, int y);
EAPI void e_zone_show(E_Zone *zone);
EAPI void e_zone_hide(E_Zone *zone);
EAPI E_Zone *e_zone_current_get(E_Container *con);
EAPI void e_zone_bg_reconfigure(E_Zone *zone);
EAPI void e_zone_useful_geometry_get(E_Zone *zone, int *x, int *y, int *w, int *h);
EAPI void e_zone_desk_count_get(E_Zone *zone, int *x, int *y);

# endif
#endif
