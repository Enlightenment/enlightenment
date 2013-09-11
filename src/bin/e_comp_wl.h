#ifdef E_TYPEDEFS

#else
# ifndef E_COMP_WL_H
#  define E_COMP_WL_H

#  define WL_HIDE_DEPRECATED
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
typedef struct _E_Wayland_Buffer E_Wayland_Buffer;
typedef struct _E_Wayland_Buffer_Reference E_Wayland_Buffer_Reference;

/*
 * NB: All of these structs and interfaces were recently removed from 
 * wayland so for now, reimplement them here 
 */
struct wl_seat;
struct wl_pointer;
struct wl_keyboard;
struct wl_touch;

struct wl_pointer_grab;
struct wl_pointer_grab_interface 
{
   void (*focus)(struct wl_pointer_grab *grab, struct wl_resource *surface, wl_fixed_t x, wl_fixed_t y);
   void (*motion)(struct wl_pointer_grab *grab, unsigned int timestamp, wl_fixed_t x, wl_fixed_t y);
   void (*button)(struct wl_pointer_grab *grab, unsigned int timestamp, unsigned int button, unsigned int state);
};
struct wl_pointer_grab
{
   const struct wl_pointer_grab_interface *interface;
   struct wl_pointer *pointer;
   struct wl_resource *focus;
   wl_fixed_t x, y;
   unsigned int edges;
};

struct wl_keyboard_grab;
struct wl_keyboard_grab_interface
{
   void (*key)(struct wl_keyboard_grab *grab, unsigned int timestamp, unsigned int key, unsigned int state);
   void (*modifiers)(struct wl_keyboard_grab *grab, unsigned int serial, unsigned int mods_depressed, unsigned int mods_latched, unsigned int mods_locked, unsigned int group);
};
struct wl_keyboard_grab
{
   const struct wl_keyboard_grab_interface *interface;
   struct wl_keyboard *keyboard;
   struct wl_resource *focus;
   unsigned int key;
};

struct wl_touch_grab;
struct wl_touch_grab_interface 
{
   void (*down)(struct wl_touch_grab *grab, unsigned int timestamp, int touch_id, wl_fixed_t sx, wl_fixed_t sy);
   void (*up)(struct wl_touch_grab *grab, unsigned int timestamp, int touch_id);
   void (*motion)(struct wl_touch_grab *grab, unsigned int timestamp, int touch_id, wl_fixed_t sx, wl_fixed_t sy);
};
struct wl_touch_grab
{
   const struct wl_touch_grab_interface *interface;
   struct wl_touch *touch;
   struct wl_resource *focus;
};

struct wl_pointer 
{
   struct wl_seat *seat;

   struct wl_list resource_list;
   struct wl_list focus_resource_list;
   struct wl_resource *focus;
   unsigned int focus_serial;
   struct wl_signal focus_signal;

   struct wl_pointer_grab *grab;
   struct wl_pointer_grab default_grab;
   wl_fixed_t grab_x, grab_y;
   unsigned int grab_button;
   unsigned int grab_serial;
   unsigned int grab_time;

   wl_fixed_t x, y;
   struct wl_resource *current;
   struct wl_listener current_listener;
   wl_fixed_t current_x, current_y;

   unsigned int button_count;
};

struct wl_keyboard 
{
   struct wl_seat *seat;

   struct wl_list resource_list;
   struct wl_list focus_resource_list;
   struct wl_resource *focus;
   unsigned int focus_serial;
   struct wl_signal focus_signal;

   struct wl_keyboard_grab *grab;
   struct wl_keyboard_grab default_grab;
   unsigned int grab_key;
   unsigned int grab_serial;
   unsigned int grab_time;

   struct wl_array keys;

   struct 
     {
        unsigned int mods_depressed;
        unsigned int mods_latched;
        unsigned int mods_locked;
        unsigned int group;
     } modifiers;
};

struct wl_touch 
{
   struct wl_seat *seat;

   struct wl_list resource_list;
   struct wl_list focus_resource_list;
   struct wl_resource *focus;
   unsigned int focus_serial;
   struct wl_signal focus_signal;

   struct wl_touch_grab *grab;
   struct wl_touch_grab default_grab;
   wl_fixed_t grab_x, grab_y;
   unsigned int grab_serial;
   unsigned int grab_time;
};

struct wl_data_offer 
{
   struct wl_resource *resource;
   struct wl_data_source *source;
   struct wl_listener source_destroy_listener;
};

struct wl_data_source 
{
   struct wl_resource *resource;
   struct wl_array mime_types;
   struct wl_signal destroy_signal;

   void (*accept)(struct wl_data_source *source,
                  uint32_t serial, const char *mime_type);
   void (*send)(struct wl_data_source *source,
                const char *mime_type, int32_t fd);
   void (*cancel)(struct wl_data_source *source);
};

struct wl_seat 
{
   struct wl_list base_resource_list;
   struct wl_signal destroy_signal;

   struct wl_pointer *pointer;
   struct wl_keyboard *keyboard;
   struct wl_touch *touch;

   unsigned int selection_serial;
   struct wl_data_source *selection_data_source;
   struct wl_listener selection_data_source_listener;
   struct wl_signal selection_signal;

   struct wl_list drag_resource_list;
   struct wl_client *drag_client;
   struct wl_data_source *drag_data_source;
   struct wl_listener drag_data_source_listener;
   struct wl_resource *drag_focus;
   struct wl_resource *drag_focus_resource;
   struct wl_listener drag_focus_listener;
   struct wl_pointer_grab drag_grab;
   struct wl_resource *drag_surface;
   struct wl_listener drag_icon_listener;
   struct wl_signal drag_icon_signal;
};

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
        struct wl_resource *resource;
     } wl;

   pixman_region32_t region;
};

