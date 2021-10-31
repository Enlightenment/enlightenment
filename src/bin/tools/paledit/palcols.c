#include "main.h"

static void _pal_color_add(Evas_Object *win);
static void _pal_color_del(Evas_Object *win);

static char *
_cb_class_gl_label_get(void *data, Evas_Object *obj EINA_UNUSED, const char *part EINA_UNUSED)
{
   Elm_Palette_Color *col = data;

   if (col->name) return strdup(col->name);
   else return strdup("");
}

static Evas_Object *
_cb_class_gl_content_get(void *data, Evas_Object *obj, const char *part)
{
   Elm_Palette_Color *col = data;
   Evas_Object *o, *tb;
   int r, g, b, a;

   if (!!strcmp(part, "elm.swallow.icon")) return NULL;

   tb = o = elm_table_add(obj);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);

   o = elm_bg_add(obj);
   elm_object_style_set(o, "checks_small");
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(tb, o, 0, 0, 1, 1);
   evas_object_show(o);

   o = evas_object_rectangle_add(evas_object_evas_get(obj));
   r = col->r;
   g = col->g;
   b = col->b;
   a = col->a;
   evas_color_argb_premul(a, &r, &g, &b);
   evas_object_color_set(o, r, g, b, a);
   evas_object_size_hint_min_set(o, ELM_SCALE_SIZE(30), ELM_SCALE_SIZE(15));
   elm_table_pack(tb, o, 0, 0, 1, 1);
   evas_object_show(o);

   return tb;
}

static void
_cb_class_gl_sel(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Evas_Object *win = data;
   Elm_Object_Item *it = event_info;
   Elm_Palette_Color *col = elm_object_item_data_get(it);

   elm_colorselector_color_set(evas_object_data_get(win, "pal_colorsel"),
                               col->r, col->g, col->b, col->a);
   colsel_update(win);
   elm_object_text_set(evas_object_data_get(win, "pal_class_entry"), col->name);
}

static int
_cb_class_insert_cmp(const void *pa, const void *pb)
{
   Elm_Palette_Color *col1 = elm_object_item_data_get(pa);
   Elm_Palette_Color *col2 = elm_object_item_data_get(pb);
   const char *n1, *n2;

   if (!col1) return -1;
   if (!col2) return 1;
   n1 = col1->name;
   if (!n1) n1 = "";
   n2 = col2->name;
   if (!n2) n2 = "";
   return strcmp(n1, n2);
}

static void
_cb_select_click(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evas_Object *win = data;
   Evas_Object *o = palsel_add(win);
   evas_object_show(o);
}

static void
_cb_add_click(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   _pal_color_add(data);
}

static void
_cb_del_click(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   _pal_color_del(data);
}

static void
_pal_color_add(Evas_Object *win)
{
   const char *markup = elm_object_text_get(evas_object_data_get(win, "pal_class_entry"));

   if (markup)
     {
        char *plain = elm_entry_markup_to_utf8(markup);
        if (plain)
          {
             Elm_Genlist_Item_Class *itc = evas_object_data_get(win, "pal_class_itc");

             if (itc)
               {
                  Elm_Palette *pal = evas_object_data_get(win, "pal");
                  Evas_Object *list = evas_object_data_get(win, "pal_class_list");
                  Eina_List *l;
                  Elm_Palette_Color *col;
                  int r, g, b, a;

                  EINA_LIST_FOREACH(pal->colors, l, col)
                    {
                       if (!col->name) continue;
                       if (!strcmp(col->name, plain)) goto exists;
                    }
                  elm_colorselector_color_get(evas_object_data_get(win, "pal_colorsel"), &r, &g, &b, &a);
                  elm_config_palette_color_set(pal, plain, r, g, b, a);
                  EINA_LIST_FOREACH(pal->colors, l, col)
                    {
                       if (!col->name) continue;
                       if (!strcmp(col->name, plain))
                         {
                            Elm_Object_Item *it;

                            it = elm_genlist_item_sorted_insert(list, itc, col,
                                                                NULL, ELM_GENLIST_ITEM_NONE,
                                                                _cb_class_insert_cmp,
                                                                _cb_class_gl_sel, win);
                            elm_genlist_item_show(it, ELM_GENLIST_ITEM_SCROLLTO_MIDDLE);
                            elm_genlist_item_selected_set(it, EINA_TRUE);
                            break;
                         }
                    }
                  palimg_update(evas_object_data_get(win, "pal_image"), pal);
               }
exists:
             free(plain);
          }
     }
   pal_save(win);
}

