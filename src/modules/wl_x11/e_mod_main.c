#include "e.h"
#include <Ecore_X.h>

E_API E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Wl_X11" };

static Ecore_Event_Handler *kbd_hdlr;

static void
_cb_delete_request(Ecore_Evas *ee EINA_UNUSED)
{
   ecore_main_loop_quit();
}

static Eina_Bool 
_cb_keymap_changed(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Config_XKB_Layout *ekbd;
   char *rules = NULL, *model = NULL, *layout = NULL;
   Ecore_X_Atom xkb = 0;
   Ecore_X_Window root = 0;
   int len = 0;
   unsigned char *dat;

   printf("KEYMAP CHANGED\n");

   /* try to fetch the E keyboard layout */
   if ((ekbd = e_xkb_layout_get()))
     {
        model = strdup(ekbd->model);
        layout = strdup(ekbd->name);
     }

   /* NB: we need a 'rules' so fetch from X atoms */
   root = ecore_x_window_root_first_get();
   xkb = ecore_x_atom_get("_XKB_RULES_NAMES");
   ecore_x_window_prop_property_get(root, xkb, ECORE_X_ATOM_STRING, 
                                    1024, &dat, &len);
   if ((dat) && (len > 0))
     {
        rules = (char *)dat;
        dat += strlen((const char *)dat) + 1;
        if (!model) model = strdup((const char *)dat);
        dat += strlen((const char *)dat) + 1;
        if (!layout) layout = strdup((const char *)dat);
     }

   /* fallback */
   if (!rules) rules = strdup("evdev");
   if (!model) model = strdup("pc105");
   if (!layout) layout = strdup("us");

   /* update compositor keymap */
   e_comp_wl_input_keymap_set(rules, model, layout);

   free(rules);
   free(model);
   free(layout);

   return ECORE_CALLBACK_PASS_ON;
}

E_API void *
e_modapi_init(E_Module *m)
{
   int w = 0, h = 0;
   Ecore_X_Window root, win;
   Eina_Bool managed;

   printf("LOAD WL_X11 MODULE\n");

   if (!ecore_x_init(NULL))
     {
        fprintf(stderr, "X11 connect failed!\n");
        return NULL;
     }
   root = ecore_x_window_root_first_get();
   managed = !!ecore_x_window_prop_window_get(root, ECORE_X_ATOM_NET_SUPPORTING_WM_CHECK,
                                      &win, 1);
   e_comp_x_randr_canvas_new(ecore_x_window_root_first_get(), 1, 1);

   if (!e_comp->ee)
     {
        ERR("Could not create ecore_evas canvas");
        return NULL;
     }
   ecore_evas_callback_delete_request_set(e_comp->ee, _cb_delete_request);
   ecore_evas_title_set(e_comp->ee, "Enlightenment: WL-X11");
   ecore_evas_name_class_set(e_comp->ee, "E", "compositor");

   ecore_evas_screen_geometry_get(e_comp->ee, NULL, NULL, &w, &h);
   if (!managed)
     e_comp_x_randr_screen_iface_set();
   if (!e_comp_wl_init()) return NULL;
   if (managed)
     w = w * 2 / 3, h = h * 2 / 3;
   if (!e_comp_canvas_init(w, h)) return NULL;

   e_comp_wl_input_pointer_enabled_set(EINA_TRUE);
   e_comp_wl_input_keyboard_enabled_set(EINA_TRUE);
   e_comp_wl_input_touch_enabled_set(EINA_TRUE);

   /* e_comp->pointer =  */
   /*   e_pointer_window_new(ecore_evas_window_get(e_comp->ee), EINA_TRUE); */
   e_comp->pointer = e_pointer_canvas_new(e_comp->ee, EINA_TRUE);
   e_comp->pointer->color = EINA_TRUE;

   /* force a keymap update so compositor keyboard gets setup */
   _cb_keymap_changed(e_comp->wl_comp_data, 0, NULL);

   /* setup keymap_change event handler */
   kbd_hdlr = 
     ecore_event_handler_add(ECORE_X_EVENT_XKB_STATE_NOTIFY, 
                             _cb_keymap_changed, e_comp->wl_comp_data);

   return m;
}

E_API int 
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   /* delete handler for keymap change */
   if (kbd_hdlr) ecore_event_handler_del(kbd_hdlr);
   ecore_x_shutdown();

   return 1;
}
