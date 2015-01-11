#ifdef E_TYPEDEFS
typedef struct _E_Comp E_Comp;

#ifdef E_COMP_WL
typedef struct _E_Comp_Wl_Client_Data E_Comp_Client_Data;
typedef struct _E_Comp_Wl_Data E_Comp_Data;
#endif

#ifdef E_COMP_X
typedef struct _E_Comp_X_Client_Data E_Comp_Client_Data;
typedef struct _E_Comp_X_Data E_Comp_Data;
#endif

#if !defined(E_COMP_WL) && !defined(E_COMP_X)
typedef struct _E_Comp_Data E_Comp_Data;
typedef struct _E_Comp_Client_Data E_Comp_Client_Data;
#endif

typedef struct _E_Comp_Demo_Style_Item E_Comp_Demo_Style_Item;

# define E_COMP_TYPE (int) 0xE0b01003

# define E_LAYER_COUNT 19
# define E_CLIENT_LAYER_COUNT 11

typedef enum _E_Layer
{
   E_LAYER_BOTTOM = -100,
   E_LAYER_BG = -1, // zone bg stuff
   E_LAYER_DESKTOP = 0, // desktop objects: fileman, gadgets, shelves
   E_LAYER_DESKTOP_TOP = 10, // raised desktop objects: gadgets
   E_LAYER_CLIENT_DESKTOP = 100, //shelves
   E_LAYER_CLIENT_BELOW = 150,
   E_LAYER_CLIENT_NORMAL = 200,
   E_LAYER_CLIENT_ABOVE = 250,
   E_LAYER_CLIENT_EDGE = 300,
   E_LAYER_CLIENT_FULLSCREEN = 350,
   E_LAYER_CLIENT_EDGE_FULLSCREEN = 400,
   E_LAYER_CLIENT_POPUP = 450,
   E_LAYER_CLIENT_TOP = 500,
   E_LAYER_CLIENT_DRAG = 550,
   E_LAYER_CLIENT_PRIO = 600,
   E_LAYER_POPUP = 999, // popups
   E_LAYER_MENU = 5000, // menus
   E_LAYER_DESKLOCK = 9999, // desklock
   E_LAYER_MAX = 32767 // EVAS_LAYER_MAX
} E_Layer;

#else
# ifndef E_COMP_H
#  define E_COMP_H

# include "e_comp_cfdata.h"

extern EAPI int E_EVENT_COMPOSITOR_DISABLE;
extern EAPI int E_EVENT_COMPOSITOR_ENABLE;

struct _E_Comp
{
   E_Object e_obj_inherit;

   Ecore_Window  win; // input overlay
   Ecore_Evas     *ee;
   Ecore_Window  ee_win;
   Evas_Object    *elm;
   Evas           *evas;
   Evas_Object    *bg_blank_object;
   Eina_List      *zones;
   E_Manager      *man;
   E_Pointer      *pointer;
   Eina_List *clients;
   unsigned int new_clients;

   E_Comp_Data *x_comp_data;
   E_Comp_Data *wl_comp_data;

   E_Pixmap_Type comp_type; //for determining X/Wayland/

   unsigned int num;
   Eina_Stringshare *name;
   struct {
      Ecore_Window win;
      Evas_Object *obj;
      //Eina_Inlist *objs; /* E_Comp_Object; NOT to be exposed; seems pointless? */
      Eina_Inlist *clients; /* E_Client, bottom to top */
      unsigned int clients_count;
   } layers[E_LAYER_COUNT];

   struct
   {
      Evas_Object *rect;
      Evas_Object *obj;
      Ecore_Event_Handler *key_handler;
      E_Comp_Object_Autoclose_Cb del_cb;
      E_Comp_Object_Key_Cb key_cb;
      void *data;
   } autoclose;

   Eina_List *debug_rects;
   Eina_List *ignore_wins;

   Eina_List      *updates;
   Eina_List      *post_updates;
   Ecore_Animator *render_animator;
   Ecore_Job      *shape_job;
   Ecore_Job      *update_job;
   Evas_Object    *fps_bg;
   Evas_Object    *fps_fg;
   Ecore_Job      *screen_job;
   Ecore_Timer    *nocomp_delay_timer;
   Ecore_Timer    *nocomp_override_timer;
   int             animating;
   double          frametimes[122];
   int             frameskip;

   int             nocomp_override; //number of times nocomp override has been requested
   Ecore_Window block_win;
   int             block_count; //number of times block window has been requested

