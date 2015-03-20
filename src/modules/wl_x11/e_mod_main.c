#include "e.h"
#include <Ecore_X.h>

EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Wl_X11" };

static Ecore_Event_Handler *kbd_hdlr;

static void
_cb_delete_request(Ecore_Evas *ee EINA_UNUSED)
{
   ecore_main_loop_quit();
}

static Eina_Bool 
_cb_keymap_changed(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Comp_Data *cdata;
   E_Config_XKB_Layout *ekbd;
   char *rules = NULL, *model = NULL, *layout = NULL;
   Ecore_X_Atom xkb = 0;
   Ecore_X_Window root = 0;
   int len = 0;
   unsigned char *dat;

   printf("KEYMAP CHANGED\n");

   if (!(cdata = data)) return ECORE_CALLBACK_PASS_ON;

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
   e_comp_wl_input_keymap_set(cdata, rules, model, layout);

   free(rules);
   free(model);
   free(layout);

   return ECORE_CALLBACK_PASS_ON;
}

EAPI void *
e_modapi_init(E_Module *m)
{
   Ecore_Evas *ee;
   E_Screen *screen;
   E_Comp *comp;
   int w = 0, h = 0;

   printf("LOAD WL_X11 MODULE\n");

   ee = ecore_evas_software_x11_new(NULL, 0, 0, 0, 1, 1);
   ecore_evas_callback_delete_request_set(ee, _cb_delete_request);

   if (!(comp = e_comp))
     {
        comp = e_comp_new();
        comp->comp_type = E_PIXMAP_TYPE_WL;
     }

   comp->ee = ee;
   if (!comp->ee)
     {
        ERR("Could not create ecore_evas canvas");
        return NULL;
     }

   ecore_evas_screen_geometry_get(comp->ee, NULL, NULL, &w, &h);

   if (!e_xinerama_fake_screens_exist())
     {
        screen = E_NEW(E_Screen, 1);
        screen->escreen = screen->screen = 0;
        screen->x = 0;
        screen->y = 0;
        screen->w = w;
        screen->h = h;
        e_xinerama_screens_set(eina_list_append(NULL, screen));
     }

   if (!e_comp_canvas_init(w, h)) return NULL;

   /* NB: This needs to be called AFTER comp_canvas has been setup as it 
    * makes reference to the comp->evas */
   if (!e_comp_wl_init()) return NULL;

   e_comp_wl_input_pointer_enabled_set(comp->wl_comp_data, EINA_TRUE);
   e_comp_wl_input_keyboard_enabled_set(comp->wl_comp_data, EINA_TRUE);

   /* comp->pointer =  */
   /*   e_pointer_window_new(ecore_evas_window_get(comp->ee), EINA_TRUE); */
   comp->pointer = e_pointer_canvas_new(comp->ee, EINA_TRUE);
   comp->pointer->color = EINA_TRUE;

   /* force a keymap update so compositor keyboard gets setup */
   _cb_keymap_changed(comp->wl_comp_data, 0, NULL);

   /* setup keymap_change event handler */
   kbd_hdlr = 
     ecore_event_handler_add(ECORE_X_EVENT_XKB_STATE_NOTIFY, 
                             _cb_keymap_changed, comp->wl_comp_data);

   return m;
}

EAPI int 
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   /* delete handler for keymap change */
   if (kbd_hdlr) ecore_event_handler_del(kbd_hdlr);

   return 1;
}
