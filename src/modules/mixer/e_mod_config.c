#include "e.h"
#include "e_mod_config.h"
#include "e_mod_main.h"
#include "emix.h"

typedef struct _Emix_Config
{
   const char *backend;
   int notify;
   int mute;

   int save;
   int save_mute;
   int save_volume;

   emix_config_backend_changed cb;
   const void *userdata;
} Emix_Config;

struct _E_Config_Dialog_Data
{
   Emix_Config config;
   Evas_Object *list;
};

static E_Config_DD *cd;
static Emix_Config *_config;

static E_Config_DD*
_emix_config_dd_new(void)
{
   E_Config_DD *result = E_CONFIG_DD_NEW("Emix_Config", Emix_Config);

   E_CONFIG_VAL(result, Emix_Config, backend, STR);
   E_CONFIG_VAL(result, Emix_Config, notify, INT);
   E_CONFIG_VAL(result, Emix_Config, mute, INT);

   E_CONFIG_VAL(result, Emix_Config, save, INT);
   E_CONFIG_VAL(result, Emix_Config, save_mute, INT);
   E_CONFIG_VAL(result, Emix_Config, save_volume, INT);

   return result;
}

const char *
emix_config_backend_get(void)
{
   return _config->backend;
}

void
emix_config_backend_set(const char *backend)
{
   eina_stringshare_replace(&_config->backend, backend);
   e_config_domain_save("module.emix", cd, _config);
}

Eina_Bool
emix_config_notify_get(void)
{
   return _config->notify;
}

Eina_Bool
emix_config_desklock_mute_get(void)
{
   return _config->mute;
}

static void
_config_set(Emix_Config *config)
{
   if ((config->backend) && (_config->backend != config->backend))
     eina_stringshare_replace(&_config->backend, config->backend);

   _config->notify = config->notify;
   _config->mute = config->mute;

   if (config->save == 0) _config->save = -1;
   else if (config->save == 1) _config->save = 1;

   DBG("SAVING CONFIG %s %d %d", _config->backend, config->notify,
       config->mute);
   e_config_domain_save("module.emix", cd, config);
}

void
emix_config_init(emix_config_backend_changed cb, const void *userdata)
{
   const Eina_List *l;

   EINA_SAFETY_ON_FALSE_RETURN(emix_init());
   cd = _emix_config_dd_new();
   _config = e_config_domain_load("module.emix", cd);
   if (!_config)
     {
        _config = E_NEW(Emix_Config, 1);
        l = emix_backends_available();
        if (l)
          _config->backend = eina_stringshare_add(l->data);
     }

   if (_config->save == 0)
     {
        _config->save = 1;
        _config->save_mute = 0;
        _config->save_volume = 100;
     }

   _config->cb = cb;
   _config->userdata = userdata;
   DBG("Config loaded, backend to use: %s", _config->backend);
}

void
emix_config_shutdown(void)
{
   E_CONFIG_DD_FREE(cd);
   if (_config->backend)
     eina_stringshare_del(_config->backend);
   free(_config);
   emix_shutdown();
}

void
emix_config_save(void)
{
   if ((_config) && (cd))
     e_config_domain_save("module.emix", cd, _config);
}

Eina_Bool
emix_config_save_get(void)
{
   if (_config->save == 1) return EINA_TRUE;
   return EINA_FALSE;
}

Eina_Bool
emix_config_save_mute_get(void)
{
   return _config->save_mute;
}

void
emix_config_save_mute_set(Eina_Bool mute)
{
   _config->save_mute = mute;
   if (_config->save == 1) e_config_save_queue();
}

int
emix_config_save_volume_get(void)
{
   return _config->save_volume;
}

void
emix_config_save_volume_set(int volume)
{
   _config->save_volume = volume;
   if (_config->save == 1) e_config_save_queue();
}

static void*
_create_data(E_Config_Dialog *cfg EINA_UNUSED)
{
   E_Config_Dialog_Data *d;

   d = E_NEW(E_Config_Dialog_Data, 1);
   d->config.backend = eina_stringshare_add(_config->backend);
   d->config.notify = _config->notify;
   d->config.mute = _config->mute;

   if (_config->save == -1) d->config.save = 0;
   else if (_config->save == 1) d->config.save = 1;
   else d->config.save = 1;

   return d;
}

static void
_free_data(E_Config_Dialog *c EINA_UNUSED, E_Config_Dialog_Data *cf)
{
   eina_stringshare_del(cf->config.backend);
   free(cf);
}

static Evas_Object *
_basic_create_widgets(E_Config_Dialog *cfd EINA_UNUSED, Evas *evas,
                      E_Config_Dialog_Data *cfdata)
{
   Evas_Object *o, *l;
   const Eina_List *node;
   char *name;
   int i = 0;

   o = e_widget_list_add(evas, 0, 0);

   l = e_widget_check_add(evas, "Notify on volume change", &cfdata->config.notify);
   e_widget_list_object_append(o, l, 0, 0, 0);

   l = e_widget_check_add(evas, "Mute on lock", &cfdata->config.mute);
   e_widget_list_object_append(o, l, 0, 0, 0);

   l = e_widget_check_add(evas, "Remember", &cfdata->config.save);
   e_widget_list_object_append(o, l, 0, 0, 0);

   l = e_widget_label_add(evas, "Backend to use:");
   e_widget_list_object_append(o, l, 0, 0, 0);

   cfdata->list = l = e_widget_ilist_add(evas, 0, 0, NULL);
   e_widget_ilist_multi_select_set(l, EINA_FALSE);
   e_widget_size_min_set(l, 100, 100);
   EINA_LIST_FOREACH(emix_backends_available(), node, name)
     {
        e_widget_ilist_append(l, NULL, name, NULL, NULL, NULL);
        i ++;
        if (_config->backend && !strcmp(_config->backend, name))
          e_widget_ilist_selected_set(l, i);
     }
   e_widget_ilist_go(l);
   e_widget_ilist_thaw(l);
   e_widget_list_object_append(o, l, 1, 1, 0);

   return o;
}

static int
_basic_apply_data(E_Config_Dialog *cfd EINA_UNUSED,
                  E_Config_Dialog_Data *cfdata)
{
   char *new_backend = eina_list_nth(
                            emix_backends_available(),
                            e_widget_ilist_selected_get(cfdata->list));

   eina_stringshare_replace(&cfdata->config.backend, new_backend);

   _config_set(&cfdata->config);
   if (_config->cb)
      _config->cb(new_backend, (void *)_config->userdata);
   return 1;
}

E_Config_Dialog*
emix_config_popup_new(Evas_Object *comp, const char *p EINA_UNUSED)
{
   E_Config_Dialog *cfd;
   E_Config_Dialog_View *v;

   if (e_config_dialog_find("E", "windows/emix"))
      return NULL;

   v = E_NEW(E_Config_Dialog_View, 1);
   v->create_cfdata = _create_data;
   v->free_cfdata = _free_data;
   v->basic.apply_cfdata = _basic_apply_data;
   v->basic.create_widgets = _basic_create_widgets;

   cfd = e_config_dialog_new(comp,
                             "Emix Configuration",
                             "E", "windows/emix",
                             NULL,
                             0, v, NULL);
   return cfd;
}
