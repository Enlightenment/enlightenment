#ifdef E_TYPEDEFS

typedef struct _E_Pointer E_Pointer;

typedef enum 
{
   /* These are compatible with netwm */
   E_POINTER_RESIZE_TL = 0,
   E_POINTER_RESIZE_T = 1,
   E_POINTER_RESIZE_TR = 2,
   E_POINTER_RESIZE_R = 3,
   E_POINTER_RESIZE_BR = 4,
   E_POINTER_RESIZE_B = 5,
   E_POINTER_RESIZE_BL = 6,
   E_POINTER_RESIZE_L = 7,
   E_POINTER_MOVE = 8,
   E_POINTER_RESIZE_NONE = 11
} E_Pointer_Mode;

#else
# ifndef E_POINTER_H
#  define E_POINTER_H

#  define E_POINTER_TYPE 0xE0b01013

struct _E_Pointer
{
   E_Object e_obj_inherit;

   Evas *evas;
   Ecore_Evas *ee;
   Evas_Object *o_ptr;
   Evas_Object *o_hot;

//   E_Pixmap *pixmap;
   Ecore_Window win;

   int *pixels;
   int x, y, w, h;
   const char *type;

   struct 
     {
        int x, y;
        Eina_Bool update : 1;
     } hot;

   Ecore_Timer *idle_tmr;
   Ecore_Poller *idle_poll;

   Eina_List *stack;

   Eina_Bool e_cursor : 1;
   Eina_Bool color : 1;
   Eina_Bool idle : 1;
   Eina_Bool canvas : 1;
};

EINTERN int e_pointer_init(void);
EINTERN int e_pointer_shutdown(void);

EAPI E_Pointer *e_pointer_window_new(Ecore_Window win, Eina_Bool filled);
EAPI E_Pointer *e_pointer_canvas_new(Ecore_Evas *ee, Eina_Bool filled);

EAPI void e_pointers_size_set(int size);
EAPI void e_pointer_hide(E_Pointer *ptr);
EAPI void e_pointer_type_push(E_Pointer *ptr, void *obj, const char *type);
EAPI void e_pointer_type_pop(E_Pointer *ptr, void *obj, const char *type);
EAPI void e_pointer_mode_push(void *obj, E_Pointer_Mode mode);
EAPI void e_pointer_mode_pop(void *obj, E_Pointer_Mode mode);
EAPI void e_pointer_idler_before(void);
EAPI void e_pointer_object_set(E_Pointer *ptr, Evas_Object *obj, int x, int y);
# endif
#endif
