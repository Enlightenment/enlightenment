#include "e.h"

typedef struct _Smart_Data Smart_Data;

struct _Smart_Data
{
  Evas_Object_Smart_Clipped_Data __clipped_data;

  Eina_Rectangle geom;

  int               num, min, max;
  int              *vals;
  Eina_Stringshare *colorspec;

  Evas_Object  *o_smart; // the smart object container itself
  Evas_Object  *o_base;
  Evas_Object  *o_grid;
  Evas_Object **o_vals;

  Eina_Bool reset_vals : 1;
  Eina_Bool reset_colors : 1;
};

static Evas_Smart      *_smart     = NULL;
static Evas_Smart_Class _sc        = EVAS_SMART_CLASS_INIT_NULL;
static Evas_Smart_Class _sc_parent = EVAS_SMART_CLASS_INIT_NULL;

#define ENTRY                                       \
  Smart_Data *sd = evas_object_smart_data_get(obj); \
  if (!sd) return

static void
_clear(Smart_Data *sd)
{
  int i;

  if (sd->o_vals)
    {
      for (i = 0; i < sd->num; i++) evas_object_del(sd->o_vals[i]);
      free(sd->o_vals);
      sd->o_vals = NULL;
    }
  if (sd->vals) free(sd->vals);
  sd->vals = NULL;
  sd->num  = 0;
}

// gui code
static void
_smart_add(Evas_Object *obj)
{
  Smart_Data  *sd;
  Evas        *e;
  Evas_Object *o;
  const char  *theme_edj_file, *grp;

  sd = calloc(1, sizeof(Smart_Data));
  if (!sd) return;
  evas_object_smart_data_set(obj, sd);

  _sc_parent.add(obj);

  e = evas_object_evas_get(obj);

  sd->o_smart = obj;
  sd->o_base = o = edje_object_add(e);
  grp            = "e/fileman/default/graph/base";
  theme_edj_file = elm_theme_group_path_find(NULL, grp);
  edje_object_file_set(o, theme_edj_file, grp);
  evas_object_smart_member_add(o, sd->o_smart); // this is a member
  evas_object_show(o);

  sd->o_grid = o = evas_object_grid_add(e);
  edje_object_part_swallow(sd->o_base, "e.swallow.content", o);
  evas_object_grid_size_set(o, 1, 1);
  evas_object_show(o);
}

static void
_smart_del(Evas_Object *obj)
{
  ENTRY;
  int i;

  if (sd->o_vals)
    {
      for (i = 0; i < sd->num; i++) evas_object_del(sd->o_vals[i]);
      free(sd->o_vals);
      sd->o_vals = NULL;
    }
  if (sd->o_grid)
    {
      evas_object_del(sd->o_grid);
      sd->o_grid = NULL;
    }
  if (sd->o_base)
    {
      evas_object_del(sd->o_base);
      sd->o_base = NULL;
    }
  eina_stringshare_replace(&sd->colorspec, NULL);
  sd->o_smart = NULL;

  _sc_parent.del(obj);
  evas_object_smart_data_set(obj, NULL);
}

static void
_smart_move(Evas_Object *obj, Evas_Coord x, Evas_Coord y)
{
  ENTRY;

  if ((sd->geom.x == x) && (sd->geom.y == y)) return;
  sd->geom.x = x;
  sd->geom.y = y;
  evas_object_smart_changed(obj);
}

static void
_smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
  ENTRY;

  if ((sd->geom.w == w) && (sd->geom.h == h)) return;
  sd->geom.w = w;
  sd->geom.h = h;
  evas_object_smart_changed(obj);
}

static void
_smart_calculate(Evas_Object *obj)
{
  ENTRY;
  int                 i;
  Evas               *e;
  Evas_Object        *o;
  const char         *theme_edj_file, *grp;
  double              v, range;
  Edje_Message_String msg;

  e = evas_object_evas_get(obj);
  evas_object_geometry_set(sd->o_base, sd->geom.x, sd->geom.y, sd->geom.w,
                           sd->geom.h);
  grp            = "e/fileman/default/graph/bar";
  theme_edj_file = elm_theme_group_path_find(NULL, grp);

  if ((!sd->o_vals) && (sd->num > 0))
    {
      evas_object_grid_size_set(sd->o_grid, sd->num, 1);
      sd->o_vals = malloc(sd->num * sizeof(Evas_Object *));
      if (sd->o_vals)
        {
          sd->reset_vals = EINA_TRUE;
          for (i = 0; i < sd->num; i++)
            {
              sd->o_vals[i] = o = edje_object_add(e);
              edje_object_file_set(o, theme_edj_file, grp);
              evas_object_grid_pack(sd->o_grid, o, i, 0, 1, 1);
              evas_object_show(o);
            }
        }
    }
  if ((sd->o_vals) && ((sd->reset_vals) || (sd->reset_colors)))
    {
      range = sd->max - sd->min;
      if (range <= 1.0) range = 1.0;
      for (i = 0; i < sd->num; i++)
        {
          v = 1.0 - ((double)(sd->vals[i] - sd->min) / range);
          if ((sd->colorspec) && (sd->reset_colors))
            {
              msg.str = (char *)sd->colorspec;
              edje_object_message_send(sd->o_vals[i], EDJE_MESSAGE_STRING, 1,
                                       &msg);
              edje_object_message_signal_process(sd->o_vals[i]);
            }
          edje_object_part_drag_value_set(sd->o_vals[i], "e.dragable.value",
                                          0.0, v);
        }
    }
  sd->reset_vals = EINA_FALSE;
  sd->reset_colors = EINA_FALSE;
}

E_API Evas_Object *
e_graph_add(Evas_Object *parent)
{
  if (!_smart)
    {
      evas_object_smart_clipped_smart_set(&_sc_parent);
      _sc           = _sc_parent;
      _sc.name      = "e_graph";
      _sc.version   = EVAS_SMART_CLASS_VERSION;
      _sc.add       = _smart_add;
      _sc.del       = _smart_del;
      _sc.resize    = _smart_resize;
      _sc.move      = _smart_move;
      _sc.calculate = _smart_calculate;
    };
  if (!_smart) _smart = evas_smart_class_new(&_sc);
  return evas_object_smart_add(evas_object_evas_get(parent), _smart);
}

E_API void
e_graph_values_set(Evas_Object *obj, int num, int *vals, int min, int max)
{
  ENTRY;

  if (sd->num != num)
    {
      _clear(sd);
      sd->reset_colors = EINA_TRUE;
      sd->num = num;
      if (sd->num > 0)
        {
          sd->vals = malloc(num * sizeof(int));
          if (!sd->vals) return;
        }
    }
  sd->reset_vals = EINA_TRUE;
  sd->min        = min;
  sd->max        = max;
  if (sd->num > 0) memcpy(sd->vals, vals, num * sizeof(int));
  evas_object_smart_changed(sd->o_smart);
}

E_API void
e_graph_colorspec_set(Evas_Object *obj, const char *cc)
{
  ENTRY;

  if ((!cc) && (!sd->colorspec)) return;
  if ((cc) && (sd->colorspec) && (!strcmp(cc, sd->colorspec))) return;
  eina_stringshare_replace(&sd->colorspec, cc);
  _clear(sd);
  sd->reset_colors = EINA_TRUE;
}
