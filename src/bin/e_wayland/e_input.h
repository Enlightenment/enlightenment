#ifdef E_TYPEDEFS

typedef struct _E_Input E_Input;
typedef struct _E_Input_Pointer E_Input_Pointer;
typedef struct _E_Input_Keyboard E_Input_Keyboard;
typedef struct _E_Input_Keyboard_Info E_Input_Keyboard_Info;
typedef struct _E_Input_Pointer_Grab E_Input_Pointer_Grab;
typedef struct _E_Input_Pointer_Grab_Interface E_Input_Pointer_Grab_Interface;
typedef struct _E_Input_Keyboard_Grab E_Input_Keyboard_Grab;
typedef struct _E_Input_Keyboard_Grab_Interface E_Input_Keyboard_Grab_Interface;

#else
# ifndef E_INPUT_H
#  define E_INPUT_H

struct _E_Input_Keyboard_Info
{
   struct xkb_keymap *keymap;
   int fd;
   size_t size;
   char *area;
   struct 
     {
        xkb_mod_index_t shift;
        xkb_mod_index_t caps;
        xkb_mod_index_t ctrl;
        xkb_mod_index_t alt;
        xkb_mod_index_t super;
        /* TODO: leds */
     } mods;
};

struct _E_Input
{
   Ecore_Wl_Input base;

   E_Compositor *compositor;

   struct wl_list link;
   struct wl_list resources;
   struct wl_list drag_resources;

   E_Input_Pointer *pointer;
   E_Input_Keyboard *keyboard;
   unsigned int modifier_state;

   E_Input_Keyboard_Info kbd_info;

   struct 
     {
        E_Surface *saved_focus;
        struct wl_listener focus_listener;
        struct xkb_state *state;
     } kbd;

   struct 
     {
        struct wl_signal selection;
        struct wl_signal destroy;
     } signals;

   char *name;
};

struct _E_Input_Pointer_Grab_Interface
{
   void (*focus)(E_Input_Pointer_Grab *grab);
   void (*motion)(E_Input_Pointer_Grab *grab, unsigned int timestamp);
   void (*button)(E_Input_Pointer_Grab *grab, unsigned int timestamp, unsigned int button, unsigned int state);
};

struct _E_Input_Pointer_Grab
{
   E_Input_Pointer *pointer;
   E_Input_Pointer_Grab_Interface *interface;

   Evas_Coord x, y;
   unsigned int button, button_count;
   unsigned int serial, timestamp;
   Eina_Bool up : 1;

   struct wl_client *client;
   struct wl_list surfaces;
};

struct _E_Input_Pointer
{
   E_Input *seat;

   struct wl_list resources;

   E_Surface *focus;
   struct wl_resource *focus_resource;
   struct wl_listener focus_listener;
   unsigned int focus_serial;

   struct 
     {
        struct wl_signal focus;
     } signals;

   Evas_Coord x, y;

   E_Input_Pointer_Grab *grab;
   E_Input_Pointer_Grab default_grab;
};

struct _E_Input_Keyboard_Grab_Interface
{
   void (*key)(E_Input_Keyboard_Grab *grab, unsigned int timestamp, unsigned int key, unsigned int state);
   void (*modifiers)(E_Input_Keyboard_Grab *grab, unsigned int serial, unsigned int pressed, unsigned int latched, unsigned int locked, unsigned int group);
};

struct _E_Input_Keyboard_Grab
{
   E_Input_Keyboard *keyboard;
   E_Input_Keyboard_Grab_Interface *interface;
};

struct _E_Input_Keyboard
{
   E_Input *seat;

   struct wl_list resources;

   E_Surface *focus;
   struct wl_resource *focus_resource;
   struct wl_listener focus_listener;
   unsigned int focus_serial;

   struct 
     {
        struct wl_signal focus;
     } signals;

   E_Input_Keyboard_Grab *grab;
   E_Input_Keyboard_Grab default_grab;

   unsigned int grab_key;
   unsigned int grab_serial;
   unsigned int grab_time;

   struct wl_array keys;

   struct 
     {
        unsigned int pressed;
        unsigned int latched;
        unsigned int locked;
        unsigned int group;
     } modifiers;

   /* TODO: input method */
};

EAPI Eina_Bool e_input_init(E_Compositor *comp, E_Input *seat, const char *name);
EAPI Eina_Bool e_input_shutdown(E_Input *seat);
EAPI Eina_Bool e_input_pointer_init(E_Input *seat);
EAPI Eina_Bool e_input_keyboard_init(E_Input *seat, struct xkb_keymap *keymap);
EAPI Eina_Bool e_input_touch_init(E_Input *seat);

EAPI void e_input_pointer_focus_set(E_Input_Pointer *pointer, E_Surface *surface, Evas_Coord x, Evas_Coord y);
EAPI void e_input_pointer_grab_start(E_Input_Pointer *pointer);
EAPI void e_input_pointer_grab_end(E_Input_Pointer *pointer);

EAPI Eina_Bool e_input_keyboard_info_keymap_add(E_Input_Keyboard_Info *kbd_info);

EAPI void e_input_keyboard_focus_set(E_Input_Keyboard *keyboard, E_Surface *surface);
EAPI void e_input_keyboard_grab_start(E_Input_Keyboard *keyboard);
EAPI void e_input_keyboard_grab_end(E_Input_Keyboard *keyboard);
EAPI E_Input_Keyboard *e_input_keyboard_add(E_Input *seat);
EAPI void e_input_keyboard_del(E_Input_Keyboard *keyboard);

EAPI void e_input_repick(E_Input *seat);

EAPI void e_input_keyboard_focus_destroy(struct wl_listener *listener, void *data);

# endif
#endif
