#ifdef E_TYPEDEFS
#  ifndef HAVE_WAYLAND_ONLY
#   include "e_comp_x.h"
#  endif
#else
# ifndef E_COMP_WL_H
#  define E_COMP_WL_H

/* NB: Turn off shadow warnings for Wayland includes */
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wshadow"
#  define WL_HIDE_DEPRECATED
#  include <wayland-server.h>
#  pragma GCC diagnostic pop

#  include <xkbcommon/xkbcommon.h>

#  ifndef HAVE_WAYLAND_ONLY
#   include "e_comp_x.h"
#  endif

/* #  ifdef HAVE_WAYLAND_EGL */
/* #   include <EGL/egl.h> */
/* #   define GL_GLEXT_PROTOTYPES */
/* #  endif */

#  ifdef __linux__
#   include <linux/input.h>
#  else
#   define BTN_LEFT 0x110
#   define BTN_RIGHT 0x111
#   define BTN_MIDDLE 0x112
#   define BTN_SIDE 0x113
#   define BTN_EXTRA 0x114
#   define BTN_FORWARD 0x115
#   define BTN_BACK 0x116
#  endif

#  define container_of(ptr, type, member) \
   ({ \
      const __typeof__( ((type *)0)->member ) *__mptr = (ptr); \
      (type *)( (char *)__mptr - offsetof(type,member) ); \
   })

typedef struct _E_Comp_Wl_Buffer E_Comp_Wl_Buffer;
typedef struct _E_Comp_Wl_Buffer_Ref E_Comp_Wl_Buffer_Ref;
typedef struct _E_Comp_Wl_Subsurf_Data E_Comp_Wl_Subsurf_Data;
typedef struct _E_Comp_Wl_Surface_State E_Comp_Wl_Surface_State;
typedef struct _E_Comp_Wl_Client_Data E_Comp_Wl_Client_Data;
typedef struct _E_Comp_Wl_Data E_Comp_Wl_Data;
typedef struct _E_Comp_Wl_Output E_Comp_Wl_Output;

struct _E_Comp_Wl_Buffer
{
   struct wl_resource *resource;
   struct wl_signal destroy_signal;
   struct wl_listener destroy_listener;
   struct wl_shm_buffer *shm_buffer;
   int32_t w, h;
   uint32_t busy;
};

struct _E_Comp_Wl_Buffer_Ref
{
   E_Comp_Wl_Buffer *buffer;
   struct wl_listener destroy_listener;
};

struct _E_Comp_Wl_Surface_State
{
   int sx, sy;
   int bw, bh;
   E_Comp_Wl_Buffer *buffer;
   struct wl_listener buffer_destroy_listener;
   Eina_List *damages, *frames;
   Eina_Tiler *input, *opaque;
   Eina_Bool new_attach : 1;
   Eina_Bool has_data : 1;
};

struct _E_Comp_Wl_Subsurf_Data
{
   struct wl_resource *resource;

   E_Client *parent;

   struct
     {
        int x, y;
        Eina_Bool set;
     } position;

   E_Comp_Wl_Surface_State cached;
   E_Comp_Wl_Buffer_Ref cached_buffer_ref;

   Eina_Bool synchronized;
};

struct _E_Comp_Wl_Data
{
   struct
     {
        struct wl_display *disp;
        struct wl_registry *registry; // only used for nested wl compositors
        /* struct wl_event_loop *loop; */
        Eina_Inlist *globals;  // only used for nested wl compositors
        struct wl_shm *shm;  // only used for nested wl compositors
        Evas_GL *gl;
        Evas_GL_Config *glcfg;
        Evas_GL_Context *glctx;
        Evas_GL_Surface *glsfc;
        Evas_GL_API *glapi;
     } wl;

