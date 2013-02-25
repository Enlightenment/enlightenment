#ifdef E_TYPEDEFS
typedef struct _E_Comp      E_Comp;
typedef struct _E_Comp_Win  E_Comp_Win;
typedef struct _E_Comp_Zone E_Comp_Zone;
typedef struct E_Event_Comp E_Event_Comp;

typedef enum
{
   E_COMP_CANVAS_LAYER_BOTTOM = -100,
   E_COMP_CANVAS_LAYER_BG = -1, // zone bg stuff
   E_COMP_CANVAS_LAYER_DESKTOP = 0, // desktop objects: fileman, gadgets, shelves
   E_COMP_CANVAS_LAYER_DESKTOP_TOP = 10, // raised desktop objects: gadgets, shelves
   E_COMP_CANVAS_LAYER_LAYOUT = 100, // should be nothing else on this layer
   E_COMP_CANVAS_LAYER_POPUP = 999, // popups
   E_COMP_CANVAS_LAYER_MENU = 5000, // menus
   E_COMP_CANVAS_LAYER_DESKLOCK = 9999, // desklock
   E_COMP_CANVAS_LAYER_MAX = 32767 // EVAS_LAYER_MAX
} E_Comp_Canvas_Layer;

typedef enum _E_Layer
{
   E_LAYER_DESKTOP = 0,
   E_LAYER_BELOW = 50,
   E_LAYER_NORMAL = 100,
   E_LAYER_ABOVE = 150,
   E_LAYER_EDGE = 200,
   E_LAYER_FULLSCREEN = 250,
   E_LAYER_EDGE_FULLSCREEN = 300,
   E_LAYER_POPUP = 300,
   E_LAYER_TOP = 350,
   E_LAYER_DRAG = 400,
   E_LAYER_PRIO = 450
} E_Layer;

typedef enum
{
   E_COMP_CANVAS_STACK_UNDER = -1,
   E_COMP_CANVAS_STACK_NONE = 0,
   E_COMP_CANVAS_STACK_ABOVE = 1
} E_Comp_Canvas_Stack;

#else
#ifndef E_MOD_COMP_H
#define E_MOD_COMP_H

# include "e_comp_cfdata.h"
# include "e_comp_render_update.h"


struct _E_Comp
{
   Ecore_X_Window  win; // input overlay
   Ecore_Evas     *ee;
   Ecore_X_Window  ee_win;
   Evas           *evas;
   Evas_Object    *layout;
   Eina_List      *zones;
   E_Manager      *man;
   E_Pointer      *pointer;

   Eina_List *debug_rects;
   Eina_List *ignore_wins;

   Eina_Inlist    *wins;
   Eina_List      *wins_list;
   Eina_List      *updates;
   Ecore_Animator *render_animator;
   Ecore_Job      *shape_job;
   Ecore_Job      *update_job;
   Ecore_Timer    *new_up_timer;
   Evas_Object    *fps_bg;
   Evas_Object    *fps_fg;
   Ecore_Job      *screen_job;
   Ecore_Timer    *nocomp_delay_timer;
   Ecore_Timer    *nocomp_override_timer;
   int             animating;
   int             render_overflow;
   double          frametimes[122];
   int             frameskip;

   int             nocomp_override; //number of times nocomp override has been requested
   Ecore_X_Window block_win;
   int             block_count; //number of times block window has been requested

   Ecore_X_Window  cm_selection;

   Eina_Bool       gl : 1;
   Eina_Bool       grabbed : 1;
   Eina_Bool       nocomp : 1;
   Eina_Bool       nocomp_want : 1;
   Eina_Bool       wins_invalid : 1;
   Eina_Bool       saver : 1;
};

struct _E_Comp_Zone
{
   E_Comp      *comp;
   E_Zone      *zone;    // never deref - just use for handle cmp's
   Evas_Object *base;
   Evas_Object *over;
   int          container_num;
   int          zone_num;
   int          x, y, w, h;
   double       bl;
   Eina_Bool    bloff;
};

struct _E_Comp_Win
{
   EINA_INLIST;