static void
_pal_color_del(Evas_Object *win)
{
   Elm_Object_Item *it, *it_sel;

   it = elm_genlist_selected_item_get(evas_object_data_get(win, "pal_class_list"));
   if (it)
     {
        Elm_Palette *pal = evas_object_data_get(win, "pal");
        Elm_Palette_Color *col = elm_object_item_data_get(it);
        if (col->name) elm_config_palette_color_unset(pal, col->name);
        it_sel = elm_genlist_item_next_get(it);
        if (!it_sel) it_sel = elm_genlist_item_prev_get(it);
        elm_object_item_del(it);
        if (it_sel)
          {
             elm_genlist_item_show(it_sel, ELM_GENLIST_ITEM_SCROLLTO_MIDDLE);
             elm_genlist_item_selected_set(it_sel, EINA_TRUE);
          }
        palimg_update(evas_object_data_get(win, "pal_image"), pal);
     }
   pal_save(win);
}

void
palcols_fill(Evas_Object *win)
{
   Elm_Object_Item *sel_it = NULL;
   Elm_Palette *pal = evas_object_data_get(win, "pal");
   Elm_Palette_Color *col;
   Evas_Object *list = evas_object_data_get(win, "pal_class_list");
   Eina_List *l;
   Elm_Genlist_Item_Class *itc = evas_object_data_get(win, "pal_class_itc");
   Elm_Object_Item *it;

   elm_genlist_clear(list);

   if (!itc)
     {
        itc = elm_genlist_item_class_new();
        evas_object_data_set(win, "pal_class_itc", itc);

        itc->item_style = "default";
        itc->func.text_get = _cb_class_gl_label_get;
        itc->func.content_get = _cb_class_gl_content_get;
     }

   EINA_LIST_FOREACH(pal->colors, l, col)
     {
        it = elm_genlist_item_sorted_insert(list, itc, col,
                                            NULL, ELM_GENLIST_ITEM_NONE,
                                            _cb_class_insert_cmp,
                                            _cb_class_gl_sel, win);
        if (!sel_it) sel_it = it;
     }
   if (sel_it)
     {
        elm_genlist_item_show(sel_it, ELM_GENLIST_ITEM_SCROLLTO_MIDDLE);
        elm_genlist_item_selected_set(sel_it, EINA_TRUE);
     }
}

static void
_cb_hover_down(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evas_Object *win = data, *o;

   o = evas_object_data_get(win, "pal_class_hover");
   elm_hover_dismiss(o);
}

static void
_hover_list_enable_disable(Evas_Object *win)
{
   Elm_Palette *pal = evas_object_data_get(win, "pal");
   Elm_Palette_Color *col;
   Elm_Widget_Item *it;
   Eina_List *l;

   for (it = elm_genlist_first_item_get(evas_object_data_get(win, "pal_class_list_basic"));
        it; it = elm_genlist_item_next_get(it))
     {
        const char *s = elm_object_item_data_get(it);

        if (s)
          {
             EINA_LIST_FOREACH(pal->colors, l, col)
               {
                  if ((col->name) && (!strcmp(s, col->name)))
                    {
                       elm_object_item_disabled_set(it, EINA_TRUE);
                       break;
                    }
               }
             if (!l) elm_object_item_disabled_set(it, EINA_FALSE);
          }
     }
   for (it = elm_genlist_first_item_get(evas_object_data_get(win, "pal_class_list_extended"));
        it; it = elm_genlist_item_next_get(it))
     {
        const char *s = elm_object_item_data_get(it);

        if (s)
          {
             EINA_LIST_FOREACH(pal->colors, l, col)
               {
                  if ((col->name) && (!strcmp(s, col->name)))
                    {
                       elm_object_item_disabled_set(it, EINA_TRUE);
                       break;
                    }
               }
             if (!l) elm_object_item_disabled_set(it, EINA_FALSE);
          }
     }
}

static void
_cb_hover_basic(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evas_Object *win = data;

   evas_object_show(evas_object_data_get(win, "pal_class_list_basic"));
   evas_object_hide(evas_object_data_get(win, "pal_class_list_extended"));
   evas_object_hide(evas_object_data_get(win, "pal_class_label"));
}

static void
_cb_hover_extended(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evas_Object *win = data;

   evas_object_hide(evas_object_data_get(win, "pal_class_list_basic"));
   evas_object_show(evas_object_data_get(win, "pal_class_list_extended"));
   evas_object_hide(evas_object_data_get(win, "pal_class_label"));
}

static void
_cb_hover_advanced(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evas_Object *win = data;

   evas_object_hide(evas_object_data_get(win, "pal_class_list_basic"));
   evas_object_hide(evas_object_data_get(win, "pal_class_list_extended"));
   evas_object_show(evas_object_data_get(win, "pal_class_label"));
}

