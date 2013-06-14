#include "e.h"

typedef struct _E_Menu_Category E_Menu_Category;
struct _E_Menu_Category
{
   void *data;
   Eina_List *callbacks;
};

/* local function prototypes */
static Eina_Bool _e_menu_categories_cb_free(const Eina_Hash *hash __UNUSED__, const void *key __UNUSED__, void *data, void *fdata __UNUSED__);
static void _e_menu_cb_free(E_Menu *m);
static void _e_menu_cb_ee_resize(Ecore_Evas *ee);
static void _e_menu_cb_container_move(void *data, Evas_Object *obj, Evas_Coord x, Evas_Coord y);
static void _e_menu_cb_container_resize(void *data, Evas_Object *obj, Evas_Coord w, Evas_Coord h);
static Eina_Bool _e_menu_cb_mouse_move(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool _e_menu_cb_mouse_up(void *data __UNUSED__, int type __UNUSED__, void *event);

static void _e_menu_item_cb_free(E_Menu_Item *mi);
static void _e_menu_item_cb_move(void *data, Evas_Object *obj, Evas_Coord x, Evas_Coord y);
static void _e_menu_item_cb_resize(void *data, Evas_Object *obj, Evas_Coord w, Evas_Coord h);
static void _e_menu_item_cb_in(void *data, Evas *evas __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__);
static void _e_menu_item_cb_out(void *data, Evas *evas __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__);
static void _e_menu_item_cb_submenu_post(void *data __UNUSED__, E_Menu *m __UNUSED__, E_Menu_Item *mi);

static int _e_menu_auto_place(E_Menu *m, int x, int y, int w, int h);
static void _e_menu_activate_internal(E_Menu *m, E_Zone *zone);
static void _e_menu_realize(E_Menu *m);
static void _e_menu_reposition(E_Menu *m);
static void _e_menu_layout_update(E_Menu *m);
static void _e_menu_unrealize(E_Menu *m);
static void _e_menu_deactivate_above(E_Menu *m);
static int _e_menu_active_call(void);
static void _e_menu_deactivate_all(void);

static void _e_menu_item_realize(E_Menu_Item *mi);
static void _e_menu_item_unrealize(E_Menu_Item *mi);
static Eina_Bool _e_menu_item_realize_call(E_Menu_Item *mi);
static void _e_menu_item_submenu_activate(E_Menu_Item *mi);
static void _e_menu_item_submenu_deactivate(E_Menu_Item *mi);

/* local variables */
static Eina_Hash *_categories = NULL;
static Eina_List *_active_menus = NULL;
static Eina_List *_handlers = NULL;
static E_Menu_Item *_active_item = NULL;
static int _menu_x = 0;
static int _menu_y = 0;
static unsigned int _menu_time = 0;

EINTERN int 
e_menu_init(void)
{
   /* TODO: Handlers */
   _handlers = 
     eina_list_append(_handlers, 
                      ecore_event_handler_add(ECORE_EVENT_MOUSE_MOVE,
                                              _e_menu_cb_mouse_move, NULL));
   _handlers = 
     eina_list_append(_handlers, 
                      ecore_event_handler_add(ECORE_EVENT_MOUSE_BUTTON_UP,
                                              _e_menu_cb_mouse_up, NULL));

   _categories = eina_hash_string_superfast_new(NULL);

   return 1;
}

EINTERN int 
e_menu_shutdown(void)
{
   Ecore_Event_Handler *hdl;

   EINA_LIST_FREE(_handlers, hdl)
     ecore_event_handler_del(hdl);

   if (!wl_fatal)
     {
        E_Menu *m;

        EINA_LIST_FREE(_active_menus, m)
          {
             m->active = EINA_FALSE;
             _e_menu_unrealize(m);
             e_object_unref(E_OBJECT(m));
          }
     }

   if (_categories)
     {
        eina_hash_foreach(_categories, _e_menu_categories_cb_free, NULL);
        eina_hash_free(_categories);
     }
   _categories = NULL;

   return 1;
}

EAPI E_Menu *
e_menu_new(void)
{
   E_Menu *m;

   m = E_OBJECT_ALLOC(E_Menu, E_MENU_TYPE, _e_menu_cb_free);
   if (!m) return NULL;

   m->cur.w = 1;
   m->cur.h = 1;
   m->category = NULL;

   return m;
}

EAPI void 
e_menu_activate_key(E_Menu *m, E_Zone *zone, int x, int y, int w, int h, int dir)
{
   E_OBJECT_CHECK(m);
   E_OBJECT_TYPE_CHECK(m, E_MENU_TYPE);
}

EAPI void 
e_menu_activate_mouse(E_Menu *m, E_Zone *zone, int x, int y, int w, int h, int dir, unsigned int activate_time)
{
   E_OBJECT_CHECK(m);
   E_OBJECT_TYPE_CHECK(m, E_MENU_TYPE);
   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   _e_menu_activate_internal(m, zone);

   if (!m->zone)
     {
        e_menu_deactivate(m);
        return;
     }

   switch (dir)
     {
      case E_MENU_POP_DIRECTION_LEFT:
        break;
      case E_MENU_POP_DIRECTION_RIGHT:
        break;
      case E_MENU_POP_DIRECTION_UP:
        break;
      case E_MENU_POP_DIRECTION_DOWN:
        break;
      case E_MENU_POP_DIRECTION_AUTO:
        _e_menu_auto_place(m, x, y, w, h);
        _e_menu_realize(m);
        break;
      default:
        m->cur.x = x + w;
        m->cur.y = y + h;
        break;
     }

   if (!_active_item) return;
   e_menu_item_active_set(_active_item, 0);
}

EAPI void 
e_menu_activate(E_Menu *m, E_Zone *zone, int x, int y, int w, int h, int dir)
{
   E_OBJECT_CHECK(m);
   E_OBJECT_TYPE_CHECK(m, E_MENU_TYPE);
}

EAPI void 
e_menu_deactivate(E_Menu *m)
{
   E_OBJECT_CHECK(m);
   E_OBJECT_TYPE_CHECK(m, E_MENU_TYPE);

   m->cur.visible = EINA_FALSE;
   m->active = EINA_FALSE;

   if (m->post_deactivate_cb.func)
     m->post_deactivate_cb.func(m->post_deactivate_cb.data, m);
}

EAPI int 
e_menu_freeze(E_Menu *m)
{
   E_OBJECT_CHECK(m);
   E_OBJECT_TYPE_CHECK(m, E_MENU_TYPE);

   m->frozen++;

   return m->frozen;
}

EAPI int 
e_menu_thaw(E_Menu *m)
{
   E_OBJECT_CHECK(m);
   E_OBJECT_TYPE_CHECK(m, E_MENU_TYPE);

   m->frozen--;
   if (m->frozen < 0) m->frozen = 0;

   return m->frozen;
}

EAPI void 
e_menu_title_set(E_Menu *m, char *title)
{
   E_OBJECT_CHECK(m);
   E_OBJECT_TYPE_CHECK(m, E_MENU_TYPE);

   if ((m->header.title) && (title) && (!strcmp(m->header.title, title)))
     return;

   eina_stringshare_replace(&m->header.title, title);
   m->changed = EINA_TRUE;
}

EAPI void 
e_menu_icon_file_set(E_Menu *m __UNUSED__, char *icon __UNUSED__)
{
   E_OBJECT_CHECK(m);
   E_OBJECT_TYPE_CHECK(m, E_MENU_TYPE);
}

EAPI void 
e_menu_category_set(E_Menu *m, char *category)
{
   E_OBJECT_CHECK(m);
   E_OBJECT_TYPE_CHECK(m, E_MENU_TYPE);

   if ((m->category) && (category) && (!strcmp(m->category, category)))
     return;

   eina_stringshare_replace(&m->category, category);
   m->changed = EINA_TRUE;
}

EAPI void 
e_menu_category_data_set(char *category, void *data)
{
   E_Menu_Category *cat;

   if ((cat = eina_hash_find(_categories, category)))
     cat->data = data;
   else
     {
        cat = calloc(1, sizeof(E_Menu_Category));
        cat->data = data;
        eina_hash_add(_categories, category, cat);
     }
}

EAPI E_Menu_Category_Callback *
e_menu_category_callback_add(char *category, void (*create) (E_Menu *m, void *category_data, void *data), void (free) (void *data), void *data)
{
   E_Menu_Category *cat;
   E_Menu_Category_Callback *cb = NULL;

   cat = eina_hash_find(_categories, category);
   if (!cat)
     {
        cat = calloc(1, sizeof(E_Menu_Category));
        eina_hash_add(_categories, category, cat);
     }

   if (cat)
     {
        if ((cb = calloc(1, sizeof(E_Menu_Category_Callback))))
          {
             cb->data = data;
             cb->create = create;
             cb->free = free;
             cb->category = eina_stringshare_add(category);
             cat->callbacks = eina_list_append(cat->callbacks, cb);
          }
     }

   return cb;
}

EAPI void 
e_menu_category_callback_del(E_Menu_Category_Callback *cb)
{
   if (cb)
     {
        E_Menu_Category *cat;

        if ((cat = eina_hash_find(_categories, cb->category)))
          cat->callbacks = eina_list_remove(cat->callbacks, cb);

        eina_stringshare_del(cb->category);
        free(cb);
     }
}

EAPI void 
e_menu_pre_activate_callback_set(E_Menu *m,  void (*func) (void *data, E_Menu *m), void *data)
{
   E_OBJECT_CHECK(m);
   E_OBJECT_TYPE_CHECK(m, E_MENU_TYPE);

   m->pre_activate_cb.func = func;
   m->pre_activate_cb.data = data;
}

EAPI void 
e_menu_post_deactivate_callback_set(E_Menu *m,  void (*func) (void *data, E_Menu *m), void *data)
{
   E_OBJECT_CHECK(m);
   E_OBJECT_TYPE_CHECK(m, E_MENU_TYPE);

   m->post_deactivate_cb.func = func;
   m->post_deactivate_cb.data = data;
}

EAPI E_Menu *
e_menu_root_get(E_Menu *m)
{
   E_Menu *ret;

   E_OBJECT_CHECK_RETURN(m, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(m, E_MENU_TYPE, NULL);

   ret = m;
   while ((ret->parent_item) && (ret->parent_item->menu))
     ret = ret->parent_item->menu;

   return ret;
}

EAPI void 
e_menu_idler_before(void)
{
   Eina_List *l;
   E_Menu *m;

   /* EINA_LIST_FOREACH(_active_menus, l, m) */
   /*   { */
   /*      if ((!m->cur.visible) && (m->prev.visible)) */
   /*        { */
   /*           m->prev.visible = m->cur.visible; */
   /*           ecore_evas_hide(m->ee); */
   /*        } */
   /*   } */

   EINA_LIST_FOREACH(_active_menus, l, m)
     {
        if ((m->cur.visible) && (!m->prev.visible))
          {
             if (!m->realized) _e_menu_realize(m);
             m->prev.visible = m->cur.visible;
             /* ecore_evas_raise(m->ee); */
             ecore_evas_show(m->ee);
          }
     }

   EINA_LIST_FOREACH(_active_menus, l, m)
     {
        if (m->realized)
          {
             if ((m->cur.x != m->prev.x) || (m->cur.y != m->prev.y))
               {
                  m->prev.x = m->cur.x;
                  m->prev.y = m->cur.y;
                  ecore_evas_move(m->ee, m->cur.x, m->cur.y);
               }
             if ((m->cur.w != m->prev.w) || (m->cur.h != m->prev.h))
               {
                  m->prev.w = m->cur.w;
                  m->prev.h = m->cur.h;
                  ecore_evas_resize(m->ee, m->cur.w, m->cur.h);
               }
          }
     }

   EINA_LIST_FOREACH(_active_menus, l, m)
     {
        if (!m->active)
          {
             _active_menus = eina_list_remove(_active_menus, m);
             _e_menu_unrealize(m);
             e_object_unref(E_OBJECT(m));
          }
     }
}

EAPI E_Menu_Item *
e_menu_item_new(E_Menu *m)
{
   E_Menu_Item *mi;

   E_OBJECT_CHECK_RETURN(m, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(m, E_MENU_TYPE, NULL);

   mi = E_OBJECT_ALLOC(E_Menu_Item, E_MENU_ITEM_TYPE, _e_menu_item_cb_free);
   if (!mi) return NULL;

   mi->menu = m;
   mi->menu->items = eina_list_append(mi->menu->items, mi);

   return mi;
}

EAPI E_Menu_Item *
e_menu_item_new_relative(E_Menu *m, E_Menu_Item *rel)
{
   E_Menu_Item *mi;

   E_OBJECT_CHECK_RETURN(m, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(m, E_MENU_TYPE, NULL);

   if (rel)
     {
        E_OBJECT_CHECK_RETURN(rel, NULL);
        E_OBJECT_TYPE_CHECK_RETURN(rel, E_MENU_ITEM_TYPE, NULL);

        if (rel->menu != m) return NULL;
     }

   mi = E_OBJECT_ALLOC(E_Menu_Item, E_MENU_ITEM_TYPE, _e_menu_item_cb_free);
   if (!mi) return NULL;

   mi->menu = m;

   if (rel)
     {
        Eina_List *l;

        l = eina_list_data_find_list(m->items, rel);
        m->items = eina_list_append_relative_list(m->items, mi, l);
     }
   else
     m->items = eina_list_prepend(m->items, mi);

   return mi;
}

EAPI E_Menu_Item *
e_menu_item_nth(E_Menu *m, int n)
{
   E_OBJECT_CHECK_RETURN(m, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(m, E_MENU_TYPE, NULL);

   return (E_Menu_Item *)eina_list_nth(m->items, n);
}

EAPI int 
e_menu_item_num_get(const E_Menu_Item *mi)
{
   const Eina_List *l;
   const E_Menu_Item *mi2;
   int i = 0;

   E_OBJECT_CHECK_RETURN(mi, -1);
   E_OBJECT_CHECK_RETURN(mi->menu, -1);
   E_OBJECT_TYPE_CHECK_RETURN(mi, E_MENU_ITEM_TYPE, -1);

   EINA_LIST_FOREACH(mi->menu->items, l, mi2)
     {
        if (mi2 == mi) return i;
        i++;
     }

   return -1;
}

EAPI void 
e_menu_item_icon_file_set(E_Menu_Item *mi, const char *icon)
{
   E_OBJECT_CHECK(mi);
   E_OBJECT_TYPE_CHECK(mi, E_MENU_ITEM_TYPE);

   if ((mi->icon) && (icon) && (!strcmp(mi->icon, icon)))
     return;

   eina_stringshare_replace(&mi->icon, icon);
   if (mi->icon_key) eina_stringshare_del(mi->icon_key);

   if (icon)
     {
        int len = 0;

        len = strlen(icon);
        if ((len > 4) && (!strcasecmp(icon + len - 4, ".edj")))
          mi->icon_key = eina_stringshare_add("icon");
     }

   mi->changed = EINA_TRUE;
   mi->menu->changed = EINA_TRUE;
}

EAPI void 
e_menu_item_icon_edje_set(E_Menu_Item *mi, const char *icon, const char *key)
{
   E_OBJECT_CHECK(mi);
   E_OBJECT_TYPE_CHECK(mi, E_MENU_ITEM_TYPE);

   if (((mi->icon) && (icon) && (!strcmp(mi->icon, icon))) || 
       ((mi->icon_key) && (key) && (!strcmp(mi->icon_key, key))))
     return;

   eina_stringshare_replace(&mi->icon, icon);
   eina_stringshare_replace(&mi->icon_key, key);

   mi->changed = EINA_TRUE;
   mi->menu->changed = EINA_TRUE;
}

EAPI void 
e_menu_item_label_set(E_Menu_Item *mi, const char *label)
{
   E_OBJECT_CHECK(mi);
   E_OBJECT_TYPE_CHECK(mi, E_MENU_ITEM_TYPE);

   if ((mi->label) && (label) && (!strcmp(mi->label, label)))
     return;

   eina_stringshare_replace(&mi->label, label);

   mi->changed = EINA_TRUE;
   mi->menu->changed = EINA_TRUE;
}

EAPI void 
e_menu_item_submenu_set(E_Menu_Item *mi, E_Menu *sub)
{
   E_OBJECT_CHECK(mi);
   E_OBJECT_TYPE_CHECK(mi, E_MENU_ITEM_TYPE);

   if (mi->submenu) e_object_unref(E_OBJECT(mi->submenu));
   if (sub) e_object_ref(E_OBJECT(sub));

   mi->submenu = sub;
   mi->changed = EINA_TRUE;
   mi->menu->changed = EINA_TRUE;
}

EAPI void 
e_menu_item_separator_set(E_Menu_Item *mi, int sep)
{
   E_OBJECT_CHECK(mi);
   E_OBJECT_TYPE_CHECK(mi, E_MENU_ITEM_TYPE);

   if (mi->separator == sep) return;

   mi->separator = sep;
   mi->changed = EINA_TRUE;
   mi->menu->changed = EINA_TRUE;
}

EAPI void 
e_menu_item_check_set(E_Menu_Item *mi, int chk)
{
   E_OBJECT_CHECK(mi);
   E_OBJECT_TYPE_CHECK(mi, E_MENU_ITEM_TYPE);

   if (mi->check == chk) return;

   mi->check = chk;
   mi->changed = EINA_TRUE;
   mi->menu->changed = EINA_TRUE;
}

EAPI void 
e_menu_item_radio_set(E_Menu_Item *mi, int rad)
{
   E_OBJECT_CHECK(mi);
   E_OBJECT_TYPE_CHECK(mi, E_MENU_ITEM_TYPE);

   if (mi->radio == rad) return;

   mi->radio = rad;
   mi->changed = EINA_TRUE;
   mi->menu->changed = EINA_TRUE;
}

EAPI void 
e_menu_item_radio_group_set(E_Menu_Item *mi, int radg)
{
   E_OBJECT_CHECK(mi);
   E_OBJECT_TYPE_CHECK(mi, E_MENU_ITEM_TYPE);

   if (mi->radio_group == radg) return;

   mi->radio_group = radg;
   mi->changed = EINA_TRUE;
   mi->menu->changed = EINA_TRUE;
}

EAPI void 
e_menu_item_toggle_set(E_Menu_Item *mi, int tog)
{
   E_OBJECT_CHECK(mi);
   E_OBJECT_TYPE_CHECK(mi, E_MENU_ITEM_TYPE);

   if (mi->separator) return;
   if (mi->toggle == tog) return;
   mi->toggle = tog;
   if (tog)
     {
        if (mi->o_bg) 
          edje_object_signal_emit(mi->o_bg, "e,state,on", "e");
        if (mi->o_icon_bg)
          edje_object_signal_emit(mi->o_icon_bg, "e,state,on", "e");
        if (mi->o_lbl)
          edje_object_signal_emit(mi->o_lbl, "e,state,on", "e");
        if (mi->o_submenu)
          edje_object_signal_emit(mi->o_submenu, "e,state,on", "e");
        if (mi->o_toggle)
          edje_object_signal_emit(mi->o_toggle, "e,state,on", "e");
        if (mi->menu->o_bg)
          edje_object_signal_emit(mi->menu->o_bg, "e,state,on", "e");

        if (mi->radio)
          {
             const Eina_List *l;
             E_Menu_Item *mi2;

             EINA_LIST_FOREACH(mi->menu->items, l, mi2)
               {
                  if ((mi2 != mi) && (mi2->radio) && 
                      (mi2->radio_group == mi->radio_group))
                    e_menu_item_toggle_set(mi2, 0);
               }
          }
     }
   else
     {
        if (mi->o_bg) 
          edje_object_signal_emit(mi->o_bg, "e,state,off", "e");
        if (mi->o_icon_bg)
          edje_object_signal_emit(mi->o_icon_bg, "e,state,off", "e");
        if (mi->o_lbl)
          edje_object_signal_emit(mi->o_lbl, "e,state,off", "e");
        if (mi->o_submenu)
          edje_object_signal_emit(mi->o_submenu, "e,state,off", "e");
        if (mi->o_toggle)
          edje_object_signal_emit(mi->o_toggle, "e,state,off", "e");
        if (mi->menu->o_bg)
          edje_object_signal_emit(mi->menu->o_bg, "e,state,off", "e");
     }
}

EAPI int 
e_menu_item_toggle_get(E_Menu_Item *mi)
{
   E_OBJECT_CHECK_RETURN(mi, 0);
   E_OBJECT_TYPE_CHECK_RETURN(mi, E_MENU_ITEM_TYPE, 0);

   return mi->toggle;
}

EAPI void 
e_menu_item_callback_set(E_Menu_Item *mi,  E_Menu_Cb func, void *data)
{
   E_OBJECT_CHECK(mi);
   E_OBJECT_TYPE_CHECK(mi, E_MENU_ITEM_TYPE);

   mi->cb.func = func;
   mi->cb.data = data;
}

EAPI void 
e_menu_item_realize_callback_set(E_Menu_Item *mi,  E_Menu_Cb func, void *data)
{
   E_OBJECT_CHECK(mi);
   E_OBJECT_TYPE_CHECK(mi, E_MENU_ITEM_TYPE);

   mi->realize_cb.func = func;
   mi->realize_cb.data = data;
}

EAPI void 
e_menu_item_submenu_pre_callback_set(E_Menu_Item *mi,  E_Menu_Cb func, void *data)
{
   E_OBJECT_CHECK(mi);
   E_OBJECT_TYPE_CHECK(mi, E_MENU_ITEM_TYPE);

   mi->submenu_pre_cb.func = func;
   mi->submenu_pre_cb.data = data;

   if (!mi->submenu_post_cb.func)
     mi->submenu_post_cb.func = _e_menu_item_cb_submenu_post;
}

EAPI void 
e_menu_item_submenu_post_callback_set(E_Menu_Item *mi,  E_Menu_Cb func, void *data)
{
   E_OBJECT_CHECK(mi);
   E_OBJECT_TYPE_CHECK(mi, E_MENU_ITEM_TYPE);

   mi->submenu_post_cb.func = func;
   mi->submenu_post_cb.data = data;
}

EAPI void 
e_menu_item_drag_callback_set(E_Menu_Item *mi __UNUSED__,  E_Menu_Cb func __UNUSED__, void *data __UNUSED__)
{
   E_OBJECT_CHECK(mi);
   E_OBJECT_TYPE_CHECK(mi, E_MENU_ITEM_TYPE);
}

EAPI void 
e_menu_item_active_set(E_Menu_Item *mi, int active)
{
   E_OBJECT_CHECK(mi);
   E_OBJECT_TYPE_CHECK(mi, E_MENU_ITEM_TYPE);

   if (mi->separator) return;
   if ((active) && (!mi->active))
     {
        E_Menu_Item *pmi;

        if (mi->disabled) return;
        pmi = _active_item;
        if (mi == pmi) return;
        if (pmi) e_menu_item_active_set(pmi, 0);
        mi->active = EINA_TRUE;
        _active_item = mi;
        if (mi->o_bg)
          edje_object_signal_emit(mi->o_bg, "e,state,selected", "e");
        if (mi->o_icon_bg)
          edje_object_signal_emit(mi->o_icon_bg, "e,state,selected", "e");
        if (mi->o_lbl)
          edje_object_signal_emit(mi->o_lbl, "e,state,selected", "e");
        if (mi->o_submenu)
          edje_object_signal_emit(mi->o_submenu, "e,state,selected", "e");
        if (mi->o_toggle)
          edje_object_signal_emit(mi->o_toggle, "e,state,selected", "e");
        if (mi->icon_key)
          {
             if (mi->o_icon)
               {
                  if (strcmp(evas_object_type_get(mi->o_icon), "e_icon"))
                    edje_object_signal_emit(mi->o_icon, "e,state,selected", "e");
                  else
                    e_icon_selected_set(mi->o_icon, EINA_TRUE);
               }
          }
        edje_object_signal_emit(mi->menu->o_bg, "e,state,selected", "e");
	_e_menu_item_submenu_activate(mi);
     }
   else if ((!active) && (mi->active))
     {
        mi->active = EINA_FALSE;
        _active_item = NULL;
        if (mi->o_bg)
          edje_object_signal_emit(mi->o_bg, "e,state,unselected", "e");
        if (mi->o_icon_bg)
          edje_object_signal_emit(mi->o_icon_bg, "e,state,unselected", "e");
        if (mi->o_lbl)
          edje_object_signal_emit(mi->o_lbl, "e,state,unselected", "e");
        if (mi->o_submenu)
          edje_object_signal_emit(mi->o_submenu, "e,state,unselected", "e");
        if (mi->o_toggle)
          edje_object_signal_emit(mi->o_toggle, "e,state,unselected", "e");
        if (mi->icon_key)
          {
             if (mi->o_icon)
               {
                  if (strcmp(evas_object_type_get(mi->o_icon), "e_icon"))
                    edje_object_signal_emit(mi->o_icon, "e,state,unselected", "e");
                  else
                    e_icon_selected_set(mi->o_icon, EINA_FALSE);
               }
          }
        edje_object_signal_emit(mi->menu->o_bg, "e,state,unselected", "e");
	_e_menu_item_submenu_deactivate(mi);
     }
}

EAPI void 
e_menu_item_disabled_set(E_Menu_Item *mi, int disable)
{
   E_OBJECT_CHECK(mi);
   E_OBJECT_TYPE_CHECK(mi, E_MENU_ITEM_TYPE);

   if (mi->separator) return;
   if (disable)
     {
        if (mi->active) e_menu_item_active_set(mi, 0);
        mi->disabled = EINA_TRUE;
        if (mi->o_icon_bg) 
          edje_object_signal_emit(mi->o_icon_bg, "e,state,disable", "e");
        if (mi->o_lbl)
          edje_object_signal_emit(mi->o_lbl, "e,state,disable", "e");
        if (mi->o_toggle)
          edje_object_signal_emit(mi->o_toggle, "e,state,disable", "e");
     }
   else
     {
        mi->disabled = EINA_FALSE;
        if (mi->o_icon_bg) 
          edje_object_signal_emit(mi->o_icon_bg, "e,state,enable", "e");
        if (mi->o_lbl)
          edje_object_signal_emit(mi->o_lbl, "e,state,enable", "e");
        if (mi->o_toggle)
          edje_object_signal_emit(mi->o_toggle, "e,state,enable", "e");
     }
}

/* local functions */
static Eina_Bool 
_e_menu_categories_cb_free(const Eina_Hash *hash __UNUSED__, const void *key __UNUSED__, void *data, void *fdata __UNUSED__)
{
   E_Menu_Category_Callback *cb;
   E_Menu_Category *cat;

   cat = (E_Menu_Category *)data;

   EINA_LIST_FREE(cat->callbacks, cb)
     free(cb);

   free(cat);

   return EINA_TRUE;
}

static void 
_e_menu_cb_free(E_Menu *m)
{
   Eina_List *l, *ll;
   E_Menu_Item *mi;

   if (m->category) 
     {
        E_Menu_Category *cat = NULL;

        if ((cat = eina_hash_find(_categories, m->category)))
          {
             E_Menu_Category_Callback *cb;

             EINA_LIST_FOREACH(cat->callbacks, l, cb)
               if (cb->free) cb->free(cb->data);
          }
     }

   _e_menu_unrealize(m);

   EINA_LIST_FOREACH_SAFE(m->items, l, ll, mi)
     e_object_del(E_OBJECT(mi));

   _active_menus = eina_list_remove(_active_menus, m);
   e_object_unref(E_OBJECT(m));

   if (m->header.title) eina_stringshare_del(m->header.title);
   if (m->header.icon_file) eina_stringshare_del(m->header.icon_file);

   E_FREE(m);
}

static void 
_e_menu_cb_ee_resize(Ecore_Evas *ee)
{
   Evas *evas;
   Evas_Object *o;
   Evas_Coord w, h;

   evas = ecore_evas_get(ee);
   evas_output_viewport_get(evas, NULL, NULL, &w, &h);
   o = evas_object_name_find(evas, "menu/background");
   evas_object_resize(o, w, h);
}

static void 
_e_menu_cb_container_move(void *data, Evas_Object *obj, Evas_Coord x, Evas_Coord y)
{
   E_Menu *m;

   if (!(m = data)) return;
   m->cx = x;
   m->cy = y;
   if (m->parent_item) _e_menu_reposition(m);
   evas_object_move(obj, x, y);
}

static void 
_e_menu_cb_container_resize(void *data, Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   E_Menu *m;

   if (!(m = data)) return;
   m->cw = w;
   m->ch = h;
   if (m->parent_item) _e_menu_reposition(m);
   evas_object_resize(obj, w, h);
}

static Eina_Bool 
_e_menu_cb_mouse_move(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Event_Mouse_Move *ev;
   Eina_List *l;
   E_Menu *m;
   int dx, dy, d;
   double dt, fast_move_threshold;
   Eina_Bool is_fast = EINA_FALSE;
   Eina_Bool event_sent = EINA_FALSE;

   ev = event;

   fast_move_threshold = e_config->menus_fast_mouse_move_threshhold;
   dx = ev->x - _menu_x;
   dy = ev->y - _menu_y;
   d = (dx * dy) + (dy * dy);
   dt = (double)(ev->timestamp - _menu_time) / 1000.0;
   dt = dt * dt;
   if ((dt > 0.0) && ((d / dt) >= (fast_move_threshold * fast_move_threshold)))
     is_fast = EINA_TRUE;

   EINA_LIST_FOREACH(_active_menus, l, m)
     {
        if ((m->realized) && (m->cur.visible))
          {
             if (ev->event_window == m->id)
               {
                  int x = 0, y = 0;

                  if (is_fast)
                    m->fast_mouse = EINA_TRUE;
                  else if (dt > 0.0)
                    {
                       m->fast_mouse = EINA_FALSE;
                       if (m->pending_new_submenu)
                         {
                            if (_active_item)
                              _e_menu_item_submenu_deactivate(_active_item);
                         }
                    }

                  x = ev->x - m->cur.x + m->zone->x;
                  y = ev->y - m->cur.y + m->zone->y;

                  if (ev->x < m->cur.x)
                    x = m->cur.x - ev->x + m->zone->x;

                  printf("Menu Position: %d %d\n", m->cur.x, m->cur.y);
                  printf("Feeding Mouse Move On Menu: %d: %d %d\n", 
                         m->id, x, y);
                  evas_event_feed_mouse_move(m->evas, x, y, ev->timestamp, NULL);
                  event_sent = EINA_TRUE;
               }
          }
     }

   _menu_x = ev->x;
   _menu_y = ev->y;
   _menu_time = ev->timestamp;

   if (event_sent) return ECORE_CALLBACK_DONE;
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool 
_e_menu_cb_mouse_up(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Event_Mouse_Button *ev;
   int ret = 0;
   unsigned int t;

   ev = event;
   t = ev->timestamp - _menu_time;
   if ((_menu_time != 0) && 
       (t < (e_config->menus_click_drag_timeout * 1000)))
     {
        return ECORE_CALLBACK_PASS_ON;
     }

   ret = _e_menu_active_call();
   if ((ret == 1) || (ret == -1))
     _e_menu_deactivate_all();

   return ECORE_CALLBACK_PASS_ON;
}

static void 
_e_menu_item_cb_free(E_Menu_Item *mi)
{
   if (mi == _active_item) _active_item = NULL;
   if (mi->submenu)
     {
        mi->submenu->parent_item = NULL;
        e_object_unref(E_OBJECT(mi->submenu));
     }

   if (mi->menu->realized) _e_menu_item_unrealize(mi);

   mi->menu->items = eina_list_remove(mi->menu->items, mi);

   if (mi->icon) eina_stringshare_del(mi->icon);
   if (mi->icon_key) eina_stringshare_del(mi->icon_key);
   if (mi->label) eina_stringshare_del(mi->label);

   E_FREE(mi);
}

static void 
_e_menu_item_cb_move(void *data, Evas_Object *obj, Evas_Coord x, Evas_Coord y)
{
   E_Menu_Item *mi;

   if (!(mi = data)) return;
   mi->x = x;
   mi->y = y;
   evas_object_move(mi->o_event, x, y);
   evas_object_move(obj, x, y);
   if ((mi->submenu) && (mi->submenu->parent_item))
     _e_menu_reposition(mi->submenu);
}

static void 
_e_menu_item_cb_resize(void *data, Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   E_Menu_Item *mi;

   if (!(mi = data)) return;
   mi->w = w;
   mi->h = h;
   evas_object_resize(mi->o_event, w, h);
   evas_object_resize(obj, w, h);
   if ((mi->submenu) && (mi->submenu->parent_item))
     _e_menu_reposition(mi->submenu);
}

static void 
_e_menu_item_cb_in(void *data, Evas *evas __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   E_Menu_Item *mi;

   if (!(mi = data)) return;
   printf("Menu Item Mouse In\n");
   e_menu_item_active_set(mi, 1);
}

static void 
_e_menu_item_cb_out(void *data, Evas *evas __UNUSED__, Evas_Object *obj __UNUSED__, void *event __UNUSED__)
{
   E_Menu_Item *mi;

   if (!(mi = data)) return;
   printf("Menu Item Mouse Out\n");
   e_menu_item_active_set(mi, 0);
}

static void 
_e_menu_item_cb_submenu_post(void *data __UNUSED__, E_Menu *m __UNUSED__, E_Menu_Item *mi)
{
   E_Menu *sm;

   if (!mi->submenu) return;
   sm = mi->submenu;
   e_menu_item_submenu_set(mi, NULL);
   e_object_del(E_OBJECT(sm));
}

static int 
_e_menu_auto_place(E_Menu *m, int x, int y, int w, int h)
{
   m->cur.x = x;
   m->cur.y = y;
   m->cur.w = w;
   m->cur.h = h;
   return 0;
}

static void 
_e_menu_activate_internal(E_Menu *m, E_Zone *zone)
{
   m->zone = zone;
   m->fast_mouse = EINA_FALSE;
   m->pending_new_submenu = EINA_FALSE;

   if (m->pre_activate_cb.func)
     m->pre_activate_cb.func(m->pre_activate_cb.data, m);

   if (!m->active)
     {
        _active_menus = eina_list_append(_active_menus, m);
        m->active = EINA_TRUE;
        e_object_ref(E_OBJECT(m));
     }

   if (m->category)
     {
        E_Menu_Category *cat = NULL;

        if ((cat = eina_hash_find(_categories, m->category)))
          {
             Eina_List *l;
             E_Menu_Category_Callback *cb;

             EINA_LIST_FOREACH(cat->callbacks, l, cb)
               if (cb->create) cb->create(m, cat->data, cb->data);
          }
     }
   m->cur.visible = EINA_TRUE;
}

static void 
_e_menu_realize(E_Menu *m)
{
   Ecore_Wl_Window *win = NULL;
   unsigned int parent = 0;
   Eina_List *l;
   E_Menu_Item *mi;
   int x, y;

   if (m->realized) return;

   if (!m->zone) return;
   if (!m->zone->container) return;
   if (!m->zone->container->bg_ee) return;

   m->realized = EINA_TRUE;

   x = m->cur.x;
   y = m->cur.y;
   if (m->parent_item)
     {
        E_Menu *pm;

        pm = m->parent_item->menu;
        x = pm->cur.x + pm->cur.w;
        y = pm->cur.y + m->parent_item->y;
        /* if ((win = ecore_evas_wayland_window_get(pm->ee))) */
        /*   parent = win->id; */
     }
   /* else */
   /*   { */
   if ((win = ecore_evas_wayland_window_get(m->zone->container->bg_ee)))
     parent = win->id;
     /* } */

   m->ee = 
     e_canvas_new(parent, x, y, m->cur.w, m->cur.h, 
                  EINA_TRUE, EINA_FALSE, &win);
   e_canvas_add(m->ee);

   m->id = win->id;
   /* m->id = ecore_evas_wayland_window_get(m->ee)->id; */
   printf("Menu Id: %d\n", m->id);

   ecore_evas_name_class_set(m->ee, "E", "_e_menu_window");
   ecore_evas_title_set(m->ee, "E Menu");
   ecore_evas_wayland_type_set(m->ee, ECORE_WL_WINDOW_TYPE_MENU);
   ecore_evas_callback_resize_set(m->ee, _e_menu_cb_ee_resize);

   m->evas = ecore_evas_get(m->ee);

   evas_event_freeze(m->evas);

   m->o_bg = edje_object_add(m->evas);
   evas_object_name_set(m->o_bg, "menu/background");
   evas_object_data_set(m->o_bg, "e_menu", m);
   evas_object_move(m->o_bg, 0, 0);
   evas_object_resize(m->o_bg, m->cur.w, m->cur.h);
   if ((e_theme_edje_object_set(m->o_bg, "base/theme/menus", 
                                "e/widgets/menu/default/background")))
     {
        const char *shaped;

        if ((shaped = edje_object_data_get(m->o_bg, "shaped")))
          if (!strcmp(shaped, "1")) 
            m->shaped = EINA_TRUE;

        ecore_evas_alpha_set(m->ee, m->shaped);
     }
   if (m->header.title)
     {
        edje_object_part_text_set(m->o_bg, "e.text.title", m->header.title);
        edje_object_signal_emit(m->o_bg, "e,action,show,title", "e");
        edje_object_message_signal_process(m->o_bg);
     }
   evas_object_show(m->o_bg);

   m->o_con = e_box_add(m->evas);
   evas_object_intercept_move_callback_add(m->o_con, 
                                           _e_menu_cb_container_move, m);
   evas_object_intercept_resize_callback_add(m->o_con, 
                                             _e_menu_cb_container_resize, m);
   e_box_freeze(m->o_con);
   evas_object_show(m->o_con);
   e_box_homogenous_set(m->o_con, 0);
   edje_object_part_swallow(m->o_bg, "e.swallow.content", m->o_con);

   EINA_LIST_FOREACH(m->items, l, mi)
     _e_menu_item_realize(mi);

   _e_menu_layout_update(m);

   e_box_thaw(m->o_con);

   evas_object_resize(m->o_bg, m->cur.w, m->cur.h);
   evas_event_thaw(m->evas);
}

static void 
_e_menu_reposition(E_Menu *m)
{
   Eina_List *l;
   E_Menu_Item *mi;
   int bottom = 0;

   if (!m->parent_item) return;
//   if ((!m->parent_item) || (!m->parent_item->menu)) return;
   m->cur.x = m->parent_item->menu->cur.x + m->parent_item->menu->cur.w;
   bottom = m->parent_item->menu->cur.y + m->parent_item->y;
   if (m->cur.h > m->zone->h)
     {
        if (bottom > (m->zone->h / 2))
          m->cur.y = (bottom - (m->ch + 1));
        else
          m->cur.y = bottom - m->cy;
     }
   else
     {
        if (((bottom + m->cur.h - m->cy) > m->zone->h) && 
            (bottom > (m->zone->h / 2)))
          m->cur.y = (bottom - (m->ch + 1)) + m->parent_item->h;
        else
          m->cur.y = bottom - m->cy;
     }

   printf("Menu Reposition: %d: %d %d\n", m->id, m->cur.x, m->cur.y);

   EINA_LIST_FOREACH(m->items, l, mi)
     if ((mi->active) && (mi->submenu))
       _e_menu_reposition(mi->submenu);
}

static void 
_e_menu_layout_update(E_Menu *m)
{
   Eina_List *l;
   E_Menu_Item *mi;
   Eina_Bool icons = EINA_FALSE;
   Eina_Bool labels = EINA_FALSE;
   Eina_Bool submenus = EINA_FALSE;
   Eina_Bool toggles = EINA_FALSE;
   Evas_Coord mw = 0, mh = 0, bw = 0, bh = 0;
   int miw = 0, mih = 0;
   int mlw = 0, mlh = 0;
   int msw = 0, msh = 0;
   int mtw = 0, mth = 0;

   e_box_freeze(m->o_con);
   EINA_LIST_FOREACH(m->items, l, mi)
     {
        if ((mi->icon) || (mi->o_icon)) icons = EINA_TRUE;
        if (mi->label) labels = EINA_TRUE;
        if (mi->submenu) submenus = EINA_TRUE;
        if ((mi->check) || (mi->radio)) toggles = EINA_TRUE;

        if (mi->iw > miw) miw = mi->iw;
        if (mi->ih > mih) mih = mi->ih;
        if (mi->lw > mlw) mlw = mi->lw;
        if (mi->lh > mlh) mlh = mi->lh;
        if (mi->subw > msw) msw = mi->subw;
        if (mi->subh > msh) msh = mi->subh;
        if (mi->tw > mtw) mtw = mi->tw;
        if (mi->th > mth) mth = mi->th;
     }

   if (labels)
     {
        if (submenus)
          {
             if (mlh < msh) mlh = msh;
          }
        if (toggles)
          {
             if (mlh < mth) mlh = mth;
          }
        if ((icons) && (mih > 0))
          {
             miw = (miw * mlh) / mih;
             miw = mlh;
          }
        mw = (mlw + miw + msw + mtw);
        mh = mlh;
     }
   else if (icons)
     {
        if (submenus)
          {
             if (mih < msh) mih = msh;
          }
        if (toggles)
          {
             if (mih < mth) mih = mth;
          }
        mw = (miw + mtw + msw);
        mh = mih;
     }
   else if (toggles)
     {
        if (submenus)
          {
             if (mth < msh) mth = msh;
          }
        mw = (mtw + msw);
        mh = mth;
     }

   EINA_LIST_FOREACH(m->items, l, mi)
     {
        if (mi->separator)
          e_box_pack_options_set(mi->o_separator, 1, 1, 1, 0, 0.5, 0.5, 
                                 mi->sepw, mi->seph, -1, mi->seph);
        else
          {
             e_box_freeze(mi->o_con);

             if (toggles)
               e_box_pack_options_set(mi->o_toggle, 1, 1, 0, 1, 0.5, 0.5, 
                                      mtw, mth, -1, -1);
             else
               e_box_pack_options_set(mi->o_toggle, 1, 1, 0, 0, 0.5, 0.5, 
                                      0, 0, 0, 0);

             if (icons)
               {
                  if (mi->o_icon_bg)
                    e_box_pack_options_set(mi->o_icon_bg, 1, 1, 0, 1, 0.5, 0.5,
                                           miw, mih, -1, -1);
                  else
                    e_box_pack_options_set(mi->o_icon, 1, 1, 0, 1, 0.5, 0.5, 
                                           miw, mih, -1, -1);
               }
             else
               e_box_pack_options_set(mi->o_icon, 1, 1, 0, 1, 0.5, 0.5, 
                                      0, 0, 0, 0);

             if (labels)
               e_box_pack_options_set(mi->o_lbl, 1, 1, 0, 1, 0.5, 0.5, 
                                      mlw, mlh, -1, -1);
             else
               e_box_pack_options_set(mi->o_lbl, 1, 1, 0, 0, 0.5, 0.5, 
                                      0, 0, 0, 0);

             if (submenus)
               e_box_pack_options_set(mi->o_submenu, 1, 1, 0, 1, 0.5, 0.5,
                                      msw, msh, -1, -1);
             else
               e_box_pack_options_set(mi->o_submenu, 1, 1, 0, 0, 0.5, 0.5, 
                                      0, 0, 0, 0);

             edje_extern_object_min_size_set(mi->o_con, mw, mh);
             edje_object_part_swallow(mi->o_bg, "e.swallow.content", mi->o_con);
             edje_object_size_min_calc(mi->o_bg, &mw, &mh);
             e_box_pack_options_set(mi->o_bg, 1, 1, 1, 0, 0.5, 0.5, 
                                    mw, mh, -1, -1);

             e_box_thaw(mi->o_con);
          }
     }

   e_box_size_min_get(m->o_con, &bw, &bh);
   edje_extern_object_min_size_set(m->o_con, bw, bh);
   edje_extern_object_max_size_set(m->o_con, bw, bh);
   edje_object_part_swallow(m->o_bg, "e.swallow.content", m->o_con);
   edje_object_size_min_calc(m->o_bg, &mw, &mh);
   e_box_thaw(m->o_con);

   m->cur.w = mw;
   m->cur.h = mh;
}

static void 
_e_menu_unrealize(E_Menu *m)
{
   Eina_List *l;
   E_Menu_Item *mi;

   if (!m->realized) return;

   evas_event_freeze(m->evas);

   e_box_freeze(m->o_con);
   EINA_LIST_FOREACH(m->items, l, mi)
     _e_menu_item_unrealize(mi);
   e_box_thaw(m->o_con);

   if (m->header.icon) evas_object_del(m->header.icon);
   m->header.icon = NULL;
   if (m->o_bg) evas_object_del(m->o_bg);
   m->o_bg = NULL;
   if (m->o_con) evas_object_del(m->o_con);
   m->o_con = NULL;

   evas_event_thaw(m->evas);

   e_canvas_del(m->ee);
   ecore_evas_free(m->ee);

   m->ee = NULL;
   m->evas = NULL;
   m->cur.visible = EINA_FALSE;
   m->prev.visible = EINA_FALSE;
   m->realized = EINA_FALSE;
   m->zone = NULL;
}

static void 
_e_menu_deactivate_above(E_Menu *m)
{
   Eina_List *l;
   E_Menu *pm;
   Eina_Bool above = EINA_FALSE;

   EINA_LIST_FOREACH(_active_menus, l, pm)
     {
        if (above)
          {
             e_menu_deactivate(pm);
             pm->parent_item = NULL;
          }
        if (m == pm) above = EINA_TRUE;
//        e_object_unref(E_OBJECT(pm));
     }
}

static int 
_e_menu_active_call(void)
{
   E_Menu_Item *mi;

   mi = _active_item;
   if (mi)
     {
        if (mi->submenu) return 0;

        if (mi->check)
          e_menu_item_toggle_set(mi, !mi->toggle);
        if ((mi->radio) && (!e_menu_item_toggle_get(mi)))
          e_menu_item_toggle_set(mi, 1);
        if (mi->cb.func)
          mi->cb.func(mi->cb.data, mi->menu, mi);

        return 1;
     }

   return -1;
}

static void 
_e_menu_deactivate_all(void)
{
   Eina_List *l;
   E_Menu *m;

   EINA_LIST_FOREACH(_active_menus, l, m)
     {
        e_menu_deactivate(m);
        m->parent_item = NULL;
     }
}

static void 
_e_menu_item_realize(E_Menu_Item *mi)
{
   Evas *evas;

   evas = mi->menu->evas;
   if (mi->separator)
     {
        mi->o_separator = edje_object_add(evas);
        e_theme_edje_object_set(mi->o_separator, "base/theme/menus", 
                                "e/widgets/menu/default/separator");
        evas_object_show(mi->o_separator);
        edje_object_size_min_calc(mi->o_separator, &mi->sepw, &mi->seph);
        e_box_pack_end(mi->menu->o_con, mi->o_separator);
     }
   else
     {
        mi->o_bg = edje_object_add(evas);
        evas_object_intercept_move_callback_add(mi->o_bg, 
                                                _e_menu_item_cb_move, mi);
        evas_object_intercept_resize_callback_add(mi->o_bg, 
                                                  _e_menu_item_cb_resize, mi);
        if ((mi->submenu) || (mi->submenu_pre_cb.func))
          {
             if (!e_theme_edje_object_set(mi->o_bg, "base/theme/menus", 
                                          "e/widgets/menu/default/submenu_bg"))
               e_theme_edje_object_set(mi->o_bg, "base/theme/menus", 
                                       "e/widgets/menu/default/item_bg");
          }
        else
          e_theme_edje_object_set(mi->o_bg, "base/theme/menus", 
                                  "e/widgets/menu/default/item_bg");
        evas_object_show(mi->o_bg);

        mi->o_con = e_box_add(evas);
        e_box_homogenous_set(mi->o_con, 0);
        e_box_orientation_set(mi->o_con, 1);
        evas_object_show(mi->o_con);

        e_box_freeze(mi->o_con);

        if (mi->check)
          {
             mi->o_toggle = edje_object_add(evas);
             e_theme_edje_object_set(mi->o_toggle, "base/theme/menus", 
                                     "e/widgets/menu/default/check");
             evas_object_pass_events_set(mi->o_toggle, 1);
             evas_object_show(mi->o_toggle);
             e_box_pack_end(mi->o_con, mi->o_toggle);
             edje_object_size_min_calc(mi->o_toggle, &mi->tw, &mi->th);
          }
        else if (mi->radio)
          {
             mi->o_toggle = edje_object_add(evas);
             e_theme_edje_object_set(mi->o_toggle, "base/theme/menus", 
                                     "e/widgets/menu/default/radio");
             evas_object_pass_events_set(mi->o_toggle, 1);
             evas_object_show(mi->o_toggle);
             e_box_pack_end(mi->o_con, mi->o_toggle);
             edje_object_size_min_calc(mi->o_toggle, &mi->tw, &mi->th);
          }
        else
          {
             mi->o_toggle = evas_object_rectangle_add(evas);
             evas_object_color_set(mi->o_toggle, 0, 0, 0, 0);
             evas_object_pass_events_set(mi->o_toggle, 1);
             e_box_pack_end(mi->o_con, mi->o_toggle);
          }

        if ((mi->icon) || (mi->realize_cb.func))
          {
             int icon_w = 0, icon_h = 0;

             mi->o_icon_bg = edje_object_add(evas);
             if (e_theme_edje_object_set(mi->o_icon_bg, "base/theme/menus", 
                                         "e/widgets/menu/default/icon"))
               evas_object_show(mi->o_icon_bg);
             else
               {
                  evas_object_del(mi->o_icon_bg);
                  mi->o_icon_bg = NULL;
               }

             if (mi->icon)
               {
                  if (mi->icon_key)
                    {
                       Evas_Coord iww, ihh;

                       mi->o_icon = edje_object_add(evas);
                       if (edje_object_file_set(mi->o_icon, mi->icon, 
                                                mi->icon_key))
                         {
                            edje_object_size_max_get(mi->o_icon, &iww, &ihh);
                            icon_w = iww;
                            icon_h = ihh;
                         }
                       else
                         {
                            evas_object_del(mi->o_icon);
                            mi->o_icon = NULL;
                         }
                    }

                  if (!mi->o_icon)
                    {
                       mi->o_icon = e_icon_add(evas);
                       e_icon_scale_size_set(mi->o_icon, 
                                             e_util_icon_size_normalize(24 * e_scale));
                       e_icon_preload_set(mi->o_icon, 1);
                       e_icon_file_set(mi->o_icon, mi->icon);
                       e_icon_fill_inside_set(mi->o_icon, 1);
                       e_icon_size_get(mi->o_icon, &icon_w, &icon_h);
                    }
               }

             if (_e_menu_item_realize_call(mi))
               {
                  e_icon_fill_inside_set(mi->o_icon, 1);
                  e_icon_size_get(mi->o_icon, &icon_w, &icon_h);
               }

             evas_object_pass_events_set(mi->o_icon, 1);
             evas_object_show(mi->o_icon);

             if (mi->o_icon_bg)
               {
                  edje_extern_object_min_size_set(mi->o_icon, icon_w, icon_h);
                  edje_object_part_swallow(mi->o_icon_bg, "e.swallow.content",
                                           mi->o_icon);
                  edje_object_size_min_calc(mi->o_icon_bg, &mi->iw, &mi->ih);

                  edje_extern_object_min_size_set(mi->o_icon, 0, 0);
                  edje_object_part_swallow(mi->o_icon_bg, "e.swallow.content", 
                                           mi->o_icon);
                  e_box_pack_end(mi->o_con, mi->o_icon_bg);
               }
             else
               {
                  Evas_Object  *o;

                  o = edje_object_add(evas);
                  e_icon_size_get(mi->o_icon, &mi->iw, &mi->ih);
                  e_box_pack_end(mi->o_con, o);
               }
          }
        else
          {
             mi->o_icon = evas_object_rectangle_add(evas);
             evas_object_color_set(mi->o_icon, 0, 0, 0, 0);
             evas_object_pass_events_set(mi->o_icon, 1);
             e_box_pack_end(mi->o_con, mi->o_icon);
          }

        if (mi->label)
          {
             mi->o_lbl = edje_object_add(evas);
             e_theme_edje_object_set(mi->o_lbl, "base/theme/menus", 
                                     "e/widgets/menu/default/label");
             edje_object_part_text_set(mi->o_lbl, "e.text.label", mi->label);
             evas_object_pass_events_set(mi->o_lbl, 1);
             evas_object_show(mi->o_lbl);
             e_box_pack_end(mi->o_con, mi->o_lbl);
             edje_object_size_min_calc(mi->o_lbl, &mi->lw, &mi->lh);
          }
        else
          {
             mi->o_lbl = evas_object_rectangle_add(evas);
             evas_object_color_set(mi->o_lbl, 0, 0, 0, 0);
             evas_object_pass_events_set(mi->o_lbl, 1);
             e_box_pack_end(mi->o_con, mi->o_lbl);
          }

        if ((mi->submenu) || (mi->submenu_pre_cb.func))
          {
             mi->o_submenu = edje_object_add(evas);
             e_theme_edje_object_set(mi->o_submenu, "base/theme/menus", 
                                     "e/widgets/menu/default/submenu");
             evas_object_pass_events_set(mi->o_submenu, 1);
             evas_object_show(mi->o_submenu);
             e_box_pack_end(mi->o_con, mi->o_submenu);
             edje_object_size_min_calc(mi->o_submenu, &mi->subw, &mi->subh);
          }
        else
          {
             mi->o_submenu = evas_object_rectangle_add(evas);
             evas_object_color_set(mi->o_submenu, 0, 0, 0, 0);
             evas_object_pass_events_set(mi->o_submenu, 1);
             e_box_pack_end(mi->o_con, mi->o_submenu);
          }

        edje_object_part_swallow(mi->o_bg, "e.swallow.content", mi->o_con);

        mi->o_event = evas_object_rectangle_add(evas);
        evas_object_color_set(mi->o_event, 0, 0, 0, 0);
        evas_object_layer_set(mi->o_event, 1);
        evas_object_repeat_events_set(mi->o_event, 1);
        evas_object_event_callback_add(mi->o_event, EVAS_CALLBACK_MOUSE_IN, 
                                       _e_menu_item_cb_in, mi);
        evas_object_event_callback_add(mi->o_event, EVAS_CALLBACK_MOUSE_OUT, 
                                       _e_menu_item_cb_out, mi);
        evas_object_show(mi->o_event);

        e_box_thaw(mi->o_con);
        e_box_pack_end(mi->menu->o_con, mi->o_bg);
     }
   if (mi->active) e_menu_item_active_set(mi, 1);
   if (mi->toggle) e_menu_item_toggle_set(mi, 1);
   if (mi->disabled) e_menu_item_disabled_set(mi, 1);
}

static void 
_e_menu_item_unrealize(E_Menu_Item *mi)
{
   if (mi->o_con) e_box_freeze(mi->o_con);
   if (mi->o_separator) evas_object_del(mi->o_separator);
   mi->o_separator = NULL;
   if (mi->o_bg) evas_object_del(mi->o_bg);
   mi->o_bg = NULL;
   if (mi->o_toggle) evas_object_del(mi->o_toggle);
   mi->o_toggle = NULL;
   if (mi->o_icon_bg) evas_object_del(mi->o_icon_bg);
   mi->o_icon_bg = NULL;
   if (mi->o_icon) evas_object_del(mi->o_icon);
   mi->o_icon = NULL;
   if (mi->o_lbl) evas_object_del(mi->o_lbl);
   mi->o_lbl = NULL;
   if (mi->o_submenu) evas_object_del(mi->o_submenu);
   mi->o_submenu = NULL;
   if (mi->o_event) evas_object_del(mi->o_event);
   mi->o_event = NULL;
   if (mi->o_con)
     {
        e_box_thaw(mi->o_con);
        evas_object_del(mi->o_con);
     }
   mi->o_con = NULL;
}

static Eina_Bool 
_e_menu_item_realize_call(E_Menu_Item *mi)
{
   if ((mi) && (mi->realize_cb.func))
     {
        mi->realize_cb.func(mi->realize_cb.data, mi->menu, mi);
        return EINA_TRUE;
     }

   return EINA_FALSE;
}

static void 
_e_menu_item_submenu_activate(E_Menu_Item *mi)
{
   if (!mi->menu->active) return;

   if (mi->menu->fast_mouse)
     {
        mi->menu->pending_new_submenu = EINA_TRUE;
        return;
     }

   mi->menu->pending_new_submenu = EINA_FALSE;
   _e_menu_deactivate_above(mi->menu);

   if (mi->submenu_pre_cb.func)
     mi->submenu_pre_cb.func(mi->submenu_pre_cb.data, mi->menu, mi);

   if (mi->submenu)
     {
        E_Menu *m;

        m = mi->submenu;
        e_object_ref(E_OBJECT(m));
        m->parent_item = mi;

        _e_menu_activate_internal(m, mi->menu->zone);
        _e_menu_realize(m);
        _e_menu_reposition(m);

        e_object_unref(E_OBJECT(m));
     }
}

static void 
_e_menu_item_submenu_deactivate(E_Menu_Item *mi)
{
   if (!mi->menu->active) return;
   if (mi->submenu_post_cb.func)
     {
        printf("Menu Calling Submenu Post\n");
        mi->submenu_post_cb.func(mi->submenu_post_cb.data, mi->menu, mi);
     }
   /* else */
   /*   { */
   /*      if (mi->submenu) */
   /*        mi->submenu->active = EINA_FALSE; */

   /*      if (mi->menu) */
   /*        mi->menu->active = EINA_TRUE; */
   /*   } */
}
