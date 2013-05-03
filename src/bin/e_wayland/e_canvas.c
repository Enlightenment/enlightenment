#include "e.h"

/* local function prototypes */
static Eina_Bool _e_canvas_cb_flush(void *data);

/* local variables */
static Eina_List *_e_canvases = NULL;
static Ecore_Poller *_e_canvas_cache_flush_poller = NULL;

/* externally accessible functions */
EAPI void
e_canvas_add(Ecore_Evas *ee)
{
   Evas *e;

   _e_canvases = eina_list_prepend(_e_canvases, ee);
   e = ecore_evas_get(ee);
   evas_image_cache_set(e, e_config->image_cache * 1024);
   evas_font_cache_set(e, e_config->font_cache * 1024);
   e_path_evas_append(path_fonts, e);
   if (e_config->font_hinting == 0)
     {
        if (evas_font_hinting_can_hint(e, EVAS_FONT_HINTING_BYTECODE))
          evas_font_hinting_set(e, EVAS_FONT_HINTING_BYTECODE);
        else if (evas_font_hinting_can_hint(e, EVAS_FONT_HINTING_AUTO))
          evas_font_hinting_set(e, EVAS_FONT_HINTING_AUTO);
        else
          evas_font_hinting_set(e, EVAS_FONT_HINTING_NONE);
     }
   else if (e_config->font_hinting == 1)
     {
        if (evas_font_hinting_can_hint(e, EVAS_FONT_HINTING_AUTO))
          evas_font_hinting_set(e, EVAS_FONT_HINTING_AUTO);
        else
          evas_font_hinting_set(e, EVAS_FONT_HINTING_NONE);
     }
   else if (e_config->font_hinting == 2)
     evas_font_hinting_set(e, EVAS_FONT_HINTING_NONE);
}

EAPI void
e_canvas_del(Ecore_Evas *ee)
{
   _e_canvases = eina_list_remove(_e_canvases, ee);
}

EAPI void
e_canvas_recache(void)
{
   Eina_List *l;
   Ecore_Evas *ee;

   EINA_LIST_FOREACH(_e_canvases, l, ee)
     {
        Evas *e;

        e = ecore_evas_get(ee);
        evas_image_cache_set(e, e_config->image_cache * 1024);
        evas_font_cache_set(e, e_config->font_cache * 1024);
     }
   edje_file_cache_set(e_config->edje_cache);
   edje_collection_cache_set(e_config->edje_collection_cache);
   if (_e_canvas_cache_flush_poller)
     {
        ecore_poller_del(_e_canvas_cache_flush_poller);
        _e_canvas_cache_flush_poller = NULL;
     }
   if (e_config->cache_flush_poll_interval > 0)
     {
        _e_canvas_cache_flush_poller =
          ecore_poller_add(ECORE_POLLER_CORE,
                           e_config->cache_flush_poll_interval,
                           _e_canvas_cb_flush, NULL);
     }
}

EAPI void
e_canvas_cache_flush(void)
{
   Eina_List *l;
   Ecore_Evas *ee;

   EINA_LIST_FOREACH(_e_canvases, l, ee)
     {
        Evas *e;

        e = ecore_evas_get(ee);
        evas_image_cache_flush(e);
        evas_font_cache_flush(e);
     }
   edje_file_cache_flush();
   edje_collection_cache_flush();
}

EAPI void
e_canvas_cache_reload(void)
{
   Eina_List *l;
   Ecore_Evas *ee;

   EINA_LIST_FOREACH(_e_canvases, l, ee)
     {
        Evas *e;

        e = ecore_evas_get(ee);
        evas_image_cache_reload(e);
     }
}

EAPI void
e_canvas_idle_flush(void)
{
   Eina_List *l;
   Ecore_Evas *ee;

   EINA_LIST_FOREACH(_e_canvases, l, ee)
     {
        Evas *e;

        e = ecore_evas_get(ee);
        evas_render_idle_flush(e);
     }
}

EAPI void
e_canvas_rehint(void)
{
   Eina_List *l;
   Ecore_Evas *ee;

   EINA_LIST_FOREACH(_e_canvases, l, ee)
     {
        Evas *e;

        e = ecore_evas_get(ee);
        if (e_config->font_hinting == 0)
          evas_font_hinting_set(e, EVAS_FONT_HINTING_BYTECODE);
        else if (e_config->font_hinting == 1)
          evas_font_hinting_set(e, EVAS_FONT_HINTING_AUTO);
        else if (e_config->font_hinting == 2)
          evas_font_hinting_set(e, EVAS_FONT_HINTING_NONE);
     }
}

EAPI Ecore_Evas *
e_canvas_new(unsigned int parent, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h, Eina_Bool override, Eina_Bool frame, Ecore_Wl_Window **win_ret)
{
   Ecore_Evas *ee;

/* #ifdef HAVE_WAYLAND_EGL */
/*    ee = ecore_evas_wayland_egl_new(NULL, parent, x, y, w, h, frame); */
/*    if (!ee) */
/* #endif */
     ee = ecore_evas_wayland_shm_new(NULL, parent, x, y, w, h, frame);

   if (!ee)
     {
        ERR("Impossible to build any Ecore_Evas window !!");
        return NULL;
     }

   ecore_evas_override_set(ee, override);
   if (win_ret) *win_ret = ecore_evas_wayland_window_get(ee);

   return ee;
}

EAPI const Eina_List *
e_canvas_list(void)
{
   return _e_canvases;
}

/* local functions */
static Eina_Bool
_e_canvas_cb_flush(void *data EINA_UNUSED)
{
   e_canvas_cache_flush();
   return ECORE_CALLBACK_RENEW;
}