   struct
     {
        struct
          {
             struct wl_signal create;
             struct wl_signal activate;
             struct wl_signal kill;
          } surface;
        /* NB: At the moment, we don't need these */
        /*      struct wl_signal destroy; */
        /*      struct wl_signal activate; */
        /*      struct wl_signal transform; */
        /*      struct wl_signal kill; */
        /*      struct wl_signal idle; */
        /*      struct wl_signal wake; */
        /*      struct wl_signal session; */
        /*      struct  */
        /*        { */
        /*           struct wl_signal created; */
        /*           struct wl_signal destroyed; */
        /*           struct wl_signal moved; */
        /*        } seat, output; */
     } signals;

   struct
     {
        struct wl_resource *shell;
        struct wl_resource *xdg_shell;
     } shell_interface;

   struct
     {
        Eina_List *resources;
        Eina_List *focused;
        Eina_Bool enabled : 1;
        xkb_mod_index_t mod_shift, mod_caps;
        xkb_mod_index_t mod_ctrl, mod_alt;
        xkb_mod_index_t mod_super;
        xkb_mod_mask_t mod_depressed, mod_latched, mod_locked;
        xkb_layout_index_t mod_group;
        struct wl_array keys;
        struct wl_resource *focus;
        int mod_changed;
        int repeat_delay;
        int repeat_rate;
     } kbd;

   struct
     {
        Eina_List *resources;
        wl_fixed_t x, y;
        wl_fixed_t grab_x, grab_y;
        uint32_t button;
        E_Client *ec;
        Eina_Bool enabled : 1;
     } ptr;

   struct
     {
        Eina_List *resources;
        Eina_Bool enabled : 1;
     } touch;

   struct
     {
        struct wl_global *global;
        Eina_List *resources;
        uint32_t version;
        char *name;

        struct
          {
             struct wl_global *global;
             struct wl_resource *resource;
          } im;
     } seat;

   struct
     {
        struct wl_global *global;
        struct wl_resource *resource;
        Eina_Hash *data_resources;
     } mgr;

   struct
     {
        void *data_source;
        uint32_t serial;
        struct wl_signal signal;
        struct wl_listener data_source_listener;
        E_Client *target;
     } selection;

   struct
     {
        void *source;
        struct wl_listener listener;
        E_Client *xwl_owner;
     } clipboard;

   struct
     {
        struct wl_resource *resource;
        uint32_t edges;
     } resize;

   struct
     {
        struct xkb_keymap *keymap;
        struct xkb_context *context;
        struct xkb_state *state;
        int fd;
        size_t size;
        char *area;
     } xkb;

   struct
     {
        struct wl_global *global;
        struct wl_client *client;
        void (*read_pixels)(E_Comp_Wl_Output *output, void *pixels);
     } screenshooter;

   Eina_List *outputs;

   Ecore_Fd_Handler *fd_hdlr;
   Ecore_Idler *idler;

   struct wl_client *xwl_client;
   Eina_List *xwl_pending;

   E_Drag *drag;
   E_Client *drag_client;
   void *drag_source;
};

struct _E_Comp_Wl_Client_Data
{
   Ecore_Timer *on_focus_timer;

   struct
     {
        E_Comp_Wl_Subsurf_Data *data;
        E_Client *restack_target;
        Eina_List *list;
     } sub;

   /* regular surface resource (wl_compositor_create_surface) */
   struct wl_resource *surface;
   struct wl_signal destroy_signal;

   struct
     {
        /* shell surface resource */
        struct wl_resource *surface;

        void (*configure_send)(struct wl_resource *resource, uint32_t edges, int32_t width, int32_t height);
        void (*configure)(struct wl_resource *resource, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
        void (*ping)(struct wl_resource *resource);
        void (*map)(struct wl_resource *resource);
        void (*unmap)(struct wl_resource *resource);
        Eina_Rectangle window;
     } shell;

   E_Comp_Wl_Buffer_Ref buffer_ref;
   E_Comp_Wl_Surface_State pending;

   Eina_List *frames;

   struct
     {
        int32_t x, y;
     } popup;
#ifndef HAVE_WAYLAND_ONLY
   E_Pixmap *xwayland_pixmap;
   E_Comp_X_Client_Data *xwayland_data;
#endif

