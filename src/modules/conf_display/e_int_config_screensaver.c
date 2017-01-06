#include "e.h"

#define E_CONFIG_BLANKING_DEFAULT 2
#define E_CONFIG_BLANKING_PREFERRED 1
#define E_CONFIG_BLANKING_NOT_PREFERRED 0

#define E_CONFIG_EXPOSURES_DEFAULT 2
#define E_CONFIG_EXPOSURES_ALLOWED 1
#define E_CONFIG_EXPOSURES_NOT_ALLOWED 0

static void *_create_data(E_Config_Dialog *cfd);
static void _free_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static int _basic_apply(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static int _basic_check_changed(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata);
static Evas_Object *_basic_create(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata);
static void _cb_ask_presentation_changed(void *data, Evas_Object *obj);

struct _E_Config_Dialog_Data
{
   E_Config_Dialog *cfd;

   int enable_screensaver;
   double timeout;
   int ask_presentation;
   double ask_presentation_timeout;
   
   int screensaver_suspend;
   int screensaver_suspend_on_ac;
   double screensaver_suspend_delay;

   int wake_on_notify;
   int wake_on_urgent;
   int no_dpms_on_fullscreen;

   struct 
     {
        Evas_Object *ask_presentation_slider;
     } gui;
};

E_Config_Dialog *
e_int_config_screensaver(Evas_Object *parent EINA_UNUSED, const char *params EINA_UNUSED)
{
   E_Config_Dialog *cfd;
   E_Config_Dialog_View *v;

   if (e_config_dialog_find("E", "screen/screen_saver")) return NULL;
   v = E_NEW(E_Config_Dialog_View, 1);

   v->create_cfdata = _create_data;
   v->free_cfdata = _free_data;
   v->basic.apply_cfdata = _basic_apply;
   v->basic.create_widgets = _basic_create;
   v->basic.check_changed = _basic_check_changed;

   v->override_auto_apply = 1;

   cfd = e_config_dialog_new(NULL,_("Screen Blank Settings"),
			     "E", "screen/screen_saver",
			     "preferences-desktop-screensaver", 0, v, NULL);
   return cfd;
}

static void
_fill_data(E_Config_Dialog_Data *cfdata)
{
   cfdata->enable_screensaver = e_config->screensaver_enable;
   cfdata->timeout = (double)e_config->screensaver_timeout / 60.0;
   cfdata->ask_presentation = e_config->screensaver_ask_presentation;
   cfdata->ask_presentation_timeout = e_config->screensaver_ask_presentation_timeout;
   cfdata->screensaver_suspend = e_config->screensaver_suspend;
   cfdata->screensaver_suspend_on_ac = e_config->screensaver_suspend_on_ac;
   cfdata->screensaver_suspend_delay = e_config->screensaver_suspend_delay;

   cfdata->wake_on_notify = e_config->screensaver_wake_on_notify;
   cfdata->wake_on_urgent = e_config->screensaver_wake_on_urgent;
   cfdata->no_dpms_on_fullscreen = e_config->no_dpms_on_fullscreen;
}

static void *
_create_data(E_Config_Dialog *cfd)
{
   E_Config_Dialog_Data *cfdata;

   cfdata = E_NEW(E_Config_Dialog_Data, 1);
   cfdata->cfd = cfd;
   _fill_data(cfdata);
   return cfdata;
}

static void
_free_data(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
   free(cfdata);
}

static int
_basic_apply(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
   e_config->screensaver_enable = cfdata->enable_screensaver;
   e_config->screensaver_timeout = lround(cfdata->timeout * 60.0);
   e_config->screensaver_ask_presentation = cfdata->ask_presentation;
   e_config->screensaver_ask_presentation_timeout = cfdata->ask_presentation_timeout;
   e_config->screensaver_suspend = cfdata->screensaver_suspend;
   e_config->screensaver_suspend_on_ac = cfdata->screensaver_suspend_on_ac;
   e_config->screensaver_suspend_delay = cfdata->screensaver_suspend_delay;

   e_config->screensaver_wake_on_notify = cfdata->wake_on_notify;
   e_config->screensaver_wake_on_urgent = cfdata->wake_on_urgent;

   // enough of dpms vs screensaver being different! useless
//   e_config->dpms_enable = e_config->screensaver_enable;
//   e_config->dpms_standby_enable = e_config->dpms_enable;
//   e_config->dpms_suspend_enable = e_config->dpms_enable;
//   e_config->dpms_off_enable = e_config->dpms_enable;
   // XXX: 5 seconds after screenblank until dpms - enough for most anims;
//   e_config->dpms_standby_timeout = e_config->screensaver_timeout;
//   e_config->dpms_suspend_timeout = e_config->screensaver_timeout;
//   e_config->dpms_off_timeout = e_config->screensaver_timeout;

   e_config->screensaver_interval = 0;

   if ((e_config->backlight.idle_dim) &&
       (e_config->backlight.timer > (e_config->screensaver_timeout)))
     {
        e_config->screensaver_timeout = e_config->backlight.timer;
        e_config->dpms_standby_timeout = e_config->screensaver_timeout;
        e_config->dpms_suspend_timeout = e_config->screensaver_timeout;
        e_config->dpms_off_timeout = e_config->screensaver_timeout;
     }
   
   e_config->no_dpms_on_fullscreen = cfdata->no_dpms_on_fullscreen;
   
   /* Apply settings */
   e_screensaver_update();
   e_dpms_update();
   
   e_config_save_queue();
   return 1;
}

static int
_basic_check_changed(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
   return ((e_config->screensaver_enable != cfdata->enable_screensaver) ||
	   (e_config->screensaver_timeout != lround(cfdata->timeout * 60.0)) ||
	   (e_config->screensaver_ask_presentation != cfdata->ask_presentation) ||
	   (!EINA_DBL_CMP(e_config->screensaver_ask_presentation_timeout, cfdata->ask_presentation_timeout)) ||
	   (e_config->screensaver_suspend != cfdata->screensaver_suspend) ||
	   (e_config->screensaver_suspend_on_ac != cfdata->screensaver_suspend_on_ac) ||
	   (!EINA_DBL_CMP(e_config->screensaver_suspend_delay, cfdata->screensaver_suspend_delay)) ||
	   (e_config->screensaver_wake_on_notify != cfdata->wake_on_notify) ||
	   (e_config->screensaver_wake_on_urgent != cfdata->wake_on_urgent) ||
           (e_config->dpms_off_timeout != cfdata->no_dpms_on_fullscreen)
    );
}

static Evas_Object *
_basic_create(E_Config_Dialog *cfd EINA_UNUSED, Evas *evas, E_Config_Dialog_Data *cfdata)
{
   Evas_Object *otb, *ol, *ow, *oc, *oc2;

   e_dialog_resizable_set(cfd->dia, 1);
   otb = e_widget_toolbook_add(evas, (24 * e_scale), (24 * e_scale));

   /* Screensaver */
   ol = e_widget_list_add(evas, 0, 0);
   oc = e_widget_check_add(evas, _("Enable screen blanking"),
                           &(cfdata->enable_screensaver));
   e_widget_list_object_append(ol, oc, 1, 1, 0.5);
   
   ow = e_widget_label_add(evas, _("Timeout"));
   e_widget_check_widget_disable_on_unchecked_add(oc, ow);
   e_widget_list_object_append(ol, ow, 1, 1, 0.5);
   ow = e_widget_slider_add(evas, 1, 0, _("%1.0f minutes"),
			    0.5, 90.0, 1.0, 0, &(cfdata->timeout), NULL, 100);
   e_widget_check_widget_disable_on_unchecked_add(oc, ow);
   e_widget_list_object_append(ol, ow, 1, 1, 0.5);
   
   ow = e_widget_check_add(evas, _("Suspend on blank"), 
                           &(cfdata->screensaver_suspend));
   e_widget_check_widget_disable_on_unchecked_add(oc, ow);
   e_widget_list_object_append(ol, ow, 1, 1, 0.5);
   ow = e_widget_check_add(evas, _("Suspend even if AC"),
                           &(cfdata->screensaver_suspend_on_ac));
   e_widget_check_widget_disable_on_unchecked_add(oc, ow);
   e_widget_list_object_append(ol, ow, 1, 1, 0.5);
   
   ow = e_widget_label_add(evas, _("Suspend delay"));
   e_widget_check_widget_disable_on_unchecked_add(oc, ow);
   e_widget_list_object_append(ol, ow, 1, 1, 0.5);
   ow = e_widget_slider_add(evas, 1, 0, _("%1.0f seconds"),
			    1.0, 20.0, 1.0, 0, &(cfdata->screensaver_suspend_delay), NULL, 100);
   e_widget_check_widget_disable_on_unchecked_add(oc, ow);
   e_widget_list_object_append(ol, ow, 1, 1, 0.5);
   
   oc = e_widget_check_add(evas, _("Disable blanking for Fullscreen Windows"),
                           &(cfdata->no_dpms_on_fullscreen));
   e_widget_list_object_append(ol, oc, 1, 1, 0.5);

   e_widget_toolbook_page_append(otb, NULL, _("Blanking"), ol, 
                                 1, 0, 1, 0, 0.5, 0.0);

   /* Wakeups */
   ol = e_widget_list_add(evas, 0, 0);
   oc = e_widget_check_add(evas, _("Wake on notification"),
                           &(cfdata->wake_on_notify));
   e_widget_list_object_append(ol, oc, 1, 1, 0.5);
   oc = e_widget_check_add(evas, _("Wake on urgency"),
                           &(cfdata->wake_on_urgent));
   e_widget_list_object_append(ol, oc, 1, 1, 0.5);
   e_widget_toolbook_page_append(otb, NULL, _("Wakeups"), ol,
                                 1, 0, 1, 0, 0.5, 0.0);
   /* Presentation */
   ol = e_widget_list_add(evas, 0, 0);
   oc2 = e_widget_check_add(evas, _("Suggest if deactivated before"), 
                           &(cfdata->ask_presentation));
   e_widget_check_widget_disable_on_unchecked_add(oc, oc2);
   e_widget_list_object_append(ol, oc2, 1, 1, 0.5);
   ow = e_widget_slider_add(evas, 1, 0, _("%1.0f seconds"),
			    1.0, 300.0, 10.0, 0,
			    &(cfdata->ask_presentation_timeout), NULL, 100);
   cfdata->gui.ask_presentation_slider = ow;
   e_widget_check_widget_disable_on_unchecked_add(oc, ow);
   e_widget_check_widget_disable_on_unchecked_add(oc2, ow);
   e_widget_on_disable_hook_set(ow, _cb_ask_presentation_changed, cfdata);
   e_widget_list_object_append(ol, ow, 1, 1, 0.5);
   e_widget_toolbook_page_append(otb, NULL, _("Presentation"), ol,
                                 1, 0, 1, 0, 0.5, 0.0);
   
   e_widget_toolbook_page_show(otb, 0);

   return otb;
}

static void
_cb_ask_presentation_changed(void *data, Evas_Object *obj EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata = data;
   Eina_Bool disable;

   disable = ((!cfdata->enable_screensaver) || (!cfdata->ask_presentation));
   e_widget_disabled_set(cfdata->gui.ask_presentation_slider, disable);
}
