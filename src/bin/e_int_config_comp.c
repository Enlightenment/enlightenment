#include "e.h"

struct _E_Config_Dialog_Data
{
   int         engine;
   int         indirect;
   int         texture_from_pixmap;
   int         smooth_windows;
   int         lock_fps;
   int         grab;
   int         vsync;
   int         swap_mode;

   const char *shadow_style;

   struct
   {
      int disable_popups;
      int disable_borders;
      int disable_overrides;
      int disable_menus;
      int disable_objects;
      int disable_all;
      int toggle_changed : 1;
   } match;

   Evas_Object *styles_il;

   int          keep_unmapped;
   int          max_unmapped_pixels;
   int          max_unmapped_time;
   int          min_unmapped_time;
   int          send_flush;
   int          send_dump;
   int          nocomp_fs;
   int          nofade;

   int          fps_show;
   int          fps_corner;
   int          fps_average_range;
   double       first_draw_delay;
   int disable_screen_effects;
   int enable_advanced_features;
   // the following options add the "/fast" suffix to the normal groups
   int fast_popups;
   int fast_borders;
   int fast_menus;
   int fast_overrides;
   int fast_objects;
   int fast;
   Evas_Object *fast_ob;
   int fast_changed : 1;
};

/* Protos */
static void        *_create_data(E_Config_Dialog *cfd);
static void         _free_data(E_Config_Dialog *cfd,
                               E_Config_Dialog_Data *cfdata);
static Evas_Object *_basic_create_widgets(E_Config_Dialog *cfd,
                                          Evas *evas,
                                          E_Config_Dialog_Data *cfdata);
static int          _basic_apply_data(E_Config_Dialog *cfd,
                                      E_Config_Dialog_Data *cfdata);
static Evas_Object *_advanced_create_widgets(E_Config_Dialog *cfd,
                                          Evas *evas,
                                          E_Config_Dialog_Data *cfdata);
static int          _advanced_apply_data(E_Config_Dialog *cfd,
                                      E_Config_Dialog_Data *cfdata);

E_API E_Config_Dialog *
e_int_config_comp(Evas_Object *parent EINA_UNUSED, const char *params EINA_UNUSED)
{
   E_Config_Dialog *cfd;
   E_Config_Dialog_View *v;

   if (e_config_dialog_find("E", "appearance/comp")) return NULL;
   v = E_NEW(E_Config_Dialog_View, 1);

   v->create_cfdata = _create_data;
   v->free_cfdata = _free_data;
   v->basic.apply_cfdata = _basic_apply_data;
   v->basic.create_widgets = _basic_create_widgets;
   v->advanced.apply_cfdata = _advanced_apply_data;
   v->advanced.create_widgets = _advanced_create_widgets;
   
   cfd = e_config_dialog_new(NULL, _("Composite Settings"),
                             "E", "appearance/comp", "preferences-composite", 0, v, NULL);
   e_dialog_resizable_set(cfd->dia, 1);
   return cfd;
}

static void *
_create_data(E_Config_Dialog *cfd EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata;
   E_Comp_Config *conf = e_comp_config_get();

   cfdata = E_NEW(E_Config_Dialog_Data, 1);

   cfdata->engine = conf->engine;
   if ((cfdata->engine != E_COMP_ENGINE_SW) &&
       (cfdata->engine != E_COMP_ENGINE_GL))
     cfdata->engine = E_COMP_ENGINE_SW;

   cfdata->fast_popups = conf->fast_popups;
   cfdata->fast_borders = conf->fast_borders;
   cfdata->fast_overrides = conf->fast_overrides;
   cfdata->fast_menus = conf->fast_menus;
   cfdata->fast_objects = conf->fast_objects;
   cfdata->match.disable_popups = conf->match.disable_popups;
   cfdata->match.disable_borders = conf->match.disable_borders;
   cfdata->match.disable_overrides = conf->match.disable_overrides;
   cfdata->match.disable_menus = conf->match.disable_menus;
   cfdata->match.disable_objects = conf->match.disable_objects;
   cfdata->disable_screen_effects = conf->disable_screen_effects;
   cfdata->enable_advanced_features = conf->enable_advanced_features;

   cfdata->indirect = conf->indirect;
   cfdata->texture_from_pixmap = conf->texture_from_pixmap;
   cfdata->smooth_windows = conf->smooth_windows;
   cfdata->lock_fps = conf->lock_fps;
   cfdata->grab = conf->grab;
   cfdata->vsync = conf->vsync;
   cfdata->swap_mode = conf->swap_mode;
   cfdata->shadow_style = eina_stringshare_add(conf->shadow_style);

   cfdata->keep_unmapped = conf->keep_unmapped;
   cfdata->max_unmapped_pixels = conf->max_unmapped_pixels;
   cfdata->max_unmapped_time = conf->max_unmapped_time;
   cfdata->min_unmapped_time = conf->min_unmapped_time;
   cfdata->send_flush = conf->send_flush;
   cfdata->send_dump = conf->send_dump;
   cfdata->nocomp_fs = conf->nocomp_fs;
   cfdata->nofade = conf->nofade;

   cfdata->fps_show = conf->fps_show;
   cfdata->fps_corner = conf->fps_corner;
   cfdata->fps_average_range = conf->fps_average_range;

   if (cfdata->fps_average_range < 1) cfdata->fps_average_range = 12;
   else if (cfdata->fps_average_range > 120)
     cfdata->fps_average_range = 120;
   cfdata->first_draw_delay = conf->first_draw_delay;

   return cfdata;
}