static int
_cb_class_insert_cc_cmp(const void *pa, const void *pb)
{
   const char *cc1 = elm_object_item_data_get(pa);
   const char *cc2 = elm_object_item_data_get(pb);
   return strcmp(cc1, cc2);
}

static char *
_cb_class_gl_class_label_get(void *data, Evas_Object *obj EINA_UNUSED, const char *part EINA_UNUSED)
{
   return strdup(data);
}

static void
_cb_class_gl_cc_sel(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Evas_Object *win = data;
   Elm_Object_Item *it = event_info;
   const char *cc = elm_object_item_data_get(it);

   elm_object_text_set(evas_object_data_get(win, "pal_class_entry"), cc);
}

static void
_pal_hover_add(Evas_Object *win, Evas_Object *button)
{
   Evas_Object *o, *btn, *hv, *tb, *tlb, *bxh;
   Elm_Widget_Item *it;
   Eina_List *ccs, *l;
   Elm_Genlist_Item_Class *itc;
   const char *s;

   ccs = elm_theme_color_class_list(NULL);

   hv = o = elm_hover_add(win);
   elm_object_style_set(o, "popout");
   elm_hover_parent_set(o, win);
   elm_hover_target_set(o, button);
   evas_object_data_set(win, "pal_class_hover", o);

   btn = o = elm_button_add(win);
   evas_object_size_hint_weight_set(o, 1.0, 0.0);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_callback_add(o, "clicked", _cb_hover_down, win);
   elm_object_text_set(o, "Change entries");
   elm_object_part_content_set(hv, "middle", o);
   evas_object_show(o);

   o = elm_icon_add(win);
   elm_icon_standard_set(o, "go-down");
   elm_object_content_set(btn, o);
   evas_object_show(o);



   tb = o = elm_table_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_table_padding_set(o, ELM_SCALE_SIZE(5), ELM_SCALE_SIZE(5));
   elm_object_part_content_set(hv, "top", o);
   evas_object_show(o);

   o = evas_object_rectangle_add(evas_object_evas_get(win));
   evas_object_size_hint_min_set(o, ELM_SCALE_SIZE(240), ELM_SCALE_SIZE(300));
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_table_pack(tb, o, 0, 0, 1, 3);

   tlb = o = elm_toolbar_add(win);
   elm_toolbar_homogeneous_set(o, EINA_TRUE);
   it = elm_toolbar_item_append(tlb, NULL, "Basic", _cb_hover_basic, win);
   elm_toolbar_item_selected_set(it, EINA_TRUE);
   elm_object_item_tooltip_text_set(it, "The basic set of colors you can modify");
   it = elm_toolbar_item_append(tlb, NULL, "Extended", _cb_hover_extended, win);
   elm_object_item_tooltip_text_set(it, "An extended set of colors that will override the basic colors<br>"
                                    "allowing very specific elements and states to have special colors set");
   it = elm_toolbar_item_append(tlb, NULL, "Advanced", _cb_hover_advanced, win);
   elm_object_item_tooltip_text_set(it, "If you know what you are doing and know precisely which color class<br>"
                                    "names are used in themes, then you can type one in here by hand");
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   elm_table_pack(tb, o, 0, 0, 1, 1);
   evas_object_show(o);

   itc = elm_genlist_item_class_new();
   itc->item_style = "default";
   itc->func.text_get = _cb_class_gl_class_label_get;

   // Basic tab

   o = elm_genlist_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(tb, o, 0, 1, 1, 1);
   evas_object_show(o);
   evas_object_data_set(win, "pal_class_list_basic", o);

   EINA_LIST_FOREACH(ccs, l, s)
     {
        if (s[0] == ':')
          {
             elm_genlist_item_sorted_insert(o, itc, s,
                                            NULL, ELM_GENLIST_ITEM_NONE,
                                            _cb_class_insert_cc_cmp,
                                            _cb_class_gl_cc_sel, win);
          }
     }

   // Extended tab

   o = elm_genlist_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(tb, o, 0, 1, 1, 1);
   evas_object_data_set(win, "pal_class_list_extended", o);

   EINA_LIST_FOREACH(ccs, l, s)
     {
        if (s[0] == '/')
          {
             elm_genlist_item_sorted_insert(o, itc, s,
                                            NULL, ELM_GENLIST_ITEM_NONE,
                                            _cb_class_insert_cc_cmp,
                                            _cb_class_gl_cc_sel, win);
          }
     }

   // Advanced tab

   o = elm_label_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set
   (o, "Adding a custom entry below requires<br>"
       "knowing which color classes are used by<br>"
       "theme EDJ files and is intended for<br>"
       "advanced usage.<br>"
       "<br>"
       "You may also use this to remove any entry<br>"
       "from the above palette."
   );
   elm_table_pack(tb, o, 0, 1, 1, 1);
   evas_object_data_set(win, "pal_class_label", o);

   // Entry + Add + Del

   bxh = o = elm_box_add(win);
   elm_box_padding_set(o, ELM_SCALE_SIZE(10), 0);
   elm_box_horizontal_set(o, EINA_TRUE);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(tb, o, 0, 2, 1, 1);
   evas_object_show(o);
   evas_object_data_set(win, "pal_class_add_del", o);

   o = elm_entry_add(win);
   elm_entry_single_line_set(o, EINA_TRUE);
   elm_entry_scrollable_set(o, EINA_TRUE);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(bxh, o);
   evas_object_show(o);
   evas_object_data_set(win, "pal_class_entry", o);
   elm_object_tooltip_text_set(o, "Name of color entry to add or delete from the palette");

   btn = o = elm_button_add(win);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_callback_add(o, "clicked", _cb_add_click, win);
   elm_box_pack_end(bxh, o);
   evas_object_show(o);
   elm_object_tooltip_text_set(o, "Add this new color entry to the palette");

   o = elm_icon_add(win);
   elm_icon_standard_set(o, "add");
   elm_object_content_set(btn, o);
   evas_object_show(o);

   btn = o = elm_button_add(win);
   evas_object_size_hint_weight_set(o, 0.0, 0.0);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_callback_add(o, "clicked", _cb_del_click, win);
   elm_box_pack_end(bxh, o);
   evas_object_show(o);
   elm_object_tooltip_text_set(o, "Delete this color entry from the palette");

   o = elm_icon_add(win);
   elm_icon_standard_set(o, "sub");
   elm_object_content_set(btn, o);
   evas_object_show(o);

   evas_object_data_set(win, "class_cc_list", ccs);
//   elm_theme_color_class_list_free(ccs);
}

