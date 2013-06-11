#ifdef E_TYPEDEFS

typedef enum _E_Maximize
{
   E_MAXIMIZE_NONE = 0x00000000,
   E_MAXIMIZE_FULLSCREEN = 0x00000001,
   E_MAXIMIZE_SMART = 0x00000002,
   E_MAXIMIZE_EXPAND = 0x00000003,
   E_MAXIMIZE_FILL = 0x00000004,
   E_MAXIMIZE_TYPE = 0x0000000f,
   E_MAXIMIZE_VERTICAL = 0x00000010,
   E_MAXIMIZE_HORIZONTAL = 0x00000020,
   E_MAXIMIZE_BOTH = 0x00000030,
   E_MAXIMIZE_DIRECTION = 0x000000f0
} E_Maximize;

typedef enum _E_Window_Placement
{
   E_WINDOW_PLACEMENT_SMART,
   E_WINDOW_PLACEMENT_ANTIGADGET,
   E_WINDOW_PLACEMENT_CURSOR,
   E_WINDOW_PLACEMENT_MANUAL
} E_Window_Placement;

typedef struct _E_Border E_Border;

#else
# ifndef E_BORDER_H
#  define E_BORDER_H

#  define E_BORDER_TYPE 0xE0b01002

struct _E_Border
{
   E_Object e_obj_inherit;
};

EAPI void e_border_uniconify(void *bd);
EAPI void e_border_unshade(void *bd, int dir);
EAPI void e_border_focus_set(void *bd, int focus, int set);
EAPI void e_border_desk_set(void *bd, E_Desk *desk);

EAPI Eina_List *e_border_client_list(void);

EAPI void e_border_activate(E_Border *bd, Eina_Bool just_do_it);
EAPI void e_border_raise(E_Border *bd);

# endif
#endif
