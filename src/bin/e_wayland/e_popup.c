#include "e.h"

/* local function prototypes */
static void _e_popup_cb_free(E_Popup *pop);

/* local variables */
static Eina_Hash *_popups = NULL;

EINTERN int 
e_popup_init(void)
{
   if (!_popups) _popups = eina_hash_string_superfast_new(NULL);

   return 1;
}

EINTERN int 
e_popup_shutdown(void)
{
   if (_popups) eina_hash_free(_popups);
   _popups = NULL;

   return 1;
}

EAPI E_Popup *
e_popup_new(E_Zone *zone, int x, int y, int w, int h)
{
   E_Popup *p;
   Ecore_Wl_Window *win = NULL;
   unsigned int parent = 0;

   p = E_OBJECT_ALLOC(E_Popup, E_POPUP_TYPE, _e_popup_cb_free);
   if (!p) return NULL;

   p->zone = zone;
   p->x = x;
   p->y = y;
   p->w = w;
   p->h = h;

   if (p->zone->container)
     {
        if ((win = ecore_evas_wayland_window_get(zone->container->bg_ee)))
          parent = win->id;
     }

   p->ee = 
     e_canvas_new(parent, zone->x + p->x, zone->y + p->y, 
                  p->w, p->h, EINA_TRUE, EINA_FALSE, &win);
   if (!p->ee)
     {
        free(p);
        return NULL;
     }

   e_canvas_add(p->ee);
   p->evas = ecore_evas_get(p->ee);

   /* TODO: window raise ? */

   ecore_evas_name_class_set(p->ee, "E", "_e_popup_window");
   ecore_evas_title_set(p->ee, "E Popup");

   e_object_ref(E_OBJECT(p->zone));
   zone->popups = eina_list_append(zone->popups, p);

   /* win = ecore_evas_wayland_window_get(p->ee); */
   eina_hash_add(_popups, e_util_winid_str_get(win), p);

   return p;
}

EAPI void 
e_popup_show(E_Popup *pop)
{
   if (pop->visible) return;
   pop->visible = EINA_TRUE;
   ecore_evas_show(pop->ee);
}

EAPI void 
e_popup_hide(E_Popup *pop)
{
   if (!pop->visible) return;
   pop->visible = EINA_FALSE;
   ecore_evas_hide(pop->ee);
}

EAPI void 
e_popup_move(E_Popup *pop, int x, int y)
{
   if ((pop->x == x) && (pop->y == y)) return;
   pop->x = x;
   pop->y = y;
   ecore_evas_move(pop->ee, pop->zone->x + x, pop->zone->y + y);
}

EAPI void 
e_popup_resize(E_Popup *pop, int w, int h)
{
   if ((pop->w == w) && (pop->h == h)) return;
   pop->w = w;
   pop->h = h;
   ecore_evas_resize(pop->ee, w, h);
}

EAPI void 
e_popup_move_resize(E_Popup *pop, int x, int y, int w, int h)
{
   if ((pop->x == x) && (pop->y == y) && (pop->w == w) && (pop->h == h))
     return;
   pop->x = x;
   pop->y = y;
   pop->w = w;
   pop->h = h;
   ecore_evas_move_resize(pop->ee, pop->zone->x + x, pop->zone->y + y, w, h);
}

EAPI void 
e_popup_layer_set(E_Popup *pop, int layer)
{
   pop->layer = layer;
}

EAPI void 
e_popup_name_set(E_Popup *pop, const char *name)
{
   if (eina_stringshare_replace(&pop->name, name))
     ecore_evas_name_class_set(pop->ee, "E", pop->name);
}

EAPI void 
e_popup_ignore_events_set(E_Popup *pop, Eina_Bool ignore)
{
   ecore_evas_ignore_events_set(pop->ee, ignore);
}

EAPI void 
e_popup_edje_bg_object_set(E_Popup *pop, Evas_Object *obj)
{
   const char *shaped;

   if ((shaped = edje_object_data_get(obj, "shaped")))
     {
        if (!strcmp(shaped, "1"))
          pop->shaped = EINA_TRUE;
        else
          pop->shaped = EINA_FALSE;
        ecore_evas_alpha_set(pop->ee, pop->shaped);
     }
   else
     ecore_evas_shaped_set(pop->ee, pop->shaped);
}

EAPI void 
e_popup_idler_before(void)
{

}

EAPI E_Popup *
e_popup_find_by_window(Ecore_Wl_Window *win)
{
   E_Popup *pop;

   if ((pop = eina_hash_find(_popups, e_util_winid_str_get(win))))
     {
        Ecore_Wl_Window *w;

        if ((w = ecore_evas_wayland_window_get(pop->ee)))
          if (w != win) 
            return NULL;
     }

   return pop;
}

/* local functions */
static void 
_e_popup_cb_free(E_Popup *pop)
{
   Ecore_Wl_Window *win;

   win = ecore_evas_wayland_window_get(pop->ee);

   e_canvas_del(pop->ee);
   ecore_evas_free(pop->ee);
   e_object_unref(E_OBJECT(pop->zone));
   pop->zone->popups = eina_list_remove(pop->zone->popups, pop);
   eina_hash_del(_popups, e_util_winid_str_get(win), pop);
   if (pop->name) eina_stringshare_del(pop->name);
   pop->name = NULL;
   free(pop);
}
