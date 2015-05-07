/* Ask about compositing */
#include "e_wizard.h"

static int do_gl = 0;
static int do_vsync = 0;
static int disable_effects = 0;
static Eina_Bool gl_avail = EINA_FALSE;

static int
match_file_glob(FILE *f, const char *globbing)
{
   char buf[32768];
   int found = 0;

   while (fgets(buf, sizeof(buf), f))
     {
        if (e_util_glob_match(buf, globbing))
          {
             found = 1;
             break;
          }
     }
   fclose(f);
   return found;
}

static int
match_xorg_log(const char *globbing)
{
   FILE *f;
   int i;
   char buf[PATH_MAX];

   for (i = 0; i < 5; i++)
     {
        snprintf(buf, sizeof(buf), "/var/log/Xorg.%i.log", i);
        f = fopen(buf, "rb");
        if (f)
          {
             if (match_file_glob(f, globbing)) return 1;
          }
     }
   return 0;
}

E_API int
wizard_page_show(E_Wizard_Page *pg)
{
   Evas_Object *o, *of, *ob;
   Ecore_Evas *ee;

#ifndef HAVE_WAYLAND_ONLY
   Ecore_X_Window_Attributes att;

   if (!ecore_x_composite_query()) return 0;
   if (!ecore_x_damage_query()) return 0;

   memset((&att), 0, sizeof(Ecore_X_Window_Attributes));
   ecore_x_window_attributes_get(ecore_x_window_root_first_get(), &att);
   if ((att.depth <= 8)) return 0;

   gl_avail = ecore_evas_engine_type_supported_get(ECORE_EVAS_ENGINE_OPENGL_X11);
#endif

   o = e_widget_list_add(pg->evas, 1, 0);
   e_wizard_title_set(_("Compositing"));


   of = e_widget_framelist_add(pg->evas, _("Settings"), 0);
   if (gl_avail)
     {
        ee = ecore_evas_gl_x11_new(NULL, 0, 0, 0, 320, 240);
        if (ee)
          {
             ecore_evas_free(ee);
             if (
               (match_xorg_log("*(II)*NVIDIA*: Creating default Display*")) ||
               (match_xorg_log("*(II)*intel*: Creating default Display*")) ||
               (match_xorg_log("*(II)*NOUVEAU*: Creating default Display*")) ||
               (match_xorg_log("*(II)*RADEON*: Creating default Display*"))
               )
               {
                  do_gl = 1;
                  do_vsync = 1;
               }
          }
        ob = e_widget_check_add(pg->evas, _("Hardware Accelerated (OpenGL)"), &(do_gl));
        e_widget_framelist_object_append(of, ob);

        ob = e_widget_check_add(pg->evas, _("Tear-free Rendering (OpenGL only)"), &(do_vsync));
        e_widget_framelist_object_append(of, ob);
     }
   ob = e_widget_check_add(pg->evas, _("Disable composite effects"), &(disable_effects));
   e_widget_framelist_object_append(of, ob);

   e_widget_list_object_append(o, of, 0, 0, 0.5);
   evas_object_show(of);
   e_wizard_page_show(o);

   return 1; /* 1 == show ui, and wait for user, 0 == just continue */
}

E_API int
wizard_page_hide(E_Wizard_Page *pg EINA_UNUSED)
{
   E_Comp_Config *conf = NULL;

   conf = e_comp_config_get();
   if (do_gl)
     {
        conf->engine = E_COMP_ENGINE_GL;
        conf->smooth_windows = 1;
        conf->vsync = do_vsync;
     }
   else
     {
        conf->engine = E_COMP_ENGINE_SW;
        conf->smooth_windows = 0;
        conf->vsync = 0;
     }
   if (disable_effects)
     {
        conf->disable_screen_effects =
        conf->match.disable_borders =
        conf->match.disable_popups =
        conf->match.disable_menus =
        conf->match.disable_objects =
        conf->match.disable_overrides = 1;
     }

   e_comp_internal_save();

   return 1;
}
