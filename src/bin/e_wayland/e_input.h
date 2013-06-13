#ifdef E_TYPEDEFS

typedef struct _E_Input E_Input;
typedef struct _E_Input_Pointer E_Input_Pointer;
typedef struct _E_Input_Keyboard E_Input_Keyboard;

#else
# ifndef E_INPUT_H
#  define E_INPUT_H

struct _E_Input
{
   Ecore_Wl_Input base;

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

struct _E_Input_Pointer
{
   E_Input *seat;

   struct wl_list resources;

   struct 
     {
        struct wl_signal focus;
     } signals;
};

struct _E_Input_Keyboard
{

};

EAPI Eina_Bool e_input_init(E_Compositor *comp, E_Input *seat, const char *name);
EAPI Eina_Bool e_input_shutdown(E_Input *seat);
EAPI Eina_Bool e_input_pointer_init(E_Input *seat);
EAPI Eina_Bool e_input_keyboard_init(E_Input *seat);
EAPI Eina_Bool e_input_touch_init(E_Input *seat);

# endif
#endif
