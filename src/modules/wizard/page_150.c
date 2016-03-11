/* Ask about compositing */
#include "e_wizard.h"
#include <Evas_GL.h>

static Eina_Bool do_gl = 0;
static Eina_Bool do_vsync = 0;
static Eina_Bool disable_effects = 0;


static void
check_add(Evas_Object *box, const char *txt, Eina_Bool *val)
{
   Evas_Object *ck;

   ck = elm_check_add(box);
   evas_object_show(ck);
   E_ALIGN(ck, 0, 0.5);
   elm_object_text_set(ck, txt);
   elm_check_state_pointer_set(ck, val);
   elm_box_pack_end(box, ck);
}

E_API int
wizard_page_show(E_Wizard_Page *pg EINA_UNUSED)
{
   Evas_Object *o, *of;

   e_wizard_title_set(_("Compositing"));

   of = elm_frame_add(e_comp->elm);
   elm_object_text_set(of, _("Settings"));

   o = elm_box_add(of);
   elm_box_homogeneous_set(o, 1);
   elm_object_content_set(of, o);
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
        check_add(o, _("Hardware Accelerated (OpenGL)"), &do_gl);
        check_add(o, _("Tear-free Rendering (OpenGL only)"), &do_vsync);
     }
   check_add(o, _("Disable composite effects"), &disable_effects);

   evas_object_show(of);
   e_wizard_page_show(of);

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