   E_Comp              *c;  // parent compositor
   Ecore_X_Window       win;  // raw window - for menus etc.
   E_Container_Shape *shape;
   E_Border            *bd;  // if its a border - later
   E_Popup             *pop;  // if its a popup - later
   E_Menu              *menu;  // if it is a menu - later
   int                  x, y, w, h;  // geometry
   Eina_Rectangle     hidden; // hidden geometry (used when its unmapped and re-instated on map)
   int                  pw, ph;  // pixmap w/h
   int                  border;  // border width
   Ecore_X_Pixmap       pixmap;  // the compositing pixmap
   Ecore_X_Damage       damage;  // damage region
   Ecore_X_Visual       vis;  // window visual
   Ecore_X_Colormap     cmap; // colormap of window
   int                  depth;  // window depth
   Evas_Object         *obj;  // composite object
   Evas_Object         *shobj;  // shadow object
   E_Object            *eobj; // internal e object
   E_Comp_Win          *cw_above; // comp win that should always be stacked above this one
   Eina_List           *stack_below; // list of objects to keep stacked below this one
   Eina_List           *obj_mirror;  // extra mirror objects
   Ecore_X_Image       *xim;  // x image - software fallback
   E_Comp_Render_Update            *up;  // update handler
   E_Object_Delfn      *dfn;  // delete function handle for objects being tracked
   Ecore_X_Sync_Counter counter;  // sync counter for syncronised drawing
   Ecore_Timer         *update_timeout;  // max time between damage and "done" event
   Ecore_Timer         *ready_timeout;  // max time on show (new window draw) to wait for window contents to be ready if sync protocol not handled. this is fallback.
   int                  dmg_updates;  // num of damage event updates since a redirect
   Ecore_X_Rectangle   *rects;  // shape rects... if shaped :(
   int                  rects_num;  // num rects above

   Ecore_X_Pixmap       cache_pixmap;  // the cached pixmap (1/nth the dimensions)
   int                  cache_w, cache_h;  // cached pixmap size
   int                  update_count;  // how many updates have happened to this win
   double               last_visible_time;  // last time window was visible
   double               last_draw_time;  // last time window was damaged

   int                  pending_count;  // pending event count

   unsigned int         opacity;  // opacity set with _NET_WM_WINDOW_OPACITY
   Ecore_Timer        *opacity_set_timer; // timer for setting opacity in ecore-x to avoid roundtrips

   char                *title, *name, *clas, *role;  // fetched for override-redirect windowa
   Ecore_X_Window_Type  primary_type;  // fetched for override-redirect windowa

   unsigned char        misses; // number of sync misses

   Eina_Bool            delete_pending : 1;  // delete pendig
   Eina_Bool            hidden_override : 1;  // hidden override
   Eina_Bool            animating : 1;  // it's busy animating - defer hides/dels
   Eina_Bool            force : 1;  // force del/hide even if animating
   Eina_Bool            defer_hide : 1;  // flag to get hide to work on deferred hide
   Eina_Bool            delete_me : 1;  // delete me!
   Eina_Bool            visible : 1;  // is visible
   Eina_Bool            input_only : 1;  // is input_only

   Eina_Bool            override : 1;  // is override-redirect
   Eina_Bool            argb : 1;  // is argb
   Eina_Bool            shaped : 1;  // is shaped
   Eina_Bool            update : 1;  // has updates to fetch
   Eina_Bool            redirected : 1;  // has updates to fetch
   Eina_Bool            shape_changed : 1;  // shape changed
   Eina_Bool            native : 1;  // native
   Eina_Bool            drawme : 1;  // drawme flag fo syncing rendering

   Eina_Bool            invalid : 1;  // invalid depth used - just use as marker
   Eina_Bool            nocomp : 1;  // nocomp applied
   Eina_Bool            nocomp_need_update : 1;  // nocomp in effect, but this window updated while in nocomp mode
   Eina_Bool            needpix : 1;  // need new pixmap
   Eina_Bool            needxim : 1;  // need new xim
   Eina_Bool            real_hid : 1;  // last hide was a real window unmap
   Eina_Bool            inhash : 1;  // is in the windows hash
   Eina_Bool            show_ready : 1;  // is this window ready for its first show

   Eina_Bool            show_anim : 1; // ran show animation

   Eina_Bool            bg_win : 1; // window is the bg win for a container
   Eina_Bool            free_shape : 1; // container shape needs to be freed
   Eina_Bool            real_obj : 1;  // real object (for dummy comp wins)
   Eina_Bool            not_in_layout : 1; // object is a dummy not in comp layout
};

struct E_Event_Comp
{
   E_Comp_Win *cw;
};

extern EAPI int E_EVENT_COMP_SOURCE_VISIBILITY;
extern EAPI int E_EVENT_COMP_SOURCE_ADD;
extern EAPI int E_EVENT_COMP_SOURCE_DEL;
extern EAPI int E_EVENT_COMP_SOURCE_CONFIGURE;

typedef enum
{
   E_COMP_ENGINE_NONE = 0,
   E_COMP_ENGINE_SW = 1,
   E_COMP_ENGINE_GL = 2
} E_Comp_Engine;

EINTERN Eina_Bool e_comp_init(void);
EINTERN int      e_comp_shutdown(void);
EINTERN Eina_Bool e_comp_manager_init(E_Manager *man);

EAPI const Eina_List *e_comp_list(void);

EAPI int e_comp_internal_save(void);
EAPI E_Comp_Config *e_comp_config_get(void);
EAPI void e_comp_shadows_reset(void);

EAPI void e_comp_block_window_add(void);
EAPI void e_comp_block_window_del(void);

