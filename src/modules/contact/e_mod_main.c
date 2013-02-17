#include "e_mod_main.h"

EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Contact" };

static void
_cb_in_left(void *data, int d, double v)
{
   // show PREV window in list from urrent focused window on top of current
   // window but in an inital "off to the right" state in comp
   Eina_List *borders = (Eina_List *)e_policy_borders_get();
   E_Border *bd_active = (E_Border *)e_polict_border_active_get();
   E_Border *bd = NULL;
   Eina_List *bd_active_l = NULL;
   if (!bd_active)
     {
        if (!borders) return;
        bd = eina_list_last(borders)->data;
     }
   if (!bd)
     {
        if (bd_active)
          bd_active_l = eina_list_data_find_list(borders, bd_active);
        if ((bd_active_l) && (bd_active_l->prev)) bd = bd_active_l->prev->data;
     }
   if ((!bd) && (bd_active))
     {
        e_border_iconify(bd_active);
        return;
     }
   if (!bd) return;
   e_border_uniconify(bd);
   e_border_raise(bd);
   e_border_show(bd);
}

static void
_cb_in_left_go(void *data, int d, double v)
{
   // as v > 0 (and heads towards 1.0) flip/slide new window in unbtil v > 1.0
   // and   once over 1.0 just do transition until end
}

static void
_cb_in_right(void *data, int d, double v)
{
   // show NEXT window in list from urrent focused window on top of current
   // window but in an inital "off to the right" state in comp
   Eina_List *borders = (Eina_List *)e_policy_borders_get();
   E_Border *bd_active = (E_Border *)e_polict_border_active_get();
   E_Border *bd = NULL;
   Eina_List *bd_active_l = NULL;
   if (!bd_active)
     {
        if (!borders) return;
        bd = borders->data;
     }
   if (!bd)
     {
        if (bd_active)
          bd_active_l = eina_list_data_find_list(borders, bd_active);
        if ((bd_active_l) && (bd_active_l->next)) bd = bd_active_l->next->data;
     }
   if ((!bd) && (bd_active))
     {
        e_border_iconify(bd_active);
        return;
     }
   if (!bd) return;
   e_border_uniconify(bd);
   e_border_raise(bd);
   e_border_show(bd);
}

static void
_cb_in_right_go(void *data, int d, double v)
{
   // as v > 0 (and heads towards 1.0) flip/slide new window in unbtil v > 1.0
   // and   once over 1.0 just do transition until end
}

static void
_cb_in_top(void *data, int d, double v)
{
   // show/populate top controls if not already there and start in offscreen
   // state and beign slide in anim and place controls at final spot
}

static void
_cb_in_top_go(void *data, int d, double v)
{
   // for now nothing - but animation would be nice for top controls
}

static void
_cb_in_bottom(void *data, int d, double v)
{
   // force kbd activation if no kbd
   e_policy_kbd_override_set(EINA_TRUE);
   // if kbd already up... hmmm show app menu?
}

static void
_cb_in_bottom_go(void *data, int d, double v)
{
   // for now nothing - but slide animation is nice
}

EAPI void *
e_modapi_init(E_Module *m __UNUSED__) 
{
   e_policy_init();
   e_edges_init();
   
   e_edges_handler_set(E_EDGES_LEFT_IN_BEGIN, _cb_in_left, NULL);
   e_edges_handler_set(E_EDGES_LEFT_IN_SLIDE, _cb_in_left_go, NULL);
   e_edges_handler_set(E_EDGES_RIGHT_IN_BEGIN, _cb_in_right, NULL);
   e_edges_handler_set(E_EDGES_RIGHT_IN_SLIDE, _cb_in_right_go, NULL);
   e_edges_handler_set(E_EDGES_TOP_IN_BEGIN, _cb_in_top, NULL);
   e_edges_handler_set(E_EDGES_TOP_IN_SLIDE, _cb_in_top_go, NULL);
   e_edges_handler_set(E_EDGES_BOTTOM_IN_BEGIN, _cb_in_bottom, NULL);
   e_edges_handler_set(E_EDGES_BOTTOM_IN_SLIDE, _cb_in_bottom_go, NULL);
   return m;
}

EAPI int 
e_modapi_shutdown(E_Module *m __UNUSED__) 
{
   e_edges_shutdown();
   e_policy_shutdown();
   return 1;
}

EAPI int 
e_modapi_save(E_Module *m __UNUSED__) 
{
   return 1;
}