static void
_advanced_features_changed(E_Comp_Config *conf)
{
   conf->enable_advanced_features = !conf->enable_advanced_features;
   if (conf->enable_advanced_features)
     e_util_dialog_internal(_("WARNING"),
                            _("This option WILL break your desktop if you don't know what you're doing.<br>"
                              "Do not file bugs about anything that occurs with this option enabled.<br>"
                              "You have been warned."));
}

static void
_free_data(E_Config_Dialog *cfd  EINA_UNUSED,
           E_Config_Dialog_Data *cfdata)
{
   eina_stringshare_del(cfdata->shadow_style);
   free(cfdata);
}

static void
_advanced_comp_style_toggle(void *oi, Evas_Object *o)
{
   e_widget_disabled_set(oi, e_widget_check_checked_get(o));
}

static void
_advanced_matches_edit(void *data, void *d EINA_UNUSED)
{
   E_Config_Dialog *cfd = data;

   e_int_config_comp_match(cfd->dia->win, NULL);
}

static Evas_Object *
_advanced_create_widgets(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata)
{
   Evas_Object *ob,*ol, *of, *otb, *oi, *orec0;
   E_Radio_Group *rg;

   elm_win_center(cfd->dia->win, 1, 1);
   orec0 = evas_object_rectangle_add(evas);
   evas_object_name_set(orec0, "style_shadows");

   otb = e_widget_toolbook_add(evas, 48 * e_scale, 48 * e_scale);

   ///////////////////////////////////////////
   ol = e_widget_list_add(evas, 0, 0);

   ob = e_widget_button_add(evas, _("Edit window matches"), NULL, _advanced_matches_edit, cfd, NULL);
   e_widget_list_object_append(ol, ob, 0, 0, 0.5);

   of = e_widget_frametable_add(evas, _("Select default style"), 0);
   e_widget_frametable_content_align_set(of, 0.5, 0.5);
   cfdata->styles_il = oi = e_comp_style_selector_create(evas, &(cfdata->shadow_style));
   e_widget_frametable_object_append(of, oi, 0, 0, 1, 1, 1, 1, 1, 1);
   e_widget_list_object_append(ol, of, 1, 1, 0.5);

   e_widget_toolbook_page_append(otb, NULL, _("Styles"), ol, 1, 1, 1, 1, 0.5, 0.0);

   //////////////////////////////////////////////

   ol = e_widget_list_add(evas, 0, 0);
   {
      Evas_Object *w, *m, *p, *o1, *o2;

      of = e_widget_framelist_add(evas, _("Fast Effects"), 0);
      w = ob = e_widget_check_add(evas, _("Enable fast composite effects for windows"), &(cfdata->fast_borders));
      e_widget_disabled_set(ob, cfdata->match.disable_borders);
      e_widget_framelist_object_append(of, ob);
      m = ob = e_widget_check_add(evas, _("Enable fast composite effects for menus"), &(cfdata->fast_menus));
      e_widget_disabled_set(ob, cfdata->match.disable_menus);
      e_widget_framelist_object_append(of, ob);
      p = ob = e_widget_check_add(evas, _("Enable fast composite effects for popups"), &(cfdata->fast_popups));
      e_widget_disabled_set(ob, cfdata->match.disable_popups);
      e_widget_framelist_object_append(of, ob);
      o1 = ob = e_widget_check_add(evas, _("Enable fast composite effects for objects"), &(cfdata->fast_objects));
      e_widget_disabled_set(ob, cfdata->match.disable_objects);
      e_widget_framelist_object_append(of, ob);
      o2 = ob = e_widget_check_add(evas, _("Enable fast composite effects for overrides"), &(cfdata->fast_overrides));
      e_widget_disabled_set(ob, cfdata->match.disable_overrides);
      e_widget_framelist_object_append(of, ob);
      e_widget_list_object_append(ol, of, 1, 0, 0.5);

      of = e_widget_framelist_add(evas, _("Disable Effects"), 0);
      ob = e_widget_check_add(evas, _("Disable composite effects for windows"), &(cfdata->match.disable_borders));
      e_widget_on_change_hook_set(ob, _advanced_comp_style_toggle, w);
      e_widget_framelist_object_append(of, ob);
      ob = e_widget_check_add(evas, _("Disable composite effects for menus"), &(cfdata->match.disable_menus));
      e_widget_on_change_hook_set(ob, _advanced_comp_style_toggle, m);
      e_widget_framelist_object_append(of, ob);
      ob = e_widget_check_add(evas, _("Disable composite effects for popups"), &(cfdata->match.disable_popups));
      e_widget_on_change_hook_set(ob, _advanced_comp_style_toggle, p);
      e_widget_framelist_object_append(of, ob);
      ob = e_widget_check_add(evas, _("Disable composite effects for objects"), &(cfdata->match.disable_objects));
      e_widget_on_change_hook_set(ob, _advanced_comp_style_toggle, o1);
      e_widget_framelist_object_append(of, ob);
      ob = e_widget_check_add(evas, _("Disable composite effects for overrides"), &(cfdata->match.disable_overrides));
      e_widget_on_change_hook_set(ob, _advanced_comp_style_toggle, o2);
      e_widget_framelist_object_append(of, ob);
      ob = e_widget_check_add(evas, _("Disable composite effects for screen"), &(cfdata->disable_screen_effects));
      e_widget_framelist_object_append(of, ob);
      e_widget_list_object_append(ol, of, 1, 0, 0.5);
   }

   e_widget_toolbook_page_append(otb, NULL, _("Effects"), ol, 1, 1, 1, 1, 0.5, 0.0);

   ///////////////////////////////////////////
   ol = e_widget_list_add(evas, 0, 0);
   of = e_widget_framelist_add(evas, _("Behavior"), 0);
   ob = e_widget_check_add(evas, _("Smooth scaling"), &(cfdata->smooth_windows));
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_check_add(evas, _("Don't composite fullscreen windows"), &(cfdata->nocomp_fs));
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_check_add(evas, _("Don't fade backlight"), &(cfdata->nofade));
   e_widget_framelist_object_append(of, ob);
   e_widget_list_object_append(ol, of, 1, 1, 0.5);

   of = e_widget_framelist_add(evas, _("Engine"), 0);
   rg = e_widget_radio_group_new(&(cfdata->engine));
   ob = e_widget_radio_add(evas, _("Software"), E_COMP_ENGINE_SW, rg);
   e_widget_framelist_object_append(of, ob);
   if (!getenv("ECORE_X_NO_XLIB"))
     {
        if (ecore_evas_engine_type_supported_get(ECORE_EVAS_ENGINE_OPENGL_X11))
          {
             ob = e_widget_radio_add(evas, _("OpenGL"), E_COMP_ENGINE_GL, rg);
             e_widget_framelist_object_append(of, ob);

             ob = e_widget_label_add(evas, _("OpenGL options:"));
             e_widget_framelist_object_append(of, ob);
             ob = e_widget_check_add(evas, _("Tear-free updates (VSynced)"), &(cfdata->vsync));
             e_widget_framelist_object_append(of, ob);
             ob = e_widget_check_add(evas, _("Texture from pixmap"), &(cfdata->texture_from_pixmap));
             e_widget_framelist_object_append(of, ob);
             ob = e_widget_label_add(evas, _("Assume swapping method:"));
             e_widget_framelist_object_append(of, ob);
             rg = e_widget_radio_group_new(&(cfdata->swap_mode));
             ob = e_widget_radio_add(evas, _("Auto"), ECORE_EVAS_GL_X11_SWAP_MODE_AUTO, rg);
             e_widget_framelist_object_append(of, ob);
             ob = e_widget_radio_add(evas, _("Invalidate (full redraw)"), ECORE_EVAS_GL_X11_SWAP_MODE_FULL, rg);
             e_widget_framelist_object_append(of, ob);
             ob = e_widget_radio_add(evas, _("Copy from back to front"), ECORE_EVAS_GL_X11_SWAP_MODE_COPY, rg);
             e_widget_framelist_object_append(of, ob);
             ob = e_widget_radio_add(evas, _("Double buffered swaps"), ECORE_EVAS_GL_X11_SWAP_MODE_DOUBLE, rg);
             e_widget_framelist_object_append(of, ob);
             ob = e_widget_radio_add(evas, _("Triple buffered swaps"), ECORE_EVAS_GL_X11_SWAP_MODE_TRIPLE, rg);
             e_widget_framelist_object_append(of, ob);
// lets not offer this anymore             
//             ob = e_widget_check_add(evas, _("Indirect OpenGL (EXPERIMENTAL)"), &(cfdata->indirect));
//             e_widget_framelist_object_append(of, ob);
          }
     }
   e_widget_list_object_append(ol, of, 1, 1, 0.5);
   e_widget_toolbook_page_append(otb, NULL, _("Rendering"), ol, 0, 0, 0, 0, 0.5, 0.0);

   ///////////////////////////////////////////
   ol = e_widget_list_add(evas, 0, 0);
   of = e_widget_framelist_add(evas, _("X Messages"), 0);
   ob = e_widget_check_add(evas, _("Send flush"), &(cfdata->send_flush));
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_check_add(evas, _("Send dump"), &(cfdata->send_dump));
   e_widget_framelist_object_append(of, ob);
   e_widget_list_object_append(ol, of, 1, 1, 0.5);

/*   
   ob = e_widget_check_add(evas, _("Keep hidden windows"), &(cfdata->keep_unmapped));
   e_widget_list_object_append(ol, ob, 1, 1, 0.5);
   of = e_widget_frametable_add(evas, _("Maximum hidden pixels"), 0);
   e_widget_frametable_content_align_set(of, 0.5, 0.5);
   rg = e_widget_radio_group_new(&(cfdata->max_unmapped_pixels));
   ob = e_widget_radio_add(evas, _("1M"), 1 * 1024, rg);
   e_widget_frametable_object_append(of, ob, 0, 0, 1, 1, 1, 1, 0, 0);
   ob = e_widget_radio_add(evas, _("2M"), 2 * 1024, rg);
   e_widget_frametable_object_append(of, ob, 0, 1, 1, 1, 1, 1, 0, 0);
   ob = e_widget_radio_add(evas, _("4M"), 4 * 1024, rg);
   e_widget_frametable_object_append(of, ob, 0, 2, 1, 1, 1, 1, 0, 0);
   ob = e_widget_radio_add(evas, _("8M"), 8 * 1024, rg);
   e_widget_frametable_object_append(of, ob, 1, 0, 1, 1, 1, 1, 0, 0);
   ob = e_widget_radio_add(evas, _("16M"), 16 * 1024, rg);
   e_widget_frametable_object_append(of, ob, 1, 1, 1, 1, 1, 1, 0, 0);
   ob = e_widget_radio_add(evas, _("32M"), 32 * 1024, rg);
   e_widget_frametable_object_append(of, ob, 1, 2, 1, 1, 1, 1, 0, 0);
   ob = e_widget_radio_add(evas, _("64M"), 64 * 1024, rg);
   e_widget_frametable_object_append(of, ob, 2, 0, 1, 1, 1, 1, 0, 0);
   ob = e_widget_radio_add(evas, _("128M"), 128 * 1024, rg);
   e_widget_frametable_object_append(of, ob, 2, 1, 1, 1, 1, 1, 0, 0);
   ob = e_widget_radio_add(evas, _("256M"), 256 * 1024, rg);
   e_widget_frametable_object_append(of, ob, 2, 2, 1, 1, 1, 1, 0, 0);
   e_widget_list_object_append(ol, of, 1, 1, 0.5);
 */
   of = e_widget_framelist_add(evas, _("Sync"), 0);
   ob = e_widget_check_add(evas, _("Grab Server during draw"), &(cfdata->grab));
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_label_add(evas, _("Initial draw timeout for newly mapped windows"));
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_slider_add(evas, 1, 0, _("%1.2f Seconds"), 0.01, 0.5, 0.01, 0, &(cfdata->first_draw_delay), NULL, 150);
   e_widget_framelist_object_append(of, ob);
   e_widget_list_object_append(ol, of, 1, 1, 0.5);
   of = e_widget_framelist_add(evas, _("DANGEROUS"), 0);
   ob = e_widget_check_add(evas, _("Enable advanced compositing features"), &(cfdata->enable_advanced_features));
   e_widget_framelist_object_append(of, ob);
   e_widget_list_object_append(ol, of, 1, 1, 0.5);
   e_widget_toolbook_page_append(otb, NULL, _("Misc"), ol, 0, 0, 0, 0, 0.5, 0.0);

   ///////////////////////////////////////////
/*   
   ol = e_widget_list_add(evas, 0, 0);
   ol2 = e_widget_list_add(evas, 1, 1);
   of = e_widget_framelist_add(evas, _("Min hidden"), 0);
   e_widget_framelist_content_align_set(of, 0.5, 0.0);
   rg = e_widget_radio_group_new(&(cfdata->min_unmapped_time));
   ob = e_widget_radio_add(evas, _("30 Seconds"), 30, rg);
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_radio_add(evas, _("1 Minute"), 60, rg);
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_radio_add(evas, _("5 Minutes"), 5 * 60, rg);
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_radio_add(evas, _("30 Minutes"), 30 * 60, rg);
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_radio_add(evas, _("2 Hours"), 2 * 3600, rg);
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_radio_add(evas, _("10 Hours"), 10 * 3600, rg);
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_radio_add(evas, _("Forever"), 0, rg);
   e_widget_framelist_object_append(of, ob);
   e_widget_list_object_append(ol2, of, 1, 1, 0.5);
   of = e_widget_framelist_add(evas, _("Max hidden"), 0);
   e_widget_framelist_content_align_set(of, 0.5, 0.0);
   rg = e_widget_radio_group_new(&(cfdata->max_unmapped_time));
   ob = e_widget_radio_add(evas, _("30 Seconds"), 30, rg);
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_radio_add(evas, _("1 Minute"), 60, rg);
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_radio_add(evas, _("5 Minutes"), 5 * 60, rg);
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_radio_add(evas, _("30 Minutes"), 30 * 60, rg);
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_radio_add(evas, _("2 Hours"), 2 * 3600, rg);
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_radio_add(evas, _("10 Hours"), 10 * 3600, rg);
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_radio_add(evas, _("Forever"), 0, rg);
   e_widget_framelist_object_append(of, ob);
   e_widget_list_object_append(ol2, of, 1, 1, 0.5);
   e_widget_list_object_append(ol, ol2, 1, 1, 0.5);
   e_widget_toolbook_page_append(otb, NULL, _("Timeouts"), ol, 0, 0, 0, 0, 0.5, 0.0);
 */
   ///////////////////////////////////////////
   ol = e_widget_list_add(evas, 0, 0);

   ob = e_widget_check_add(evas, _("Show Framerate"), &(cfdata->fps_show));
   e_widget_list_object_append(ol, ob, 1, 1, 0.5);
   ob = e_widget_label_add(evas, _("Rolling average frame count"));
   e_widget_list_object_append(ol, ob, 1, 1, 0.5);
   ob = e_widget_slider_add(evas, 1, 0, _("%1.0f Frames"), 1, 120, 1, 0,
                            NULL, &(cfdata->fps_average_range), 240);
   e_widget_list_object_append(ol, ob, 1, 1, 0.5);

   of = e_widget_frametable_add(evas, _("Corner"), 0);
   e_widget_frametable_content_align_set(of, 0.5, 0.5);
   rg = e_widget_radio_group_new(&(cfdata->fps_corner));
   ob = e_widget_radio_icon_add(evas, _("Top Left"), "preferences-position-top-left",
                                24, 24, 0, rg);
   e_widget_frametable_object_append(of, ob, 0, 0, 1, 1, 1, 1, 1, 1);
   ob = e_widget_radio_icon_add(evas, _("Top Right"), "preferences-position-top-right",
                                24, 24, 1, rg);
   e_widget_frametable_object_append(of, ob, 1, 0, 1, 1, 1, 1, 1, 1);
   ob = e_widget_radio_icon_add(evas, _("Bottom Left"), "preferences-position-bottom-left",
                                24, 24, 2, rg);
   e_widget_frametable_object_append(of, ob, 0, 1, 1, 1, 1, 1, 1, 1);
   ob = e_widget_radio_icon_add(evas, _("Bottom Right"), "preferences-position-bottom-right",
                                24, 24, 3, rg);
   e_widget_frametable_object_append(of, ob, 1, 1, 1, 1, 1, 1, 1, 1);
   e_widget_list_object_append(ol, of, 1, 1, 0.5);

   e_widget_toolbook_page_append(otb, NULL, _("Debug"), ol, 0, 0, 0, 0, 0.5, 0.0);

   e_widget_toolbook_page_show(otb, 0);

   return otb;
}