EAPI void e_comp_render_update(E_Comp *c);
EAPI void e_comp_zone_update(E_Comp_Zone *cz);

EAPI void e_comp_override_del(E_Comp *c);
EAPI void e_comp_override_add(E_Comp *c);

EAPI E_Comp_Win *e_comp_win_find_client_win(Ecore_X_Window win);
EAPI E_Comp_Win *e_comp_win_find(Ecore_X_Window win);
EAPI const Eina_List *e_comp_win_list_get(E_Comp *c);
EAPI Evas_Object *e_comp_win_image_mirror_add(E_Comp_Win *cw);
EAPI void e_comp_win_hidden_set(E_Comp_Win *cw, Eina_Bool hidden);
EAPI void e_comp_win_opacity_set(E_Comp_Win *cw, unsigned int opacity);

EAPI E_Comp *e_comp_get(void *o);

EAPI Ecore_X_Window e_comp_top_window_at_xy_get(E_Comp *c, Evas_Coord x, Evas_Coord y, Ecore_X_Window *ignore, unsigned int ignore_num);

/* for injecting objects into the comp layout */
EAPI E_Comp_Win *e_comp_object_inject(E_Comp *c, Evas_Object *obj, E_Object *eobj, E_Layer layer);
/* for giving objects the comp theme and such without injecting into layout */
EAPI E_Comp_Win *e_comp_object_add(E_Comp *c, Evas_Object *obj, E_Object *eobj);

EAPI void e_comp_win_move(E_Comp_Win *cw, Evas_Coord x, Evas_Coord y);
EAPI void e_comp_win_resize(E_Comp_Win *cw, int w, int h);
EAPI void e_comp_win_moveresize(E_Comp_Win *cw, Evas_Coord x, Evas_Coord y, int w, int h);
EAPI void e_comp_win_hide(E_Comp_Win *cw);
EAPI void e_comp_win_show(E_Comp_Win *cw);
EAPI void e_comp_win_del(E_Comp_Win *cw);
EAPI void e_comp_win_reshadow(E_Comp_Win *cw);

EAPI void e_comp_ignore_win_add(Ecore_X_Window win);

#define E_LAYER_SET(obj, layer) e_comp_canvas_layer_set(obj, layer, 0, E_COMP_CANVAS_STACK_NONE)
#define E_LAYER_SET_UNDER(obj, layer) e_comp_canvas_layer_set(obj, layer, 0, E_COMP_CANVAS_STACK_UNDER)
#define E_LAYER_SET_ABOVE(obj, layer) e_comp_canvas_layer_set(obj, layer, 0, E_COMP_CANVAS_STACK_ABOVE)
#define E_LAYER_LAYOUT_ADD(obj, layer) e_comp_canvas_layer_set(obj, E_COMP_CANVAS_LAYER_LAYOUT, layer, E_COMP_CANVAS_STACK_NONE)
#define E_LAYER_LAYOUT_ADD_UNDER(obj, layer) e_comp_canvas_layer_set(obj, E_COMP_CANVAS_LAYER_LAYOUT, layer, E_COMP_CANVAS_STACK_UNDER)
#define E_LAYER_LAYOUT_ADD_ABOVE(obj, layer) e_comp_canvas_layer_set(obj, E_COMP_CANVAS_LAYER_LAYOUT, layer, E_COMP_CANVAS_STACK_ABOVE)

EAPI E_Comp_Win *e_comp_canvas_layer_set(Evas_Object *obj, E_Comp_Canvas_Layer comp_layer, E_Layer layer, E_Comp_Canvas_Stack stack);
EAPI unsigned int e_comp_e_object_layer_get(const E_Object *obj);

static inline E_Comp *
e_comp_util_evas_object_comp_get(Evas_Object *obj)
{
   return ecore_evas_data_get(ecore_evas_ecore_evas_get(evas_object_evas_get(obj)), "comp");
}

static inline Eina_Bool
e_comp_evas_exists(void *o)
{
   E_Comp *c;

   EINA_SAFETY_ON_NULL_RETURN_VAL(o, EINA_FALSE);
   c = e_comp_get(o);
   return c ? !!c->evas : EINA_FALSE;
}

static inline void
e_comp_win_ignore_events_set(E_Comp_Win *cw, Eina_Bool ignore)
{
   EINA_SAFETY_ON_NULL_RETURN(cw);
   ignore = !!ignore;
   evas_object_pass_events_set(cw->shobj, ignore);
}

static inline unsigned int
e_comp_e_object_layer_effective_get(const E_Object *obj)
{
   unsigned int layer;

   layer = e_comp_e_object_layer_get(obj);
   if ((layer > E_COMP_CANVAS_LAYER_LAYOUT) && (layer < E_COMP_CANVAS_LAYER_POPUP))
     layer = E_COMP_CANVAS_LAYER_LAYOUT;
   return layer;
}

EAPI void e_comp_util_wins_print(const E_Comp *c);

#endif
#endif