   Ecore_Window  cm_selection; //FIXME: move to comp_x ?
   E_Client       *nocomp_ec;

   int depth;
   unsigned int    input_key_grabs;
   unsigned int    input_mouse_grabs;

   Ecore_Cb        grab_cb;
   Ecore_Cb        bindings_grab_cb;
   Ecore_Cb        bindings_ungrab_cb;

   Eina_Bool       gl : 1;
   Eina_Bool       grabbed : 1;
   Eina_Bool       nocomp : 1;
   Eina_Bool       nocomp_want : 1;
   Eina_Bool       saver : 1;
   Eina_Bool       shape_queue_blocked : 1;
};


struct _E_Comp_Demo_Style_Item
{
   Evas_Object *preview;
   Evas_Object *frame;
   Evas_Object *livethumb;
   Evas_Object *layout;
   Evas_Object *border;
   Evas_Object *client;
};

typedef enum
{
   E_COMP_ENGINE_NONE = 0,
   E_COMP_ENGINE_SW = 1,
   E_COMP_ENGINE_GL = 2
} E_Comp_Engine;

EINTERN Eina_Bool e_comp_init(void);
EAPI E_Comp *e_comp_new(void);
EAPI int e_comp_internal_save(void);
EINTERN int e_comp_shutdown(void);
EAPI void e_comp_render_queue(E_Comp *c);
EAPI void e_comp_shape_queue(E_Comp *c);
EAPI void e_comp_shape_queue_block(E_Comp *c, Eina_Bool block);
EAPI E_Comp_Config *e_comp_config_get(void);
EAPI const Eina_List *e_comp_list(void);
EAPI void e_comp_shadows_reset(void);
EINA_DEPRECATED EAPI E_Comp *e_comp_get(const void *o);
EAPI Ecore_Window e_comp_top_window_at_xy_get(E_Comp *c, Evas_Coord x, Evas_Coord y);
EAPI void e_comp_util_wins_print(const E_Comp *c);
EAPI void e_comp_ignore_win_add(E_Pixmap_Type type, Ecore_Window win);
EAPI void e_comp_ignore_win_del(E_Pixmap_Type type, Ecore_Window win);
EAPI Eina_Bool e_comp_ignore_win_find(Ecore_Window win);
EAPI void e_comp_override_del(E_Comp *c);
EAPI void e_comp_override_add(E_Comp *c);
EAPI void e_comp_block_window_add(void);
EAPI void e_comp_block_window_del(void);
EAPI E_Comp *e_comp_find_by_window(Ecore_Window win);
EAPI void e_comp_override_timed_pop(E_Comp *c);
EAPI unsigned int e_comp_e_object_layer_get(const E_Object *obj);
EAPI Eina_Bool e_comp_grab_input(E_Comp *c, Eina_Bool mouse, Eina_Bool kbd);
EAPI void e_comp_ungrab_input(E_Comp *c, Eina_Bool mouse, Eina_Bool kbd);
EAPI void e_comp_gl_set(Eina_Bool set);
EAPI Eina_Bool e_comp_gl_get(void);
EAPI E_Comp *e_comp_evas_find(const Evas *e);

EAPI void e_comp_button_bindings_grab_all(void);
EAPI void e_comp_button_bindings_ungrab_all(void);
EAPI void e_comp_client_redirect_toggle(E_Client *ec);
EAPI Eina_Bool e_comp_util_object_is_above_nocomp(Evas_Object *obj);

EINTERN Evas_Object *e_comp_style_selector_create(Evas *evas, const char **source);
EAPI E_Config_Dialog *e_int_config_comp(Evas_Object *parent, const char *params);
EAPI E_Config_Dialog *e_int_config_comp_match(Evas_Object *parent, const char *params);

EINA_DEPRECATED static inline E_Comp *
e_comp_util_evas_object_comp_get(Evas_Object *obj)
{
   return ecore_evas_data_get(ecore_evas_ecore_evas_get(evas_object_evas_get(obj)), "comp");
}

static inline Eina_Bool
e_comp_util_client_is_fullscreen(const E_Client *ec)
{
   if ((!ec->visible) || (ec->input_only))
     return EINA_FALSE;
   return ((ec->client.x == 0) && (ec->client.y == 0) &&
       ((ec->client.w) >= ec->comp->man->w) &&
       ((ec->client.h) >= ec->comp->man->h) &&
       (!ec->argb) && (!ec->shaped)
       );
}

extern EAPI E_Comp *e_comp;

#endif
#endif