static int
_advanced_apply_data(E_Config_Dialog *cfd  EINA_UNUSED,
                     E_Config_Dialog_Data *cfdata)
{
   E_Comp_Config *conf = e_comp_config_get();

   if ((cfdata->lock_fps != conf->lock_fps) ||
       (cfdata->smooth_windows != conf->smooth_windows) ||
       (cfdata->grab != conf->grab) ||
       (cfdata->keep_unmapped != conf->keep_unmapped) ||
       (cfdata->nocomp_fs != conf->nocomp_fs) ||
       (cfdata->nofade != conf->nofade) ||
       (cfdata->shadow_style != conf->shadow_style) ||
       (cfdata->max_unmapped_pixels != conf->max_unmapped_pixels) ||
       (cfdata->max_unmapped_time != conf->max_unmapped_time) ||
       (cfdata->min_unmapped_time != conf->min_unmapped_time) ||
       (cfdata->send_flush != conf->send_flush) ||
       (cfdata->send_dump != conf->send_dump) ||
       (cfdata->fps_show != conf->fps_show) ||
       (cfdata->fps_corner != conf->fps_corner) ||
       (cfdata->fps_average_range != conf->fps_average_range) ||
       (cfdata->first_draw_delay != conf->first_draw_delay) ||
       (conf->match.disable_popups != cfdata->match.disable_popups) ||
       (conf->match.disable_borders != cfdata->match.disable_borders) ||
       (conf->match.disable_overrides != cfdata->match.disable_overrides) ||
       (conf->match.disable_menus != cfdata->match.disable_menus) ||
       (conf->match.disable_objects != cfdata->match.disable_objects) ||
       (conf->disable_screen_effects != cfdata->disable_screen_effects) ||
       (conf->enable_advanced_features != cfdata->enable_advanced_features) ||
       (conf->fast_popups != cfdata->fast_popups) ||
       (conf->fast_borders != cfdata->fast_borders) ||
       (conf->fast_overrides != cfdata->fast_overrides) ||
       (conf->fast_menus != cfdata->fast_menus) ||
       (conf->fast_objects != cfdata->fast_objects)
       )
     {
        conf->fast_popups = cfdata->fast_popups;
        conf->fast_borders = cfdata->fast_borders;
        conf->fast_overrides = cfdata->fast_overrides;
        conf->fast_menus = cfdata->fast_menus;
        conf->fast_objects = cfdata->fast_objects;
        conf->match.disable_popups = cfdata->match.disable_popups;
        conf->match.disable_borders = cfdata->match.disable_borders;
        conf->match.disable_overrides = cfdata->match.disable_overrides;
        conf->match.disable_menus = cfdata->match.disable_menus;
        conf->match.disable_objects = cfdata->match.disable_objects;
        conf->disable_screen_effects = cfdata->disable_screen_effects;
        if (conf->enable_advanced_features != cfdata->enable_advanced_features)
          _advanced_features_changed(conf);
        conf->lock_fps = cfdata->lock_fps;
        conf->smooth_windows = cfdata->smooth_windows;
        conf->grab = cfdata->grab;
        conf->keep_unmapped = cfdata->keep_unmapped;
        conf->nocomp_fs = cfdata->nocomp_fs;
        conf->nofade = cfdata->nofade;
        conf->max_unmapped_pixels = cfdata->max_unmapped_pixels;
        conf->max_unmapped_time = cfdata->max_unmapped_time;
        conf->min_unmapped_time = cfdata->min_unmapped_time;
        conf->send_flush = cfdata->send_flush;
        conf->send_dump = cfdata->send_dump;
        conf->fps_show = cfdata->fps_show;
        conf->fps_corner = cfdata->fps_corner;
        conf->fps_average_range = cfdata->fps_average_range;
        conf->first_draw_delay = cfdata->first_draw_delay;
        if (conf->shadow_style)
          eina_stringshare_del(conf->shadow_style);
        conf->shadow_style = eina_stringshare_ref(cfdata->shadow_style);
        e_comp_shadows_reset();
     }
   if ((cfdata->engine != conf->engine) ||
       (cfdata->indirect != conf->indirect) ||
       (cfdata->texture_from_pixmap != conf->texture_from_pixmap) ||
       (cfdata->vsync != conf->vsync) ||
       (cfdata->swap_mode != conf->swap_mode))
     {
        E_Action *a;

        conf->engine = cfdata->engine;
        conf->indirect = cfdata->indirect;
        conf->texture_from_pixmap = cfdata->texture_from_pixmap;
        conf->vsync = cfdata->vsync;
        conf->swap_mode = cfdata->swap_mode;

        a = e_action_find("restart");
        if ((a) && (a->func.go)) a->func.go(NULL, NULL);
     }
   e_config_save_queue();
   return 1;
}

