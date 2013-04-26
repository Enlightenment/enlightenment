#ifdef E_TYPEDEFS

#else
# ifndef E_COMP_WL_H
#  define E_COMP_WL_H

#  include <pixman.h>
#  include <wayland-server.h>
#  include <xkbcommon/xkbcommon.h>

/* headers for terminal support */
#  include <termios.h>
#  include <linux/vt.h>

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

#  ifdef HAVE_WAYLAND_EGL
#   include <EGL/egl.h>
#   include <EGL/eglext.h>
#  endif

#  define container_of(ptr, type, member) ({ \
   const __typeof__(((type *)0)->member) *__mptr = (ptr); \
   (type *)((char *)__mptr - offsetof(type,member));})

typedef enum _E_Wayland_Shell_Surface_Type E_Wayland_Shell_Surface_Type;

typedef struct _E_Wayland_Region E_Wayland_Region;
typedef struct _E_Wayland_Surface E_Wayland_Surface;
typedef struct _E_Wayland_Surface_Frame_Callback E_Wayland_Surface_Frame_Callback;
typedef struct _E_Wayland_Shell_Surface E_Wayland_Shell_Surface;
typedef struct _E_Wayland_Shell_Interface E_Wayland_Shell_Interface;
typedef struct _E_Wayland_Shell_Grab E_Wayland_Shell_Grab;
typedef struct _E_Wayland_Keyboard_Info E_Wayland_Keyboard_Info;
typedef struct _E_Wayland_Input E_Wayland_Input;
typedef struct _E_Wayland_Compositor E_Wayland_Compositor;
typedef struct _E_Wayland_Output E_Wayland_Output;
typedef struct _E_Wayland_Output_Mode E_Wayland_Output_Mode;
typedef struct _E_Wayland_Terminal E_Wayland_Terminal;
typedef struct _E_Wayland_Plane E_Wayland_Plane;

enum _E_Wayland_Shell_Surface_Type
{
   E_WAYLAND_SHELL_SURFACE_TYPE_NONE,
     E_WAYLAND_SHELL_SURFACE_TYPE_TOPLEVEL,
     E_WAYLAND_SHELL_SURFACE_TYPE_TRANSIENT,
     E_WAYLAND_SHELL_SURFACE_TYPE_FULLSCREEN,
     E_WAYLAND_SHELL_SURFACE_TYPE_MAXIMIZED,
     E_WAYLAND_SHELL_SURFACE_TYPE_POPUP
};

struct _E_Wayland_Region
{
   struct 
     {
        struct wl_resource resource;
     } wl;

   pixman_region32_t region;
};

struct _E_Wayland_Surface_Frame_Callback
{
   struct  
    {
        struct wl_resource resource;
        struct wl_list link;
     } wl;
};

struct _E_Wayland_Surface
{
   struct 
     {
        struct wl_surface surface;
        struct wl_list link, frames;
     } wl;

   struct 
     {
        struct wl_buffer *buffer;
        struct wl_listener buffer_destroy;
        struct wl_list frames;
        Evas_Coord x, y;
        Eina_Bool new_buffer : 1;
        pixman_region32_t damage, opaque, input;
     } pending;

   struct 
     {
        struct wl_buffer *buffer;
        struct wl_listener buffer_destroy;
     } reference;

   struct 
     {
        Evas_Coord x, y;
        Evas_Coord w, h;
        Eina_Bool changed : 1;
     } geometry;

   struct 
     {
        pixman_region32_t opaque, input;
        pixman_region32_t damage, clip;
     } region;

   /* smart object for this surface */
   Evas_Object *obj;

   Ecore_Evas *ee;
   Ecore_X_Window evas_win;
   Evas *evas;

   E_Border *bd;
   Eina_List *bd_hooks;

   E_Wayland_Shell_Surface *shell_surface;
   Eina_Bool mapped : 1;

   E_Wayland_Input *input;

