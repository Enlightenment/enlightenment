/* Ask about compositing */
#include "e_wizard.h"
#include <Evas_GL.h>

static int do_gl = 0;
static int do_vsync = 0;
static int disable_effects = 0;


E_API int
wizard_page_show(E_Wizard_Page *pg)
{
   Evas_Object *o, *of, *ob;

   o = e_widget_list_add(pg->evas, 1, 0);
   e_wizard_title_set(_("Compositing"));


   of = e_widget_framelist_add(pg->evas, _("Settings"), 0);
   if (e_comp->gl)
     {
        Evas_GL *gl;

        gl = evas_gl_new(e_comp->evas);
        if (gl)
          {
             const char *str;
             Evas_GL_API *glapi = evas_gl_api_get(gl);
             str = (char*)glapi->glGetString(GL_RENDERER);
             if (str && (!strcasestr(str, "llvmpipe")))
               do_gl = do_vsync = 1;
             evas_gl_free(gl);
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
