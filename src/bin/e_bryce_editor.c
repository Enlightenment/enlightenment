#include "e.h"

#define DEFAULT_AUTOSIZE EINA_TRUE
#define DEFAULT_AUTOHIDE EINA_FALSE
#define DEFAULT_LAYER E_LAYER_CLIENT_EDGE

typedef struct Bryce_Info
{
   E_Gadget_Site_Anchor anchor;
   E_Gadget_Site_Orient orient;
   Eina_Stringshare *style;
   Eina_Bool stack_under;
   Eina_Bool autohide;
   Eina_Bool autosize;
} Bryce_Info;


static void _editor_add_bottom(void *data, Evas_Object *obj, const char *sig, const char *src);
static void _editor_add_top(void *data, Evas_Object *obj, const char *sig, const char *src);
static void _editor_add_left(void *data, Evas_Object *obj, const char *sig, const char *src);
static void _editor_add_right(void *data, Evas_Object *obj, const char *sig, const char *src);

static void
setup_exists(Evas_Object *bryce, Evas_Object *editor, Evas_Object *parent, E_Gadget_Site_Anchor an)
{
   if (e_bryce_exists(parent, bryce, E_GADGET_SITE_ORIENT_HORIZONTAL, E_GADGET_SITE_ANCHOR_BOTTOM | an))
     elm_object_signal_emit(editor, "e,bryce,exists,bottom", "e");
   if (e_bryce_exists(parent, bryce, E_GADGET_SITE_ORIENT_HORIZONTAL, E_GADGET_SITE_ANCHOR_TOP | an))
     elm_object_signal_emit(editor, "e,bryce,exists,top", "e");
   if (e_bryce_exists(parent, bryce, E_GADGET_SITE_ORIENT_VERTICAL, E_GADGET_SITE_ANCHOR_LEFT | an))
     elm_object_signal_emit(editor, "e,bryce,exists,left", "e");
   if (e_bryce_exists(parent, bryce, E_GADGET_SITE_ORIENT_VERTICAL, E_GADGET_SITE_ANCHOR_RIGHT | an))
     elm_object_signal_emit(editor, "e,bryce,exists,right", "e");
}

static void
_editor_bryce_add(Evas_Object *obj)
{
   Evas_Object *b, *site;
   char buf[1024];
   const char *loc = "", *loc2 = "";
   Bryce_Info *bi;
   E_Gadget_Site_Gravity gravity = E_GADGET_SITE_GRAVITY_CENTER;

   bi = evas_object_data_get(obj, "__bryce_info");
   b = evas_object_data_get(obj, "__bryce_editor_bryce");
   if (bi->anchor & E_GADGET_SITE_ANCHOR_TOP)
     loc = "top";
   else if (bi->anchor & E_GADGET_SITE_ANCHOR_BOTTOM)
     loc = "bottom";
   else if (bi->anchor & E_GADGET_SITE_ANCHOR_LEFT)
     loc = "left";
   else if (bi->anchor & E_GADGET_SITE_ANCHOR_RIGHT)
     loc = "right";
   if (bi->anchor & E_GADGET_SITE_ANCHOR_RIGHT)
     loc2 = "right";
   else if (bi->anchor & E_GADGET_SITE_ANCHOR_LEFT)
     loc2 = "left";
   else if (bi->anchor & E_GADGET_SITE_ANCHOR_TOP)
     loc2 = "top";
   else if (bi->anchor & E_GADGET_SITE_ANCHOR_BOTTOM)
     loc2 = "bottom";

   snprintf(buf, sizeof(buf), "bryce_%s_%s", loc, loc2);
   if (bi->orient == E_GADGET_SITE_ORIENT_HORIZONTAL)
     {
        if (bi->anchor & E_GADGET_SITE_ANCHOR_LEFT)
          gravity = E_GADGET_SITE_GRAVITY_LEFT;
        else if (bi->anchor & E_GADGET_SITE_ANCHOR_RIGHT)
          gravity = E_GADGET_SITE_GRAVITY_RIGHT;
     }
   else
     {
        if (bi->anchor & E_GADGET_SITE_ANCHOR_TOP)
          gravity = E_GADGET_SITE_GRAVITY_TOP;
        else if (bi->anchor & E_GADGET_SITE_ANCHOR_BOTTOM)
          gravity = E_GADGET_SITE_GRAVITY_BOTTOM;
     }
   if (b)
     site = e_bryce_site_get(b);
   else
     {
        b = e_bryce_add(e_comp->elm, buf, bi->orient, bi->anchor);
        site = e_bryce_site_get(b);

        e_gadget_site_gadget_add(site, "Start", 0);
        e_gadget_site_gadget_add(site, "Digital Clock", 0);
        e_gadget_site_gadget_add(site, "Wireless", 0);
     }
   e_gadget_site_gravity_set(site, gravity);
   e_bryce_style_set(b, bi->style);
   e_bryce_autohide_set(b, bi->autohide);
   e_bryce_autosize_set(b, bi->autosize);
   evas_object_layer_set(b, bi->stack_under ? E_LAYER_DESKTOP : E_LAYER_CLIENT_EDGE);
   evas_object_del(obj);
}

