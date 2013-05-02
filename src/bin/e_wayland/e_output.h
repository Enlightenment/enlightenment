#ifdef E_TYPEDEFS

typedef struct _E_Output_Mode E_Output_Mode;
typedef struct _E_Output E_Output;

#else
# ifndef E_OUTPUT_H
#  define E_OUTPUT_H

struct _E_Output_Mode
{
   Evas_Coord w, h;
   unsigned int refresh;
   unsigned int flags;
};

struct _E_Output
{
   unsigned int id;

   E_Compositor *compositor;

   Evas_Coord x, y, w, h;
   Evas_Coord mm_w, mm_h;
   char *make, *model;
   unsigned int subpixel;
   unsigned int transform;

   Eina_List *modes;
   E_Output_Mode *current, *original;

   Eina_Bool dirty : 1;

   struct
     {
        Eina_Bool needed : 1;
        Eina_Bool scheduled : 1;
     } repaint;

   struct 
     {
        struct wl_list link;
        struct wl_list resources;
        struct wl_global *global;
     } wl;

   /* TODO: add backlight and dpms support */

   void (*cb_destroy)(E_Output *output);
};

EAPI void e_output_init(E_Output *output, E_Compositor *comp, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h, unsigned int transform);
EAPI void e_output_shutdown(E_Output *output);

# endif
#endif
