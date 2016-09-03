#include <e.h>
#include <Eeze.h>
#include "e_mod_main.h"
#include "imfos_devices.h"

typedef struct _Imfos_Config_Panel
{
   Evas_Object *main;
   Evas_Object *current;
   Imfos_Device *dev;
   Imfos_Device *config;

} Imfos_Config_Panel;

static void _v4l_create(void);
static void _device_create(void);
static void *_create_data(E_Config_Dialog *cfd);
static void _free_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static void _fill_data(E_Config_Dialog_Data *cfdata);
static Evas_Object *_basic_create(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata);
static int _apply(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);

Imfos_Config_Panel *_tmp_cfg = NULL;
static Evas_Object *_imfos_cam_widget = NULL;
int flipped = EINA_FALSE;


EAPI E_Config_Dialog *
e_mod_config_imfos(Evas_Object *parent, const char *params EINA_UNUSED)
{
   E_Config_Dialog *cfd = NULL;
   E_Config_Dialog_View *v = NULL;
   char buf[PATH_MAX];


   if (e_config_dialog_find("Imfos", "advanced/Imfos")) return NULL;

   v = E_NEW(E_Config_Dialog_View, 1);
   if (!v) return NULL;

   v->create_cfdata = _create_data;
   v->free_cfdata = _free_data;
   v->basic.create_widgets = _basic_create;
   v->basic.apply_cfdata = _apply;

   /* Icon */
   cfd = e_config_dialog_new(NULL, "Imfos module", "Imfos", "advanced/Imfos",
                             NULL, 0, v, NULL);
   return cfd;
}

static void *
_create_data(E_Config_Dialog *cfd)
{
   _tmp_cfg = calloc(1, sizeof(Imfos_Config_Panel));
   return NULL;
}

static void
_free_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata)
{
   free(_tmp_cfg->config);
   free(_tmp_cfg);
   _tmp_cfg = NULL;
   _imfos_cam_widget = NULL;
}

static void
_fill_data(E_Config_Dialog_Data *cfdata)
{
}

static void
_imfos_device_changed(void *data)
{
   Imfos_Device *dev;

   dev = data;
   if (_tmp_cfg->dev)
     imfos_devices_cancel(_tmp_cfg->dev);
   _tmp_cfg->dev = dev;
   _tmp_cfg->config = malloc(sizeof(Imfos_Device));
   /* Warning just a copy ! */
   memcpy(_tmp_cfg->config, dev, sizeof(Imfos_Device));
   if (_tmp_cfg->current)
       evas_object_del(_tmp_cfg->current);
   _tmp_cfg->current = e_widget_list_add(evas_object_evas_get(_tmp_cfg->main), 0, 0);
   e_widget_list_object_append(_tmp_cfg->main, _tmp_cfg->current, 1, 0, 0.5);
   _device_create();
   printf ("sel %s\n", dev->name);

}

static void
_imfos_cam_widget_delete(void *data, Evas *evas, Evas_Object *obj, void *event_info)
{
   Imfos_Device *dev;

   dev = data;
   printf("deleting cam widget\n");

   dev->param.v4l.cam = NULL;
   imfos_devices_cancel(dev);
}

static void
_fill_devices(Evas *evas, Evas_Object *list)
{
   const Eina_List *devices, *l;
   Imfos_Device *dev;
   char buf[1024];
   Evas_Object *o;

   EINA_LIST_FOREACH(imfos_config->devices, l, dev)
     {
        snprintf(buf, sizeof(buf), "%s(%s)", dev->name, dev->dev_name);
        e_widget_ilist_append(list, NULL, buf, _imfos_device_changed, dev, NULL);
        printf("dev %s(%s)\n", dev->name, dev->dev_name);
     }
   e_widget_ilist_selected_set(list, 1);
   _imfos_device_changed(eina_list_data_get(imfos_config->devices));
}

static void
_v4l_create(void)
{
   Evas_Object *o;
   Evas *evas;
   Evas_Object *list;


   list = _tmp_cfg->current;
   evas = evas_object_evas_get(_tmp_cfg->current);

   printf("V4l panel\n");
   o = evas_object_image_filled_add(evas);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_min_set(o, 320, 240);
   e_widget_list_object_append(list, o, 1, 1, 0.5);
   _tmp_cfg->dev->param.v4l.cam = o;
//   if (_tmp_cfg->config->enabled)
//     imfos_devices_run(_tmp_cfg->dev);
   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, _imfos_cam_widget_delete, _tmp_cfg->dev);

   o = e_widget_check_add(evas, "Flip camera", &flipped);
   e_widget_list_object_append(list, o, 1, 0, 0.5);

   o = e_widget_check_add(evas, "Set as the fallback", &flipped);
   e_widget_list_object_append(list, o, 1, 0, 0.5);
}

