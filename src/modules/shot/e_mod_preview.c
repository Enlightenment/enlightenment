#include "e_mod_main.h"

static Evas_Object *win = NULL;
static Evas_Object *o_bg = NULL, *o_box = NULL, *o_content = NULL;
static Evas_Object *o_event = NULL, *o_img = NULL, *o_hlist = NULL;
static Evas_Object *o_rectdim[MAXZONES] = { NULL };
static Evas_Object *o_radio_all = NULL;
static Evas_Object *o_radio[MAXZONES] = { NULL };
static int          quality = 90;
static int          screen = -1;

static void
_win_save_cb(void *data EINA_UNUSED, void *data2 EINA_UNUSED)
{
   save_dialog_show();
}

static void
_win_share_cb(void *d EINA_UNUSED, void *d2 EINA_UNUSED)
{
   share_confirm();
}

static void
_win_cancel_cb(void *data EINA_UNUSED, void *data2 EINA_UNUSED)
{
   E_FREE_FUNC(win, evas_object_del);
}

static void
_win_delete_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   win = NULL;
}

static void
_screen_change_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Eina_List *l;
   E_Zone *z;

   EINA_LIST_FOREACH(e_comp->zones, l, z)
     {
        if (screen == -1)
          evas_object_color_set(o_rectdim[z->num], 0, 0, 0, 0);
        else if (screen == (int)z->num)
          evas_object_color_set(o_rectdim[z->num], 0, 0, 0, 0);
        else
          evas_object_color_set(o_rectdim[z->num], 0, 0, 0, 200);
     }
}

static void
_rect_down_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Evas_Event_Mouse_Down *ev = event_info;
   Eina_List *l;
   E_Zone *z;

   if (ev->button != 1) return;

   e_widget_radio_toggle_set(o_radio_all, 0);
   EINA_LIST_FOREACH(e_comp->zones, l, z)
     {
        if (obj == o_rectdim[z->num])
          {
             screen = z->num;
             e_widget_radio_toggle_set(o_radio[z->num], 1);
          }
        else
          e_widget_radio_toggle_set(o_radio[z->num], 0);
     }

   EINA_LIST_FOREACH(e_comp->zones, l, z)
     {
        if (screen == -1)
          evas_object_color_set(o_rectdim[z->num], 0, 0, 0, 0);
        else if (screen == (int)z->num)
          evas_object_color_set(o_rectdim[z->num], 0, 0, 0, 0);
        else
          evas_object_color_set(o_rectdim[z->num], 0, 0, 0, 200);
     }
}

