/**
 * @addtogroup Optional_Gadgets
 * @{
 *
 * @defgroup Module_Pants Pants Tracker
 *
 * Tracks the state of your pants
 *
 * @}
 */
#include "Elementary.h"
#include "Evas.h"
#include "e.h"
#include "eina_types.h"
#include "elm_table_eo.legacy.h"

/* gadcon requirements */
static E_Gadcon_Client *_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style);
static void             _gc_shutdown(E_Gadcon_Client *gcc);
static void             _gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient);
static const char      *_gc_label(const E_Gadcon_Client_Class *client_class);
static Evas_Object     *_gc_icon(const E_Gadcon_Client_Class *client_class, Evas *evas);
static const char      *_gc_id_new(const E_Gadcon_Client_Class *client_class);
/* and actually define the gadcon class that this module provides (just 1) */
static const E_Gadcon_Client_Class _gadcon_class =
{
   GADCON_CLIENT_CLASS_VERSION,
   "pants",
   {
      _gc_init, _gc_shutdown, _gc_orient, _gc_label, _gc_icon, _gc_id_new, NULL,
      e_gadcon_site_is_not_toolbar
   },
   E_GADCON_CLIENT_STYLE_PLAIN
};

/* actual module specifics */
typedef struct _Instance Instance;
typedef struct _Item     Item;
typedef struct _Config   Config;

struct _Instance
{
   E_Gadcon_Client *gcc;
   Evas_Object     *o_gad;
   Evas_Object     *o_ggrid;
   E_Gadcon_Popup  *popup;
};

struct _Item
{
  Instance *inst;
  char      file[];
};

struct _Config
{
  const char *pants;
};

static void _button_cb_mouse_down(void *data, Evas *e, Evas_Object *obj, void *event_info);

static E_Module *_module = NULL;
static E_Config_DD *_conf_edd = NULL;
static Config *_config = NULL;

static void
_pants_env_set(const char *imfile)
{
  char   buf[PATH_MAX];
  FILE  *f;
  size_t len;

  if (!imfile) return;
  snprintf(buf, sizeof(buf), "%s.txt", imfile);
  f = fopen(buf, "r");
  if (f)
    {
      if ((fgets(buf, sizeof(buf), f)) &&
          (fgets(buf, sizeof(buf), f)))
        {
          // second line is pants env var value
          len = strlen(buf);
          if (len > 0) buf[len - 1] = 0;
          e_util_env_set("PANTS", buf);
        }
      fclose(f);
    }
}

static char *
_grid_text_get(void *data, Evas_Object *obj EINA_UNUSED,
               const char *part EINA_UNUSED)
{
  Item   *it = data;
  char    buf[PATH_MAX], *s;
  FILE   *f;
  size_t  len;

  snprintf(buf, sizeof(buf), "%s.txt", it->file);
  f = fopen(buf, "r");
  if (f)
    {
      if (fgets(buf, sizeof(buf), f))
        {
          // first line is title/label
          len = strlen(buf);
          if (len > 0) buf[len - 1] = 0;
          s = strdup(buf);
        }
      else s = strdup("");
      fclose(f);
      return s;
    }
  return strdup("");
}

static Evas_Object *
_grid_content_get(void *data, Evas_Object *obj, const char *part)
{
  Evas_Object *o = NULL;
  Item *it = data;

  if (!strcmp(part, "elm.swallow.icon"))
    {
      o = e_icon_add(evas_object_evas_get(obj));
      e_icon_fill_inside_set(o, EINA_TRUE);
      e_icon_file_set(o, it->file);
    }
  return o;
}

static void
_grid_del(void *data, Evas_Object *obj EINA_UNUSED)
{
  Item *it = data;

  free(it);
}

static void
_grid_sel(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
  Item *it = data;

  eina_stringshare_replace(&(_config->pants), it->file);
  e_icon_file_set(it->inst->o_gad, _config->pants);
  _pants_env_set(_config->pants);
  e_config_save_queue();
}

static void
_grid_fill(Instance *inst)
{
  Elm_Gengrid_Item_Class *ic = elm_gengrid_item_class_new();
  Elm_Gengrid_Item       *git, *git_sel = NULL;
  char                    buf[PATH_MAX];
  char                   *file, *s;
  Eina_List              *files;
  Item                   *it;

  ic->item_style       = "default";
  ic->func.text_get    = _grid_text_get;
  ic->func.content_get = _grid_content_get;
  ic->func.del         = _grid_del;

  snprintf(buf, sizeof(buf), "%s/pants-db", e_module_dir_get(_module));
  files = ecore_file_ls(buf);
  EINA_LIST_FREE(files, file)
  {
    if ((file[0] != '.') && (eina_fnmatch("*.txt", file, 0)))
      {
        snprintf(buf, sizeof(buf), "%s/pants-db/%s", e_module_dir_get(_module),
                 file);
        s = strrchr(buf, '.');
        if (s) *s = 0;
        it = malloc(sizeof(Item) + strlen(buf) + 1);
        if (it)
          {
            it->inst = inst;
            strcpy(it->file, buf);
            git = elm_gengrid_item_append(inst->o_ggrid, ic, it, _grid_sel, it);
            if ((_config) && (_config->pants) && (!strcmp(_config->pants, it->file)))
              git_sel = git;
          }
      }
    free(file);
  }
  if (git_sel)
    {
      elm_gengrid_item_selected_set(git_sel, EINA_TRUE);
      elm_gengrid_item_show(git_sel, ELM_GENGRID_ITEM_SCROLLTO_TOP);
    }
  elm_gengrid_item_class_free(ic);
}

