#include "e.h"

EAPI int E_EVENT_INIT_DONE = 0;

/* local variables */
static int done = 0;
static int undone = 0;
static Evas_Object *_e_init_object = NULL;
static Eina_List *splash_objs = NULL;
static Ecore_Timer *_e_init_timeout_timer = NULL;

static Eina_Bool
_e_init_cb_timeout(void *data __UNUSED__)
{
   _e_init_timeout_timer = NULL;
   e_init_hide();
   return ECORE_CALLBACK_CANCEL;
}

static void
_e_init_cb_signal_done_ok(void *data __UNUSED__, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__)
{
   e_init_hide();
}

/* public functions */
EINTERN int
e_init_init(void)
{
   E_EVENT_INIT_DONE = ecore_event_type_new();

   done = 0;
   return 1;
}

EINTERN int
e_init_shutdown(void)
{
   /* if not killed, kill init */
   e_init_hide();
   return 1;
}

EAPI void
e_init_show(void)
{
   Evas_Object *o;
   E_Zone *zone;
   Eina_List *l;
   /* exec init */

   EINA_LIST_FOREACH(e_comp->zones, l, zone)
     {
        o = edje_object_add(e_comp->evas);
        if (!zone->num)
          {
             e_theme_edje_object_set(o, NULL, "e/init/splash");
             evas_object_name_set(o, "_e_init_object");
             _e_init_object = o;
          }
        else
          {
             e_theme_edje_object_set(o, NULL, "e/init/extra_screen");
             evas_object_name_set(o, "_e_init_extra_screen");
          }
        evas_object_clip_set(o, zone->bg_clip_object);
        evas_object_move(o, zone->x, zone->y);
        evas_object_resize(o, zone->w, zone->h);
        evas_object_layer_set(o, E_LAYER_MAX);
        evas_object_show(o);
        splash_objs = eina_list_append(splash_objs, o);
     }
   edje_object_part_text_set(_e_init_object, "e.text.disable_text",
                             "Disable splash screen");
   edje_object_signal_callback_add(_e_init_object, "e,state,done_ok", "e",
                                   _e_init_cb_signal_done_ok, NULL);
   _e_init_timeout_timer = ecore_timer_add(240.0, _e_init_cb_timeout, NULL);
   e_init_title_set(_("Enlightenment"));
   e_init_version_set(VERSION);
}

EAPI void
e_init_hide(void)
{
   E_FREE_LIST(splash_objs, evas_object_del);
   e_comp_shape_queue();
   _e_init_object = NULL;
   E_FREE_FUNC(_e_init_timeout_timer, ecore_timer_del);
}

EAPI void
e_init_title_set(const char *str)
{
   if (!_e_init_object) return;
   edje_object_part_text_set(_e_init_object, "e.text.title", str);
}

EAPI void
e_init_version_set(const char *str)
{
   if (!_e_init_object) return;
   edje_object_part_text_set(_e_init_object, "e.text.version", str);
}

EAPI void
e_init_status_set(const char *str)
{
   if (!_e_init_object) return;
   edje_object_part_text_set(_e_init_object, "e.text.status", str);
}

EAPI void
e_init_done(void)
{
   undone--;
   if (undone > 0) return;
   done = 1;
   ecore_event_add(E_EVENT_INIT_DONE, NULL, NULL, NULL);
//   printf("---DONE %p\n", client);
   edje_object_signal_emit(_e_init_object, "e,state,done", "e");
}

EAPI void
e_init_undone(void)
{
   undone++;
}

EAPI int
e_init_count_get(void)
{
   return undone;
}
