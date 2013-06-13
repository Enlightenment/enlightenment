#ifdef E_TYPEDEFS

typedef struct _E_Input E_Input;
typedef struct _E_Input_Pointer E_Input_Pointer;
typedef struct _E_Input_Keyboard E_Input_Keyboard;
typedef struct _E_Input_Pointer_Grab E_Input_Pointer_Grab;
typedef struct _E_Input_Pointer_Grab_Interface E_Input_Pointer_Grab_Interface;

#else
# ifndef E_INPUT_H
#  define E_INPUT_H

struct _E_Input
{
   Ecore_Wl_Input base;

   E_Compositor *compositor;

   struct wl_list link;
   struct wl_list resources;
   struct wl_list drag_resources;

   E_Input_Pointer *pointer;

   struct 
     {
        struct wl_signal selection;
        struct wl_signal destroy;
     } signals;

   char *name;
};

struct _E_Input_Keyboard
{

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
};

struct _E_Input_Pointer
{
   E_Input *seat;

   struct wl_list resources;

   E_Surface *focus;
   struct wl_resource *focus_resource;

   struct 
     {
        struct wl_signal focus;
     } signals;

   Evas_Coord x, y;

   E_Input_Pointer_Grab *grab;
   E_Input_Pointer_Grab default_grab;
};

EAPI Eina_Bool e_input_init(E_Compositor *comp, E_Input *seat, const char *name);
EAPI Eina_Bool e_input_shutdown(E_Input *seat);
EAPI Eina_Bool e_input_pointer_init(E_Input *seat);
EAPI Eina_Bool e_input_keyboard_init(E_Input *seat);
EAPI Eina_Bool e_input_touch_init(E_Input *seat);

EAPI void e_input_pointer_focus_set(E_Input_Pointer *pointer, E_Surface *surface, Evas_Coord x, Evas_Coord y);

# endif
#endif