static void
_popup_show(Instance *inst)
{
  Evas *evas = e_comp->evas;
  Evas_Object *o, *o_tab;

  if (inst->popup) return;

  evas_event_freeze(evas);

  inst->popup = e_gadcon_popup_new(inst->gcc, 0);

  o_tab = o = elm_table_add(e_comp->elm);
  e_gadcon_popup_content_set(inst->popup, o_tab);
  evas_object_show(o_tab);

  o = evas_object_rectangle_add(evas);
  evas_object_size_hint_align_set(o, 0.5, 0.5);
  evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  evas_object_size_hint_min_set(o, ELM_SCALE_SIZE(170), ELM_SCALE_SIZE(170));
  elm_table_pack(o_tab, o, 0, 0, 1, 1);

  o = elm_gengrid_add(e_comp->elm);
  elm_gengrid_item_size_set(o, ELM_SCALE_SIZE(80), ELM_SCALE_SIZE(80));
  evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
  evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  elm_table_pack(o_tab, o, 0, 0, 1, 1);
  evas_object_show(o);

  inst->o_ggrid = o;

  _grid_fill(inst);

  evas_event_thaw(evas);
  evas_event_thaw_eval(evas);
  e_gadcon_popup_show(inst->popup);
}

static void
_popup_hide(Instance *inst)
{
  E_FREE_FUNC(inst->popup, e_object_del);
}

static E_Gadcon_Client *
_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
  E_Gadcon_Client *gcc;
  Evas_Object     *o;
  Instance        *inst;

  inst = E_NEW(Instance, 1);

  o = e_icon_add(gc->evas);
  e_icon_fill_inside_set(o, EINA_TRUE);
  e_icon_file_set(o, _config->pants);

  gcc       = e_gadcon_client_new(gc, name, id, style, o);
  gcc->data = inst;

  inst->gcc = gcc;
  inst->o_gad = o;

  e_gadcon_client_util_menu_attach(gcc);

  evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                 _button_cb_mouse_down, inst);
  return gcc;
}

static void
_gc_shutdown(E_Gadcon_Client *gcc)
{
  Instance *inst = gcc->data;

  _popup_hide(inst);
  evas_object_del(inst->o_gad);
  free(inst);
}

static void
_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient EINA_UNUSED)
{
  e_gadcon_client_aspect_set(gcc, 16, 16);
  e_gadcon_client_min_size_set(gcc, 16, 16);
}

static const char *
_gc_label(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
  return _("Pants");
}

static Evas_Object *
_gc_icon(const E_Gadcon_Client_Class *client_class EINA_UNUSED, Evas *evas)
{
  Evas_Object *o;
  char buf[PATH_MAX];

  o = edje_object_add(evas);
  snprintf(buf, sizeof(buf), "%s/e-module-pants.edj",
           e_module_dir_get(_module));
  edje_object_file_set(o, buf, "icon");
  return o;
}

static const char *
_gc_id_new(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
  return _gadcon_class.name;
}

static void
_button_cb_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
  Instance *inst = data;
  Evas_Event_Mouse_Down *ev = event_info;

  if (ev->button == 1)
    {
      if (inst->popup) _popup_hide(inst);
      else _popup_show(inst);
    }
}

/* module setup */
E_API E_Module_Api e_modapi = {
  E_MODULE_API_VERSION,
  "Pants"
};

E_API void *
e_modapi_init(E_Module *m)
{
  _module = m;
  _conf_edd = E_CONFIG_DD_NEW("Config", Config);
#undef T
#undef D
#define T Config
#define D _conf_edd
  E_CONFIG_VAL(D, T, pants, STR);

  if (_conf_edd)
    {
      _config = e_config_domain_load("module.pants", _conf_edd);
      if (!_config)
        {
          char buf[PATH_MAX];

          _config = E_NEW(Config, 1);
          if (_config)
            {
              snprintf(buf, sizeof(buf), "%s/pants-db/%s",
                       e_module_dir_get(_module), "000-pants-on.png");
              eina_stringshare_replace(&(_config->pants), buf);
            }
        }
      if (_config) _pants_env_set(_config->pants);
    }

  e_gadcon_provider_register(&_gadcon_class);
  return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
  e_gadcon_provider_unregister(&_gadcon_class);
  if (_config) eina_stringshare_replace(&(_config->pants), NULL);
  free(_config);
  E_CONFIG_DD_FREE(_conf_edd);
  _config = NULL;
  _conf_edd = NULL;
  _module = NULL;
  return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
  e_config_domain_save("module.pants", _conf_edd, _config);
  return 1;
}