static void
_basic_comp_style_fast_toggle(void *data, Evas_Object *o EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata = data;
   cfdata->fast_changed = 1;
}

static void
_basic_comp_style_toggle(void *data, Evas_Object *o)
{
   E_Config_Dialog_Data *cfdata = data;
   
   e_widget_disabled_set(cfdata->styles_il, e_widget_check_checked_get(o));
   e_widget_disabled_set(cfdata->fast_ob, e_widget_check_checked_get(o));
   cfdata->match.toggle_changed = 1;
}

static Evas_Object *
_basic_create_widgets(E_Config_Dialog *cfd,
                      Evas *evas,
                      E_Config_Dialog_Data *cfdata)
{
   Evas_Object *ob,*ol, *of, *otb, *oi, *orec0, *tab;
   E_Radio_Group *rg;

   e_dialog_resizable_set(cfd->dia, 0);
   elm_win_center(cfd->dia->win, 1, 1);
   orec0 = evas_object_rectangle_add(evas);
   evas_object_name_set(orec0, "style_shadows");

   tab = e_widget_table_add(e_win_evas_win_get(evas), 0);
   otb = e_widget_toolbook_add(evas, 48 * e_scale, 48 * e_scale);

   ///////////////////////////////////////////
   ol = e_widget_list_add(evas, 0, 0);

   ob = e_widget_check_add(evas, _("Don't fade backlight"), &(cfdata->nofade));
   e_widget_list_object_append(ol, ob, 1, 0, 0.5);
   
   cfdata->fast =
     (cfdata->fast_menus && cfdata->fast_overrides && cfdata->fast_borders && cfdata->fast_popups && cfdata->fast_objects);
   cfdata->fast_ob = ob = e_widget_check_add(evas, _("Enable \"fast\" composite effects"), &(cfdata->fast));
   evas_object_data_set(ob, "cfdata", cfdata);
   e_widget_list_object_append(ol, ob, 1, 0, 0.5);
   e_widget_on_change_hook_set(ob, _basic_comp_style_fast_toggle, cfdata);

   cfdata->match.disable_all =
     (cfdata->match.disable_menus && cfdata->match.disable_overrides && cfdata->match.disable_borders &&
      cfdata->match.disable_popups && cfdata->match.disable_objects && cfdata->disable_screen_effects);
   e_widget_disabled_set(ob, cfdata->match.disable_all);
   ob = e_widget_check_add(evas, _("Disable composite effects"), &(cfdata->match.disable_all));
   evas_object_data_set(ob, "cfdata", cfdata);
   e_widget_list_object_append(ol, ob, 1, 0, 0.5);

   of = e_widget_frametable_add(evas, _("Select default style"), 0);
   e_widget_frametable_content_align_set(of, 0.5, 0.5);
   cfdata->styles_il = oi = e_comp_style_selector_create(evas, &(cfdata->shadow_style));
   e_widget_frametable_object_append(of, oi, 0, 0, 1, 1, 1, 1, 1, 1);
   e_widget_list_object_append(ol, of, 1, 1, 0.5);

   e_widget_on_change_hook_set(ob, _basic_comp_style_toggle, cfdata);

   e_widget_toolbook_page_append(otb, NULL, _("General"), ol, 1, 1, 1, 1, 0.5, 0.0);

   ///////////////////////////////////////////
   ol = e_widget_list_add(evas, 0, 0);

   of = e_widget_framelist_add(evas, _("Behavior"), 0);

   ob = e_widget_check_add(evas, _("Tear-free updates (VSynced)"), &(cfdata->vsync));
   e_widget_framelist_object_append(of, ob);
   
   ob = e_widget_check_add(evas, _("Smooth scaling of window content"), &(cfdata->smooth_windows));
   e_widget_framelist_object_append(of, ob);

   ob = e_widget_check_add(evas, _("Don't composite fullscreen windows"), &(cfdata->nocomp_fs));
   e_widget_framelist_object_append(of, ob);

   e_widget_list_object_append(ol, of, 1, 0, 0.5);

   of = e_widget_framelist_add(evas, _("Engine"), 0);
   rg = e_widget_radio_group_new(&(cfdata->engine));
   ob = e_widget_radio_add(evas, _("Software"), E_COMP_ENGINE_SW, rg);
   e_widget_framelist_object_append(of, ob);
   if (!getenv("ECORE_X_NO_XLIB"))
     {
        if (ecore_evas_engine_type_supported_get(ECORE_EVAS_ENGINE_OPENGL_X11))
          {
             ob = e_widget_radio_add(evas, _("OpenGL"), E_COMP_ENGINE_GL, rg);
             e_widget_framelist_object_append(of, ob);
          }
     }
   ob = e_widget_label_add(evas, _("To reset compositor:"));
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_label_add(evas, _("Ctrl+Alt+Shift+Home"));
   e_widget_framelist_object_append(of, ob);

   e_widget_list_object_append(ol, of, 1, 0, 0.5);
   
   e_widget_toolbook_page_append(otb, NULL, _("Rendering"), ol, 0, 0, 0, 0, 0.5, 0.0);

   e_widget_toolbook_page_show(otb, 0);


   e_widget_table_object_append(tab, otb, 0, 0, 1, 1, 1, 1, 1, 1);
   return tab;
}

