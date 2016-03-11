/* Ask about updates checking */
#include "e_wizard.h"

static Eina_Bool do_up = 1;
/*
E_API int
wizard_page_init(E_Wizard_Page *pg EINA_UNUSED, Eina_Bool *need_xdg_desktops EINA_UNUSED, Eina_Bool *need_xdg_icons EINA_UNUSED)
{
   return 1;
}

E_API int
wizard_page_shutdown(E_Wizard_Page *pg EINA_UNUSED)
{
   return 1;
}
*/
E_API int
wizard_page_show(E_Wizard_Page *pg EINA_UNUSED)
{
   Evas_Object *o, *of, *ob;

   e_wizard_title_set(_("Updates"));

   of = elm_frame_add(e_comp->elm);
   elm_object_text_set(of, _("Check for available updates"));

   o = elm_box_add(of);
   elm_object_content_set(of, o);

   ob = elm_label_add(of);
   E_ALIGN(ob, 0, 0.5);
   evas_object_show(ob);
   elm_box_pack_end(o, ob);
   elm_object_text_set(ob,
     _("Enlightenment can check for new<br>"
       "versions, updates, security and<br>"
       "bugfixes, as well as available add-ons.<br>"
       "<br>"
       "This is very useful, because it lets<br>"
       "you know about available bug fixes and<br>"
       "security fixes when they happen. As a<br>"
       "result, Enlightenment will connect to<br>"
       "enlightenment.org and transmit some<br>"
       "information, much like any web browser<br>"
       "might do. No personal information such as<br>"
       "username, password or any personal files<br>"
       "will be transmitted. If you don't like this,<br>"
       "please disable this below. It is highly<br>"
       "advised that you do not disable this as it<br>"
       "may leave you vulnerable or having to live<br>"
       "with bugs."
       )
     );
   ob = elm_check_add(o);
   E_ALIGN(ob, 0, 0.5);
   evas_object_show(ob);
   elm_object_text_set(ob, _("Enable update checking"));
   elm_check_state_pointer_set(ob, &do_up);
   elm_box_pack_end(o, ob);

   evas_object_show(of);

   e_wizard_page_show(of);
   return 1; /* 1 == show ui, and wait for user, 0 == just continue */
}

E_API int
wizard_page_hide(E_Wizard_Page *pg EINA_UNUSED)
{
   e_config->update.check = do_up;
   e_config_save_queue();
   return 1;
}
/*
E_API int
wizard_page_apply(E_Wizard_Page *pg EINA_UNUSED)
{
   return 1;
}
*/