void
preview_dialog_show(E_Zone *zone, E_Client *ec, const char *params, void *dst,
                    int sw, int sh)
{
   Evas *evas, *evas2;
   Evas_Object *o, *oa, *op, *ol;
   E_Radio_Group *rg;
   int w, h;
   char smode[128], squal[128], sscreen[128];

   win = elm_win_add(NULL, NULL, ELM_WIN_BASIC);

   evas = evas_object_evas_get(win);
   elm_win_title_set(win, _("Select action to take with screenshot"));
   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_delete_cb, NULL);
   elm_win_center(win, 1, 1);
   ecore_evas_name_class_set(e_win_ee_get(win), "E", "_shot_dialog");

   o_bg = o = elm_layout_add(e_win_evas_win_get(evas));
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_win_resize_object_add(win, o);
   e_theme_edje_object_set(o, "base/theme/dialog", "e/widgets/dialog/main");
   evas_object_show(o);

   o_content = o = e_widget_list_add(evas, 0, 0);
   elm_object_part_content_set(o_bg, "e.swallow.content", o);
   evas_object_show(o);

   w = sw / 4;
   if (w < 220) w = 220;
   h = (w * sh) / sw;

   oa = o = e_widget_aspect_add(evas, w, h);
   op = o = e_widget_preview_add(evas, w, h);

   evas2 = e_widget_preview_evas_get(op);

   o_img = o = evas_object_image_filled_add(evas2);
   evas_object_image_colorspace_set(o, EVAS_COLORSPACE_ARGB8888);
   evas_object_image_alpha_set(o, EINA_FALSE);
   evas_object_image_size_set(o, sw, sh);
   evas_object_image_data_copy_set(o, dst);

   evas_object_image_data_update_add(o, 0, 0, sw, sh);
   e_widget_preview_extern_object_set(op, o);
   evas_object_show(o);

   evas_object_show(op);
   evas_object_show(oa);

   e_widget_aspect_child_set(oa, op);
   e_widget_list_object_append(o_content, oa, 0, 0, 0.5);

   o_hlist = o = e_widget_list_add(evas, 1, 1);

   ol = o = e_widget_framelist_add(evas, _("Quality"), 0);

   rg = e_widget_radio_group_new(&quality);
   o = e_widget_radio_add(evas, _("Perfect"), 100, rg);
   e_widget_framelist_object_append(ol, o);
   o = e_widget_radio_add(evas, _("High"), 90, rg);
   e_widget_framelist_object_append(ol, o);
   o = e_widget_radio_add(evas, _("Medium"), 70, rg);
   e_widget_framelist_object_append(ol, o);
   o = e_widget_radio_add(evas, _("Low"), 50, rg);
   e_widget_framelist_object_append(ol, o);

   e_widget_list_object_append(o_hlist, ol, 1, 0, 0.5);

   if (zone)
     {
        screen = -1;
        if (eina_list_count(e_comp->zones) > 1)
          {
             Eina_List *l;
             E_Zone *z;
             int i;

             ol = o = e_widget_framelist_add(evas, _("Screen"), 0);

             rg = e_widget_radio_group_new(&screen);
             o_radio_all = o = e_widget_radio_add(evas, _("All"), -1, rg);
             evas_object_smart_callback_add(o, "changed",
                                            _screen_change_cb, NULL);
             e_widget_framelist_object_append(ol, o);
             i = 0;
             EINA_LIST_FOREACH(e_comp->zones, l, z)
               {
                  char buf[32];

                  if (z->num >= MAXZONES) continue;
                  snprintf(buf, sizeof(buf), "%i", z->num);
                  o_radio[z->num] = o = e_widget_radio_add(evas, buf, z->num, rg);
                  evas_object_smart_callback_add(o, "changed",
                                                 _screen_change_cb, NULL);
                  e_widget_framelist_object_append(ol, o);

                  o_rectdim[z->num] = o = evas_object_rectangle_add(evas2);
                  evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                                 _rect_down_cb, NULL);
                  evas_object_color_set(o, 0, 0, 0, 0);
                  evas_object_show(o);
                  evas_object_geometry_get(o_img, NULL, NULL, &w, &h);
                  evas_object_move(o, (z->x * w) / sw, (z->y * h) / sh);
                  evas_object_resize(o, (z->w * w) / sw, (z->h * h) / sh);
                  i++;
               }
             e_widget_list_object_append(o_hlist, ol, 1, 0, 0.5);
          }

     }
   e_widget_list_object_append(o_content, o_hlist, 0, 0, 0.5);

   o = o_content;
   e_widget_size_min_get(o, &w, &h);
   evas_object_size_hint_min_set(o, w, h);
   elm_object_part_content_set(o_bg, "e.swallow.content", o);
   evas_object_show(o);

   ///////////////////////////////////////////////////////////////////////

   o_box = o = e_widget_list_add(evas, 1, 1);
   elm_object_part_content_set(o_bg, "e.swallow.buttons", o);

   o = e_widget_button_add(evas, _("Save"), NULL, _win_save_cb, win, NULL);
   e_widget_list_object_append(o_box, o, 1, 0, 0.5);
   o = e_widget_button_add(evas, _("Share"), NULL, _win_share_cb, win, NULL);
   e_widget_list_object_append(o_box, o, 1, 0, 0.5);
   o = e_widget_button_add(evas, _("Cancel"), NULL, _win_cancel_cb, win, NULL);
   e_widget_list_object_append(o_box, o, 1, 0, 0.5);

   o = o_box;
   e_widget_size_min_get(o, &w, &h);
   evas_object_size_hint_min_set(o, w, h);
   elm_object_part_content_set(o_bg, "e.swallow.buttons", o);

   o_event = o = evas_object_rectangle_add(evas);
   evas_object_size_hint_min_get(o_bg, &w, &h);
   evas_object_resize(win, w, h);
   evas_object_size_hint_min_set(win, w, h);
   evas_object_size_hint_max_set(win, 99999, 99999);

   if ((params) &&
       (sscanf(params, "%100s %100s %100s", smode, squal, sscreen) == 3))
     {
        screen = -1;
        if ((zone) && (!strcmp(sscreen, "current"))) screen = zone->num;
        else if (!strcmp(sscreen, "all")) screen = -1;
        else screen = atoi(sscreen);

        quality = 90;
        if (!strcmp(squal, "perfect")) quality = 100;
        else if (!strcmp(squal, "high")) quality = 90;
        else if (!strcmp(squal, "medium")) quality = 70;
        else if (!strcmp(squal, "low")) quality = 50;
        else quality = atoi(squal);

        if (!strcmp(smode, "save")) _win_save_cb(NULL, NULL);
        else if (!strcmp(smode, "share")) _win_share_cb(NULL, NULL);
     }
   else
     {
        evas_object_show(win);
        e_win_client_icon_set(win, "screenshot");
        if (!e_widget_focus_get(o_bg)) e_widget_focus_set(o_box, 1);
        if (ec)
          {
             E_Client *c = e_win_client_get(win);
             if (c) evas_object_layer_set(c->frame, ec->layer);
          }
     }
}

Eina_Bool
preview_have(void)
{
   if (win) return EINA_TRUE;
   else return EINA_FALSE;
}

void
preview_abort(void)
{
   E_FREE_FUNC(win, evas_object_del);
}

Evas_Object *
preview_image_get(void)
{
   return o_img;
}
