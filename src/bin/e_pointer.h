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
#ifndef E_POINTER_H
#define E_POINTER_H

#define E_POINTER_TYPE 0xE0b01013

struct _E_Pointer
{
   E_Object e_obj_inherit;

   unsigned char     e_cursor : 1;
   unsigned char     color : 1;
   unsigned char     idle : 1;
   unsigned char     canvas : 1;

   Evas             *evas;
   E_Pixmap         *pixmap;
   Evas_Object      *pointer_object;
   Evas_Object      *pointer_image;
   Evas_Object      *hot_object;
   int              *pixels;
   Ecore_Window      win;
   int               w, h;
   Ecore_Timer      *idle_timer;
   Ecore_Poller     *idle_poller;
   int               x, y;
   unsigned int     blocks;

   const char       *type;
   void             *obj;
   Eina_List        *stack;

   struct {
      int            x, y;
      unsigned char  update : 1;
   } hot;
};

EINTERN int        e_pointer_init(void);
EINTERN int        e_pointer_shutdown(void);
EAPI E_Pointer *e_pointer_window_new(Ecore_Window win, int filled);
EAPI void	e_pointer_hide(E_Pointer *p);
EAPI E_Pointer *e_pointer_canvas_new(Evas *e, int filled);
EAPI void       e_pointer_image_set(E_Pointer *p, E_Pixmap *cp, int w, int h, int hot_x, int hot_y);
EAPI void       e_pointer_type_push(E_Pointer *p, void *obj, const char *type);
EAPI void       e_pointer_type_pop(E_Pointer *p, void *obj, const char *type);
EAPI void       e_pointers_size_set(int size);
EAPI void       e_pointer_idler_before(void);

EAPI void e_pointer_mode_push(void *obj, E_Pointer_Mode mode);
EAPI void e_pointer_mode_pop(void *obj, E_Pointer_Mode mode);

#endif
#endif
