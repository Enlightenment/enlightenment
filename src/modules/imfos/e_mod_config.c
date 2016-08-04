#include <e.h>
#include "e_mod_main.h"

static void *_create_data(E_Config_Dialog *cfd);
static void _free_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static void _fill_data(E_Config_Dialog_Data *cfdata);
static Evas_Object *_basic_create(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata);
static int _apply(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);

Imfos_Config *imfos_config = NULL;

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
   return NULL;
}

static void
_free_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata)
{
}

static void
_fill_data(E_Config_Dialog_Data *cfdata)
{
}

static Evas_Object *
_basic_create(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata)
{
   Evas_Object *list, *o;

   list = e_widget_list_add(evas, 0, 0);

   o = e_widget_check_add(evas, "Flip camera", &flipped);

   e_widget_list_object_append(list, o, 1, 0, 0.5);

   return list;
}

static int
_apply(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata)
{
}