static void
_editor_finish(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   _editor_bryce_add(data);
}

static void
_editor_autosize(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
  Bryce_Info *bi = data;

  bi->autosize = !bi->autosize;
}

static void
_editor_autohide(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
  Bryce_Info *bi = data;

  bi->autohide = !bi->autohide;
}

static void
_editor_stacking(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
  Bryce_Info *bi = data;

  bi->stack_under = !bi->stack_under;
}

static void
_editor_style_click(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   const char *g;
   char style[1024] = {0};
   Bryce_Info *bi;
   Evas_Object *ly, *box, *ck, *button;

   ly = elm_object_part_content_get(obj, "e.swallow.content");
   elm_layout_file_get(ly, NULL, &g);
   g += (sizeof("e/bryce/") - 1);
   memcpy(style, g, MIN(sizeof(style) - 1, strchr(g, '/') - g));

   bi = evas_object_data_get(data, "__bryce_info");
   bi->style = eina_stringshare_add(style);
   e_theme_edje_object_set(data, NULL, "e/bryce/editor/finish");
   elm_object_part_text_set(data, "e.text", _("Finishing touches... (4/4)"));
   box = elm_box_add(data);
   elm_box_padding_set(box, 0, 20 * e_scale);

   ck = elm_check_add(box);
   E_ALIGN(ck, 0, 0.5);
   evas_object_show(ck);
   elm_object_text_set(ck, _("Automatically size based on contents"));
   evas_object_smart_callback_add(ck, "changed", _editor_autosize, bi);
   elm_box_pack_end(box, ck);

   ck = elm_check_add(box);
   E_ALIGN(ck, 0, 0.5);
   evas_object_show(ck);
   elm_object_text_set(ck, _("Automatically hide"));
   evas_object_smart_callback_add(ck, "changed", _editor_autohide, bi);
   elm_box_pack_end(box, ck);

   ck = elm_check_add(box);
   E_ALIGN(ck, 0, 0.5);
   evas_object_show(ck);
   elm_object_text_set(ck, _("Do not stack above windows"));
   evas_object_smart_callback_add(ck, "changed", _editor_stacking, bi);
   elm_box_pack_end(box, ck);

   //ck = elm_check_add(box);
   //elm_object_text_set(ck, _("Allow windows to overlap"));
   //evas_object_smart_callback_add(ck, "changed", _editor_overlap, data);
   //elm_box_pack_end(box, ck);

   button = elm_button_add(data);
   evas_object_show(button);
   elm_object_text_set(button, _("Finish!"));
   evas_object_smart_callback_add(button, "clicked", _editor_finish, data);
   elm_box_pack_end(box, button);

   elm_object_part_content_set(data, "e.swallow.content", box);
}

static void
_editor_style(Evas_Object *obj)
{
   Eina_List *l;
   Eina_Stringshare *style;
   Evas_Object *box;
   int w;

   evas_object_geometry_get(obj, NULL, NULL, &w, NULL);
   box = elm_box_add(obj);
   e_theme_edje_object_set(obj, NULL, "e/bryce/editor/style");
   elm_object_part_text_set(obj, "e.text", _("Choose style (3/4)"));
   elm_box_homogeneous_set(box, 1);
   elm_box_padding_set(box, 0, 20 * e_scale);
   l = elm_theme_group_base_list(NULL, "e/bryce/");
   EINA_LIST_FREE(l, style)
     {
        Evas_Object *ly, *bryce;
        char buf[1024] = {0};
        size_t len;

        if (!eina_str_has_suffix(style, "/base"))
          {
             eina_stringshare_del(style);
             continue;
          }
        ly = elm_layout_add(box);
        e_theme_edje_object_set(ly, NULL, "e/bryce/editor/style/item");
        bryce = edje_object_add(evas_object_evas_get(box));
        elm_object_part_content_set(ly, "e.swallow.content", bryce);
        len = strlen(style);
        strncpy(buf, style + sizeof("e/bryce/") - 1,
          MIN(sizeof(buf) - 1, len - (sizeof("e/bryce/") - 1) - (sizeof("/base") - 1)));
        buf[0] = toupper(buf[0]);
        elm_object_part_text_set(ly, "e.text", buf);
        e_comp_object_util_del_list_append(ly, bryce);
        e_theme_edje_object_set(bryce, NULL, style);
        evas_object_size_hint_min_set(bryce, w * 2 / 3, 48 * e_scale);
        evas_object_show(ly);
        evas_object_event_callback_add(ly, EVAS_CALLBACK_MOUSE_DOWN, _editor_style_click, obj);
        elm_box_pack_end(box, ly);
     }
   elm_object_part_content_set(obj, "e.swallow.content", box);
}

