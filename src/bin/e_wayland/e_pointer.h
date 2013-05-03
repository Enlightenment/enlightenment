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

   Eina_Bool e_cursor : 1;
   Eina_Bool color : 1;
   Eina_Bool idle : 1;

   Evas *evas;
   Ecore_Evas *ee;
   Evas_Object *o_ptr, *o_hot;
   void *obj;

   Evas_Coord x, y, w, h;
   const char *type;
   Eina_List *stack;

   Ecore_Wl_Window *win;

   Ecore_Timer *idle_timer;
   Ecore_Poller *idle_poller;

   struct 
     {
        Evas_Coord x, y;
        Eina_Bool update : 1;
     } hot;
};

EINTERN int e_pointer_init(void);
EINTERN int e_pointer_shutdown(void);

EAPI E_Pointer *e_pointer_new(Ecore_Wl_Window *win, Eina_Bool filled);
EAPI void e_pointer_hide(E_Pointer *p);
EAPI void e_pointer_type_push(E_Pointer *p, void *obj, const char *type);
EAPI void e_pointer_type_pop(E_Pointer *p, void *obj, const char *type);
EAPI void e_pointer_size_set(int size);
EAPI void e_pointer_idler_before(void);

# endif
#endif