static void
_cb_modify_click(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evas_Object *win = data, *o;

   _hover_list_enable_disable(win);
   o = evas_object_data_get(win, "pal_class_hover");
   evas_object_show(o);
}

Evas_Object *
palcols_add(Evas_Object *win)
{
   Evas_Object *o, *bxl, *btn, *bxh;

   bxl = o = elm_box_add(win);
   elm_box_padding_set(o, 0, ELM_SCALE_SIZE(10));
   elm_box_align_set(o, 0.5, 0.0);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(o);

   btn = o = elm_button_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_callback_add(o, "clicked", _cb_select_click, win);
   elm_object_text_set(o, "Select palette");
   elm_box_pack_end(bxl, o);
   evas_object_show(o);
   elm_object_tooltip_text_set(o, "Select another palette to use from abvailable palettes");

   o = evas_object_rectangle_add(evas_object_evas_get(win));
   evas_object_size_hint_min_set(o, ELM_SCALE_SIZE(240), 0);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   elm_box_pack_end(bxl, o);

   bxh = o = elm_box_add(win);
   elm_box_padding_set(o, ELM_SCALE_SIZE(10), 0);
   elm_box_horizontal_set(o, EINA_TRUE);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(bxl, o);
   evas_object_show(o);

   o = elm_entry_add(win);
   elm_entry_single_line_set(o, EINA_TRUE);
   elm_entry_scrollable_set(o, EINA_TRUE);
   elm_entry_editable_set(o, EINA_FALSE);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(bxh, o);
   evas_object_show(o);
   evas_object_data_set(win, "pal_name", o);
   elm_object_tooltip_text_set(o, "Name of the selected palette");

   o = palimg_add(win);
   elm_box_pack_end(bxh, o);
   evas_object_show(o);
   evas_object_data_set(win, "pal_image", o);

   o = elm_genlist_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(bxl, o);
   evas_object_show(o);
   evas_object_data_set(win, "pal_class_list", o);
   elm_object_tooltip_text_set(o, "Colors listed in this palette");

   btn = o = elm_button_add(win);
   evas_object_size_hint_weight_set(o, 1.0, 0.0);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_fill_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_callback_add(o, "clicked", _cb_modify_click, win);
   elm_object_text_set(o, "Change entries");
   elm_box_pack_end(bxl, o);
   evas_object_show(o);
   elm_object_tooltip_text_set(o, "Add or remove entries from this palette to fine-tune colors");

   o = elm_icon_add(win);
   elm_icon_standard_set(o, "go-up");
   elm_object_content_set(btn, o);
   evas_object_show(o);

   _pal_hover_add(win, btn);

   return bxl;
}