static void
_editor_info_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Bryce_Info *bi = data;

   eina_stringshare_del(bi->style);
   free(bi);
}

static void
_editor_add(Evas_Object *obj, E_Gadget_Site_Orient orient, E_Gadget_Site_Anchor an)
{
   char buf[1024];
   Bryce_Info *bi;

   bi = evas_object_data_get(obj, "__bryce_info");
   if (bi)
     {
        bi->anchor |= an;
        _editor_style(obj);
     }
   else
     {
        bi = E_NEW(Bryce_Info, 1);
        bi->anchor = an;
        bi->orient = orient;
        evas_object_data_set(obj, "__bryce_info", bi);
        evas_object_event_callback_add(obj, EVAS_CALLBACK_DEL, _editor_info_del, bi);
        snprintf(buf, sizeof(buf), "e/bryce/editor/side/%s",
          orient == E_GADGET_SITE_ORIENT_HORIZONTAL ? "horizontal" : "vertical");
        e_theme_edje_object_set(obj, NULL, buf);
        elm_object_part_text_set(obj, "e.text", _("Choose position (2/4)"));
        if (an & E_GADGET_SITE_ANCHOR_BOTTOM)
          elm_object_signal_emit(obj, "e,state,bottom", "e");
        else if (an & E_GADGET_SITE_ANCHOR_RIGHT)
          elm_object_signal_emit(obj, "e,state,right", "e");
        setup_exists(evas_object_data_get(obj, "__bryce_editor_bryce"), obj,
                          evas_object_data_get(obj, "__bryce_editor_site"), an);
     }
}

static void
_editor_add_bottom(void *data EINA_UNUSED, Evas_Object *obj, const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   _editor_add(obj, E_GADGET_SITE_ORIENT_HORIZONTAL, E_GADGET_SITE_ANCHOR_BOTTOM);
}

static void
_editor_add_top(void *data EINA_UNUSED, Evas_Object *obj, const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   _editor_add(obj, E_GADGET_SITE_ORIENT_HORIZONTAL, E_GADGET_SITE_ANCHOR_TOP);
}

static void
_editor_add_left(void *data EINA_UNUSED, Evas_Object *obj, const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   _editor_add(obj, E_GADGET_SITE_ORIENT_VERTICAL, E_GADGET_SITE_ANCHOR_LEFT);
}

static void
_editor_add_center(void *data EINA_UNUSED, Evas_Object *obj, const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   _editor_add(obj, E_GADGET_SITE_ORIENT_NONE, E_GADGET_SITE_ANCHOR_NONE);
}

static void
_editor_add_right(void *data EINA_UNUSED, Evas_Object *obj, const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   _editor_add(obj, E_GADGET_SITE_ORIENT_VERTICAL, E_GADGET_SITE_ANCHOR_RIGHT);
}

static void
_editor_dismiss(void *data EINA_UNUSED, Evas_Object *obj, const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   evas_object_del(obj);
}