   void (*map) (E_Wayland_Surface *ews, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
   void (*unmap) (E_Wayland_Surface *ews);
   void (*configure) (E_Wayland_Surface *ews, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
};

struct _E_Wayland_Shell_Surface
{
   struct 
     {
        struct wl_resource resource;
        struct wl_listener surface_destroy;
        struct wl_list link;
     } wl;

   struct 
     {
        Evas_Coord x, y;
        Evas_Coord w, h;
        Eina_Bool valid : 1;
     } saved;

   struct 
     {
        struct wl_pointer_grab grab;
        struct wl_seat *seat;
        struct wl_listener parent_destroy;
        int x, y;
        Eina_Bool up : 1;
        unsigned int serial;
     } popup;

   struct 
     {
        Evas_Coord x, y;
        unsigned int flags;
     } transient;

   E_Wayland_Surface *surface, *parent;
   E_Wayland_Shell_Surface_Type type, next_type;

   char *title, *clas;

   Eina_Bool active : 1;

   void *ping_timer;
};

struct _E_Wayland_Shell_Interface
{
   void *shell;

   E_Wayland_Shell_Surface *(*shell_surface_create) (void *shell, E_Wayland_Surface *ews, const void *client);
   void (*toplevel_set) (E_Wayland_Shell_Surface *ewss);
   void (*transient_set) (E_Wayland_Shell_Surface *ewss, E_Wayland_Surface *ews, int x, int y, unsigned int flags);
   void (*fullscreen_set) (E_Wayland_Shell_Surface *ewss, unsigned int method, unsigned int framerate, void *output);
   void (*popup_set) (E_Wayland_Shell_Surface *ewss, E_Wayland_Surface *ews, void *seat, unsigned int serial, int x, int y, unsigned int flags);
   void (*maximized_set) (E_Wayland_Shell_Surface *ewss, unsigned int edges);
   int (*move) (E_Wayland_Shell_Surface *ewss, void *seat);
   int (*resize) (E_Wayland_Shell_Surface *ewss, void *seat, unsigned int edges);
};

struct _E_Wayland_Shell_Grab
{
   struct wl_pointer_grab grab;
   struct wl_pointer *pointer;
   Evas_Coord x, y, w, h;
   unsigned int edges;

   E_Wayland_Shell_Surface *shell_surface;
   struct wl_listener shell_surface_destroy;
};

struct _E_Wayland_Keyboard_Info
{
   struct xkb_keymap *keymap;
   int fd;
   size_t size;
   char *area;
   xkb_mod_index_t mod_shift, mod_caps, mod_ctrl, mod_alt, mod_super;
};

struct _E_Wayland_Input
{
   struct 
     {
        struct wl_seat seat;
        struct wl_pointer pointer;
        struct wl_keyboard keyboard;
        struct wl_resource *keyboard_resource;
        struct wl_list link;
     } wl;

   struct 
     {
        E_Wayland_Keyboard_Info *info;
        struct xkb_state *state;
     } xkb;

   struct 
     {
        E_Wayland_Surface *surface;
        struct wl_listener surface_destroy;
        struct 
          {
             Evas_Coord x, y;
          } hot;
     } pointer;

   Eina_Bool has_pointer : 1;
   Eina_Bool has_keyboard : 1;
   Eina_Bool has_touch : 1;
};

struct _E_Wayland_Compositor 
{
   struct 
     {
        struct wl_display *display;
        struct wl_event_loop *loop;
     } wl;

   struct 
     {
        struct wl_signal destroy;
        struct wl_signal activate;
        struct wl_signal kill;
        struct wl_signal seat;
     } signals;

   struct 
     {
        struct xkb_rule_names names;
        struct xkb_context *context;
     } xkb;

#ifdef HAVE_WAYLAND_EGL
   struct 
     {
        EGLDisplay display;
        EGLContext context;
        EGLConfig config;
        PFNEGLBINDWAYLANDDISPLAYWL bind_display;
        PFNEGLUNBINDWAYLANDDISPLAYWL unbind_display;
        Eina_Bool bound : 1;
     } egl;
#endif

   E_Wayland_Shell_Interface shell_interface;

   Ecore_Event_Handler *kbd_handler;
   Ecore_Fd_Handler *fd_handler;
   Ecore_Idler *idler;

   E_Wayland_Input *input;

   Eina_List *surfaces;
   Eina_List *seats;

   struct wl_list outputs;

   void (*ping_cb) (E_Wayland_Surface *ews, unsigned int serial);
};

struct _E_Wayland_Output
{
   unsigned int id;

   void *render_state;

   struct 
     {
        struct wl_list link;
        struct wl_list resources;
        struct wl_global *global;
     } wl;

   E_Wayland_Compositor *compositor;

   struct 
     {
        Evas_Coord x, y, w, h;
        Evas_Coord mm_w, mm_h;
     } geometry;

   pixman_region32_t region, prev_damage;

   Eina_Bool repaint_needed : 1;
   Eina_Bool repaint_scheduled : 1;

   Eina_Bool dirty : 1;

   struct 
     {
        struct wl_signal sig;
        unsigned int timestamp;
     } frame;

   char *make, *model;
   unsigned int subpixel, transform;

   struct 
     {
        E_Wayland_Output_Mode *current;
        E_Wayland_Output_Mode *origin;
        struct wl_list list;
     } mode;

   void (*repaint) (E_Wayland_Output *output, pixman_region32_t *damage);
   void (*destroy) (E_Wayland_Output *output);
   int (*switch_mode) (E_Wayland_Output *output, E_Wayland_Output_Mode *mode);

   /* TODO: add backlight support */
};

struct _E_Wayland_Output_Mode
{
   struct 
     {
        struct wl_list link;
     } wl;

   unsigned int flags;
   Evas_Coord w, h;
   unsigned int refresh;
};

struct _E_Wayland_Terminal
{
   E_Wayland_Compositor *compositor;

   int fd, kbd_mode;

   struct termios attributes;

   struct 
     {
        struct wl_event_source *input;
        struct wl_event_source *vt;
     } wl;

   struct 
     {
        void (*func) (E_Wayland_Compositor *compositor, int event);
        int current, starting;
        Eina_Bool exists : 1;
     } vt;
};

struct _E_Wayland_Plane
{
   pixman_region32_t damage, clip;
   Evas_Coord x, y;
   struct wl_list link;
};

/* external variables */
extern E_Wayland_Compositor *_e_wl_comp;

EINTERN Eina_Bool e_comp_wl_init(void);
EINTERN void e_comp_wl_shutdown(void);

EAPI unsigned int e_comp_wl_time_get(void);
EAPI void e_comp_wl_input_modifiers_update(unsigned int serial);

# endif
#endif
