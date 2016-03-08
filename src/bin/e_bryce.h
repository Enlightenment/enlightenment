#ifndef E_TYPEDEFS
#ifndef E_BRYCE_H
# define E_BRYCE_H

EINTERN void e_bryce_init(void);
EINTERN void e_bryce_shutdown(void);

E_API Evas_Object *e_bryce_add(Evas_Object *parent, const char *name, E_Gadget_Site_Orient orient, E_Gadget_Site_Anchor an);
E_API void e_bryce_orient(Evas_Object *bryce, E_Gadget_Site_Orient orient, E_Gadget_Site_Anchor an);
E_API Evas_Object *e_bryce_site_get(Evas_Object *bryce);
E_API Eina_Bool e_bryce_autosize_get(Evas_Object *bryce);
E_API void e_bryce_autosize_set(Evas_Object *bryce, Eina_Bool set);
E_API Eina_Bool e_bryce_autohide_get(Evas_Object *bryce);
E_API void e_bryce_autohide_set(Evas_Object *bryce, Eina_Bool set);
E_API Eina_Bool e_bryce_exists(Evas_Object *parent, Evas_Object *bryce, E_Gadget_Site_Orient orient, E_Gadget_Site_Anchor an);
E_API Eina_List *e_bryce_list(Evas_Object *parent);
E_API void e_bryce_style_set(Evas_Object *bryce, const char *style);

E_API Evas_Object *e_bryce_editor_add(Evas_Object *parent, Evas_Object *bryce);
E_API Evas_Object *e_bryce_edit(Evas_Object *bryce);
#endif
#endif
