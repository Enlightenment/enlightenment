#ifdef E_TYPEDEFS

typedef struct _E_Desk E_Desk;
typedef struct _E_Event_Desk_Show E_Event_Desk_Show;

#else
# ifndef E_DESK_H
#  define E_DESK_H

#  define E_DESK_TYPE 0xE0b01005

struct _E_Desk
{
   E_Object e_obj_inherit;

   E_Zone *zone;
   int x, y;
   const char *name;
   Eina_Bool visible : 1;
   Evas_Object *o_bg;
};

struct _E_Event_Desk_Show
{
   E_Desk   *desk;
};

extern EAPI int E_EVENT_DESK_SHOW;

EINTERN int e_desk_init(void);
EINTERN int e_desk_shutdown(void);

EAPI E_Desk *e_desk_new(E_Zone *zone, int x, int y);
EAPI void e_desk_show(E_Desk *desk);
EAPI void e_desk_hide(E_Desk *desk);
EAPI E_Desk *e_desk_current_get(E_Zone *zone);
EAPI E_Desk *e_desk_at_xy_get(E_Zone *zone, int x, int  y);

# endif
#endif