   Eina_Bool keep_buffer : 1;
   Eina_Bool mapped : 1;
   Eina_Bool change_icon : 1;
   Eina_Bool need_reparent : 1;
   Eina_Bool reparented : 1;
   Eina_Bool evas_init : 1;
   Eina_Bool first_damage : 1;
   Eina_Bool set_win_type : 1;
   Eina_Bool frame_update : 1;
   Eina_Bool maximize_pre : 1;
};

struct _E_Comp_Wl_Output
{
   struct wl_global *global;
   Eina_List *resources;
   const char *id, *make, *model;
   int x, y, w, h;
   int phys_width, phys_height;
   unsigned int refresh;
   unsigned int subpixel;
   unsigned int transform;
   double scale;

   /* added for screenshot ability */
   struct wl_output *wl_output;
   struct wl_buffer *buffer;
   void *data;
};

E_API Eina_Bool e_comp_wl_init(void);
EINTERN void e_comp_wl_shutdown(void);

EINTERN struct wl_resource *e_comp_wl_surface_create(struct wl_client *client, int version, uint32_t id);
EINTERN void e_comp_wl_surface_destroy(struct wl_resource *resource);
EINTERN void e_comp_wl_surface_attach(E_Client *ec, E_Comp_Wl_Buffer *buffer);
EINTERN Eina_Bool e_comp_wl_surface_commit(E_Client *ec);
EINTERN Eina_Bool e_comp_wl_subsurface_commit(E_Client *ec);
EINTERN void e_comp_wl_buffer_reference(E_Comp_Wl_Buffer_Ref *ref, E_Comp_Wl_Buffer *buffer);
E_API E_Comp_Wl_Buffer *e_comp_wl_buffer_get(struct wl_resource *resource);

E_API struct wl_signal e_comp_wl_surface_create_signal_get(void);
E_API double e_comp_wl_idle_time_get(void);
E_API Eina_Bool e_comp_wl_output_init(const char *id, const char *make, const char *model, int x, int y, int w, int h, int pw, int ph, unsigned int refresh, unsigned int subpixel, unsigned int transform);
E_API void e_comp_wl_output_remove(const char *id);

EINTERN Eina_Bool e_comp_wl_key_down(Ecore_Event_Key *ev);
EINTERN Eina_Bool e_comp_wl_key_up(Ecore_Event_Key *ev);
E_API Eina_Bool e_comp_wl_evas_handle_mouse_button(E_Client *ec, uint32_t timestamp, uint32_t button_id, uint32_t state);

E_API extern int E_EVENT_WAYLAND_GLOBAL_ADD;

E_API extern Ecore_Wl2_Display *ewd;

# ifndef HAVE_WAYLAND_ONLY
EINTERN void e_comp_wl_xwayland_client_queue(E_Client *ec);
static inline E_Comp_X_Client_Data *
e_comp_wl_client_xwayland_data(const E_Client *ec)
{
   return ec->comp_data ? ((E_Comp_Wl_Client_Data*)ec->comp_data)->xwayland_data : NULL;
}

static inline E_Pixmap *
e_comp_wl_client_xwayland_pixmap(const E_Client *ec)
{
   return ec->comp_data ?  ((E_Comp_Wl_Client_Data*)ec->comp_data)->xwayland_pixmap : NULL;
}

static inline void
e_comp_wl_client_xwayland_setup(E_Client *ec, E_Comp_X_Client_Data *cd, E_Pixmap *ep)
{
   if (cd && ep)
     {
        ((E_Comp_Wl_Client_Data*)ec->comp_data)->xwayland_data = cd;
        ((E_Comp_Wl_Client_Data*)ec->comp_data)->xwayland_pixmap = ep;
     }
   if (e_comp_wl->xwl_pending)
     e_comp_wl->xwl_pending = eina_list_remove(e_comp_wl->xwl_pending, ec);
}
#  endif
# endif
#endif