static int
_basic_apply_data(E_Config_Dialog *cfd  EINA_UNUSED,
                  E_Config_Dialog_Data *cfdata)
{
   E_Comp_Config *conf = e_comp_config_get();

   if (cfdata->match.toggle_changed || cfdata->fast_changed ||
       (cfdata->lock_fps != conf->lock_fps) ||
       (cfdata->smooth_windows != conf->smooth_windows) ||
       (cfdata->grab != conf->grab) ||
       (cfdata->nofade != conf->nofade) ||
       (cfdata->keep_unmapped != conf->keep_unmapped) ||
       (cfdata->nocomp_fs != conf->nocomp_fs) ||
       (cfdata->shadow_style != conf->shadow_style) ||
       (cfdata->max_unmapped_pixels != conf->max_unmapped_pixels) ||
       (cfdata->max_unmapped_time != conf->max_unmapped_time) ||
       (cfdata->min_unmapped_time != conf->min_unmapped_time) ||
       (cfdata->send_flush != conf->send_flush) ||
       (cfdata->send_dump != conf->send_dump) ||
       (cfdata->fps_show != conf->fps_show) ||
       (cfdata->fps_corner != conf->fps_corner) ||
       (cfdata->fps_average_range != conf->fps_average_range) ||
       (cfdata->first_draw_delay != conf->first_draw_delay)
       )
     {
        if (cfdata->match.toggle_changed)
          {
             conf->match.disable_popups = cfdata->match.disable_popups = cfdata->match.disable_all;
             conf->match.disable_borders = cfdata->match.disable_borders = cfdata->match.disable_all;
             conf->match.disable_overrides = cfdata->match.disable_overrides = cfdata->match.disable_all;
             conf->match.disable_menus = cfdata->match.disable_menus = cfdata->match.disable_all;
             conf->match.disable_objects = cfdata->match.disable_objects = cfdata->match.disable_all;
             conf->disable_screen_effects = cfdata->disable_screen_effects = cfdata->match.disable_all;
          }
        if (cfdata->fast_changed)
          {
             conf->fast_borders = cfdata->fast_borders = cfdata->fast;
             conf->fast_popups = cfdata->fast_popups = cfdata->fast;
             conf->fast_menus = cfdata->fast_menus = cfdata->fast;
             conf->fast_objects = cfdata->fast_objects = cfdata->fast;
             conf->fast_overrides = cfdata->fast_overrides = cfdata->fast;
          }
        conf->lock_fps = cfdata->lock_fps;
        conf->smooth_windows = cfdata->smooth_windows;
        conf->grab = cfdata->grab;
        conf->nofade = cfdata->nofade;
        conf->keep_unmapped = cfdata->keep_unmapped;
        conf->nocomp_fs = cfdata->nocomp_fs;
        conf->max_unmapped_pixels = cfdata->max_unmapped_pixels;
        conf->max_unmapped_time = cfdata->max_unmapped_time;
        conf->min_unmapped_time = cfdata->min_unmapped_time;
        if (conf->enable_advanced_features != cfdata->enable_advanced_features)
          _advanced_features_changed(conf);
        conf->send_flush = cfdata->send_flush;
        conf->send_dump = cfdata->send_dump;
        conf->fps_show = cfdata->fps_show;
        conf->fps_corner = cfdata->fps_corner;
        conf->fps_average_range = cfdata->fps_average_range;
        conf->first_draw_delay = cfdata->first_draw_delay;
        if (conf->shadow_style)
          eina_stringshare_del(conf->shadow_style);
        conf->shadow_style = NULL;
        if (cfdata->shadow_style)
          conf->shadow_style = eina_stringshare_add(cfdata->shadow_style);
        e_comp_shadows_reset();
     }
   if ((cfdata->engine != conf->engine) ||
       (cfdata->indirect != conf->indirect) ||
       (cfdata->texture_from_pixmap != conf->texture_from_pixmap) ||
       (cfdata->vsync != conf->vsync))
     {
        E_Action *a;

        conf->engine = cfdata->engine;
        conf->indirect = cfdata->indirect;
        conf->texture_from_pixmap = cfdata->texture_from_pixmap;
        conf->vsync = cfdata->vsync;

        a = e_action_find("restart");
        if ((a) && (a->func.go)) a->func.go(NULL, NULL);
     }
   return e_comp_internal_save();
}

