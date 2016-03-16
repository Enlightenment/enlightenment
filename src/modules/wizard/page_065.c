/* ask about policy mouse binding modifiers */
#include "e_wizard.h"

static Eina_Bool shift;
static Eina_Bool ctrl;
static Eina_Bool alt;
static Eina_Bool win;
static Eina_Bool altgr;

static const char *names[] =
{
   "Shift",
   "Control",
   "Alt",
   "Win",
   "AltGr",
};

static struct
{
   Eina_Bool *val;
   const char *name;
} keys[5];

static unsigned int current;

static void
modifiers_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Eina_Bool *val = data;
   unsigned long i, binding = 0;
   Eina_List *l;
   E_Config_Binding_Mouse *ebm;

   *val = elm_check_state_get(obj);
   for (i = 0; i < 5; i++)
     if (*keys[i].val)
       binding |= (1 << i);
   if (binding == current) return;
   current = binding;
   EINA_LIST_FOREACH(e_bindings->mouse_bindings, l, ebm)
     if (eina_streq(ebm->action, "window_move") ||
         eina_streq(ebm->action, "window_resize") ||
         eina_streq(ebm->action, "window_menu"))
       ebm->modifiers = current;
   e_config_save_queue();
}

static void
check_add(Evas_Object *box, const char *txt, Eina_Bool *val)
{
   Evas_Object *ck;

   ck = elm_check_add(box);
   evas_object_smart_callback_add(ck, "changed", modifiers_changed, val);
   evas_object_show(ck);
   E_ALIGN(ck, 0, 0.5);
   elm_object_text_set(ck, _(txt));
   elm_check_state_pointer_set(ck, val);
   elm_box_pack_end(box, ck);
}

E_API int
wizard_page_show(E_Wizard_Page *pg EINA_UNUSED)
{
   Evas_Object *o, *of, *ob;
   unsigned int i, num = 0;
   Eina_List *l;
   char buf[4096];
   Eina_Strbuf *sbuf;
   E_Config_Binding_Mouse *ebm;

   EINA_LIST_FOREACH(e_bindings->mouse_bindings, l, ebm)
     if (eina_streq(ebm->action, "window_move") ||
         eina_streq(ebm->action, "window_resize") ||
         eina_streq(ebm->action, "window_menu"))
       {
          current = ebm->modifiers;
          break;
       }
   if (!current) return 0;
   e_wizard_title_set(_("Mouse Modifiers"));

   keys[0].val = &shift;
   keys[1].val = &ctrl;
   keys[2].val = &alt;
   keys[3].val = &win;
   keys[4].val = &altgr;
   sbuf = eina_strbuf_new();
   for (i = 0; i < 5; i++)
     {
        keys[i].name = names[i];
        *keys[i].val = (current & (1 << i));
        if (!*keys[i].val) continue;
        if (eina_strbuf_length_get(sbuf))
          eina_strbuf_append_char(sbuf, '+');
        eina_strbuf_append_printf(sbuf, "<hilight>%s</hilight>", _(keys[i].name));
        num++;
     }

   of = elm_frame_add(e_comp->elm);
   elm_object_text_set(of, _("Keys:"));
   o = elm_box_add(of);
   elm_object_content_set(of, o);

   ob = elm_label_add(o);
   elm_object_style_set(ob, "default/left");
   snprintf(buf, sizeof(buf), _("Enlightenment sets default mouse bindings for objects.<br>"
                                "These bindings provide three operations on click:<br>"
                                "* <b>Move</b> (left button)<br>"
                                "* <b>Resize</b> (middle/wheel button)<br>"
                                "* <b>Open Menu</b> (right button)<br>"
                                "The default key%s which must be held to activate these bindings<br>"
                                "is '%s'. Configure the modifiers for this binding below."),
                                num > 1 ? "s" : "", eina_strbuf_string_get(sbuf));
   elm_object_text_set(ob, buf);
   evas_object_show(ob);
   E_ALIGN(ob, 0, 0.5);
   elm_box_pack_end(o, ob);

   eina_strbuf_free(sbuf);

   for (i = 0; i < 5; i++)
     check_add(o, keys[i].name, keys[i].val);

   e_wizard_page_show(of);

   return 1; /* 1 == show ui, and wait for user, 0 == just continue */return 1;
}

E_API int
wizard_page_hide(E_Wizard_Page *pg EINA_UNUSED)
{
   return 1;
}