static void
_device_create(void)
{
   Evas_Object *o;
   Evas *evas;
   Evas_Object *list;
   char buf[4096];

   list = _tmp_cfg->current;
   evas = evas_object_evas_get(_tmp_cfg->current);

   printf("Device Create\n");
   o = e_widget_check_add(evas, "Enabled",
                          (int *)&_tmp_cfg->config->enabled);
   e_widget_list_object_append(list, o, 1, 0, 0.0);
   snprintf(buf, sizeof(buf), "Name : %s", _tmp_cfg->config->name);
   o = e_widget_label_add(evas, buf);
   e_widget_list_object_append(list, o, 1, 0, 0.0);
   snprintf(buf, sizeof(buf), "System path : %s", _tmp_cfg->config->syspath);
   o = e_widget_label_add(evas, buf);
   e_widget_list_object_append(list, o, 1, 0, 0.0);
   snprintf(buf, sizeof(buf), "Device path : %s", _tmp_cfg->config->dev_name);
   o = e_widget_label_add(evas, buf);
   e_widget_list_object_append(list, o, 1, 0, 0.0);
   o = e_widget_check_add(evas, "Lock the device path",
                          (int *)&_tmp_cfg->config->dev_name_locked);
   e_widget_list_object_append(list, o, 1, 0, 0.0);
   _v4l_create();
   o = e_widget_slider_add(evas, 1, 0, "Priority : %d", -100.0, 100.0, 5.0, 50,
                           NULL, &_tmp_cfg->config->priority, 110);
   e_widget_list_object_append(list, o, 1, 0, 0.5);
   o = e_widget_check_add(evas, "async", (int *)&_tmp_cfg->config->async);
   e_widget_list_object_append(list, o, 1, 0, 0.5);
   o = e_widget_check_add(evas, "Timeout", (int *)&_tmp_cfg->config->timeout_enabled);
   e_widget_list_object_append(list, o, 1, 0, 0.5);
   o = e_widget_slider_add(evas, 1, 0, "Timeout (%2.0f)", 0.0, 64580.0, 5.0, 0,
                           NULL, &_tmp_cfg->config->timeout, 110);
   e_widget_list_object_append(list, o, 1, 0, 0.5);
//   o = e_widget_radio_group_new((int*) &(_tmp_cfg->config->powersave_min_state));
//   e_widget_list_object_append(list, o, 1, 0, 0.5);
   o = e_widget_check_add(evas, "Force the login when detected",
                          (int*)&_tmp_cfg->config->auto_login);
   e_widget_list_object_append(list, o, 1, 0, 0.5);
   o = e_widget_check_add(evas, "Force the logoff when missmatch",
                          (int*)&_tmp_cfg->config->auto_logout);
   e_widget_list_object_append(list, o, 1, 0, 0.5);
   o = e_widget_check_add(evas, "Report the screensaver when detected",
                          (int*)&_tmp_cfg->config->auto_screen_on);
   e_widget_list_object_append(list, o, 1, 0, 0.5);
   o = e_widget_check_add(evas, "Enable the screensaver when missmatch",
                          (int*)&_tmp_cfg->config->auto_screen_off);
   e_widget_list_object_append(list, o, 1, 0, 0.5);


   /*
   powersave_min_state
   */

}


static Evas_Object *
_basic_create(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata)
{
   Evas_Object *list, *list_cam, *devices_list, *o;

   _tmp_cfg->main = e_widget_list_add(evas, 0, 1);

   o = e_widget_ilist_add(evas, 0, 0, NULL);
   e_widget_ilist_multi_select_set(o, EINA_FALSE);
   evas_object_size_hint_min_set(o, 160, 240);
   e_widget_list_object_append(_tmp_cfg->main, o, 1, 0, 0.5);
   evas_object_show(o);

   _fill_devices(evas, o);
   return _tmp_cfg->main;
}

static int
_apply(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata)
{
}