struct _E_Wayland_Surface_Frame_Callback
{
   struct  
    {
        struct wl_resource *resource;
        struct wl_list link;
     } wl;
};

struct _E_Wayland_Buffer
{
   EINA_INLIST;
   struct 
     {
        struct wl_resource *resource;
        struct wl_signal destroy_signal;
        struct wl_listener destroy_listener;
        union 
          {
             struct wl_shm_buffer *shm_buffer;
             void *legacy_buffer;
          };
     } wl;

   int w, h;
   unsigned int busy_count;
   E_Wayland_Surface *ews;
};

struct _E_Wayland_Buffer_Reference
{
   E_Wayland_Buffer *buffer;
   struct wl_listener destroy_listener;
};

struct _E_Wayland_Surface
{
   EINA_INLIST;
   Ecore_Window id;
   struct 
     {
        struct wl_client *client;
        struct wl_resource *surface;
        struct wl_signal destroy_signal;
        struct wl_list link, frames;
     } wl;

   struct 
     {
        E_Wayland_Buffer *buffer;
        struct wl_listener buffer_destroy;
        struct wl_list frames;
        Evas_Coord x, y;
        Eina_Bool new_buffer : 1;
        pixman_region32_t damage, opaque, input;
     } pending;

   E_Wayland_Buffer_Reference buffer_reference;

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

   E_Client *ec;
   E_Pixmap *pixmap;
   Eina_Inlist *buffers;

   E_Wayland_Shell_Surface *shell_surface;
   Eina_Bool mapped : 1;
   Eina_Bool updates : 1; //surface has render updates

   E_Wayland_Input *input;

   void (*map) (E_Wayland_Surface *ews, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
   void (*unmap) (E_Wayland_Surface *ews);
   void (*configure) (E_Wayland_Surface *ews, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
};

struct _E_Wayland_Shell_Surface
{
   struct 
     {
        struct wl_resource *resource;
        struct wl_listener surface_destroy;
        struct wl_signal destroy_signal;
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

   Eina_Stringshare *title, *clas;

   Eina_Bool active : 1;

   void *shell;
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

#ifdef WAYLAND_ONLY
   Eina_Bool focus : 1;

   unsigned int output_pool;

   struct xkb_rule_names xkb_names;
   struct xkb_context *xkb_context;
#endif

   Ecore_Event_Handler *kbd_handler;
   Ecore_Fd_Handler *fd_handler;
   Ecore_Idler *idler;

   E_Wayland_Input *input;

   Eina_Inlist *surfaces;
   Eina_List *seats;
   
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
extern EAPI E_Wayland_Compositor *_e_wl_comp;

EAPI Eina_Bool e_comp_wl_init(void);
EINTERN void e_comp_wl_shutdown(void);

#ifdef WAYLAND_ONLY
EAPI int e_comp_wl_input_read(int fd EINA_UNUSED, unsigned int mask EINA_UNUSED, void *data);
#endif

EAPI void wl_seat_init(struct wl_seat *seat);
EAPI void wl_seat_release(struct wl_seat *seat);

EAPI void wl_seat_set_pointer(struct wl_seat *seat, struct wl_pointer *pointer);
EAPI void wl_seat_set_keyboard(struct wl_seat *seat, struct wl_keyboard *keyboard);
EAPI void wl_seat_set_touch(struct wl_seat *seat, struct wl_touch *touch);

EAPI void wl_pointer_init(struct wl_pointer *pointer);
EAPI void wl_pointer_set_focus(struct wl_pointer *pointer, struct wl_resource *surface, wl_fixed_t sx, wl_fixed_t sy);
EAPI void wl_pointer_start_grab(struct wl_pointer *pointer, struct wl_pointer_grab *grab);
EAPI void wl_pointer_end_grab(struct wl_pointer *pointer);

EAPI void wl_keyboard_init(struct wl_keyboard *keyboard);
EAPI void wl_keyboard_set_focus(struct wl_keyboard *keyboard, struct wl_resource *surface);
EAPI void wl_keyboard_start_grab(struct wl_keyboard *device, struct wl_keyboard_grab *grab);
EAPI void wl_keyboard_end_grab(struct wl_keyboard *keyboard);

EAPI void wl_touch_init(struct wl_touch *touch);
EAPI void wl_touch_start_grab(struct wl_touch *device, struct wl_touch_grab *grab);
EAPI void wl_touch_end_grab(struct wl_touch *touch);

EAPI void wl_data_device_set_keyboard_focus(struct wl_seat *seat);
EAPI int wl_data_device_manager_init(struct wl_display *display);
EAPI struct wl_resource *wl_data_source_send_offer(struct wl_data_source *source, struct wl_resource *target);
EAPI void wl_seat_set_selection(struct wl_seat *seat, struct wl_data_source *source, uint32_t serial);

EAPI unsigned int e_comp_wl_time_get(void);
EAPI void e_comp_wl_input_modifiers_update(unsigned int serial);

EAPI void e_comp_wl_mouse_button(struct wl_resource *resource, uint32_t serial, uint32_t timestamp, uint32_t button, uint32_t state_w);
EAPI Eina_Bool e_comp_wl_cb_keymap_changed(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED);

static inline uint64_t
e_comp_wl_id_get(uint32_t client, uint32_t surface)
{
   return ((uint64_t)surface << 32) + (uint64_t)client;
}

# endif
#endif