E_API Evas_Object *
e_bryce_editor_add(Evas_Object *parent, Evas_Object *bryce)
{
   Evas_Object *editor;

   editor = elm_layout_add(parent);
   evas_object_data_set(editor, "__bryce_editor_site", parent);
   evas_object_data_set(editor, "__bryce_editor_bryce", bryce);
   e_theme_edje_object_set(editor, NULL, "e/bryce/editor/side");
   elm_object_part_text_set(editor, "e.text", _("Choose screen edge (1/4)"));

   setup_exists(bryce, editor, parent, 0);

   elm_object_signal_callback_add(editor, "e,action,dismiss", "e", _editor_dismiss, editor);
   elm_object_signal_callback_add(editor, "e,bryce,add,bottom", "e", _editor_add_bottom, editor);
   elm_object_signal_callback_add(editor, "e,bryce,add,top", "e", _editor_add_top, editor);
   elm_object_signal_callback_add(editor, "e,bryce,add,left", "e", _editor_add_left, editor);
   elm_object_signal_callback_add(editor, "e,bryce,add,right", "e", _editor_add_right, editor);
   elm_object_signal_callback_add(editor, "e,bryce,add,center", "e", _editor_add_center, editor);
   return editor;
}

static Ecore_Event_Handler *handler;

static void
_bryce_edit_key_location(Evas_Object *obj, Ecore_Event_Key *ev)
{
   if (eina_streq(ev->key, "Up"))
     _editor_add(obj, E_GADGET_SITE_ORIENT_HORIZONTAL, E_GADGET_SITE_ANCHOR_TOP);
   else if (eina_streq(ev->key, "Down"))
     _editor_add(obj, E_GADGET_SITE_ORIENT_HORIZONTAL, E_GADGET_SITE_ANCHOR_BOTTOM);
   else if (eina_streq(ev->key, "Left"))
     _editor_add(obj, E_GADGET_SITE_ORIENT_VERTICAL, E_GADGET_SITE_ANCHOR_LEFT);
   else if (eina_streq(ev->key, "Right"))
     _editor_add(obj, E_GADGET_SITE_ORIENT_VERTICAL, E_GADGET_SITE_ANCHOR_RIGHT);
}

static Eina_Bool
_bryce_edit_key_handler(void *data, int t EINA_UNUSED, Ecore_Event_Key *ev)
{
   if (eina_streq(ev->key, "Escape"))
     {
        elm_layout_signal_emit(data, "e,action,dismiss", "e");
        return ECORE_CALLBACK_RENEW;
     }
   if (evas_object_data_get(data, "__bryce_info"))
     {
        const char *grp;

        edje_object_file_get(data, NULL, &grp);
        if (!strncmp(grp, "e/bryce/editor/side/", sizeof("e/bryce/editor/side/") - 1))
          _bryce_edit_key_location(data, ev);
        else if (!strncmp(grp, "e/bryce/editor/style", sizeof("e/bryce/editor/style") - 1))
          {
             Evas_Object *bx, *o;
             Eina_List *l;
             int n;

             bx = elm_object_part_content_get(data, "e.swallow.content");
             l = elm_box_children_get(bx);
             n = strtol(ev->key, NULL, 10);
             if (n > 0)
               {
                  o = eina_list_nth(l, n - 1);
                  if (o)
                    _editor_style_click(data, NULL, o, NULL);
               }
             eina_list_free(l);
          }
        else if (eina_streq(ev->key, "Return") || eina_streq(ev->key, "KP_Enter"))
          _editor_bryce_add(data);
     }
   else
     _bryce_edit_key_location(data, ev);
   return ECORE_CALLBACK_RENEW;
}

static void
_bryce_edit_end(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   e_bindings_disabled_set(0);
   e_comp_ungrab_input(1, 1);
   evas_object_hide(data);
   evas_object_del(data);
   E_FREE_FUNC(handler, ecore_event_handler_del);
}

E_API Evas_Object *
e_bryce_edit(Evas_Object *bryce)
{
   Evas_Object *editor, *comp_object;
   E_Zone *zone;
   int x, y, w, h;

   zone = e_zone_current_get();
   x = zone->x, y = zone->y, w = zone->w, h = zone->h;
   e_bindings_disabled_set(1);
   editor = e_bryce_editor_add(e_comp->elm, bryce);

   evas_object_geometry_set(editor, x, y, w, h);
   comp_object = e_comp_object_util_add(editor, E_COMP_OBJECT_TYPE_NONE);
   evas_object_event_callback_add(editor, EVAS_CALLBACK_DEL, _bryce_edit_end, comp_object);
   evas_object_layer_set(comp_object, E_LAYER_POPUP);
   evas_object_show(comp_object);

   e_comp_object_util_autoclose(comp_object, NULL, e_comp_object_util_autoclose_on_escape, NULL);
   e_comp_grab_input(1, 1);
   handler = ecore_event_handler_add(ECORE_EVENT_KEY_UP, (Ecore_Event_Handler_Cb)_bryce_edit_key_handler, editor);
   return comp_object;
}
