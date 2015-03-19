#ifdef E_TYPEDEFS
#else
#ifndef E_HINTS_H
#define E_HINTS_H

EINTERN void e_hints_init(Ecore_Window win, Ecore_Window propwin);
//EINTERN void e_hints_manager_init(E_Manager *man);
EAPI void e_hints_client_list_set(void);
EAPI void e_hints_client_stacking_set(void);

EAPI void e_hints_active_window_set(E_Client *ec);

EINTERN void e_hints_window_init(E_Client *ec);
EAPI void e_hints_window_state_set(E_Client *ec);
EAPI void e_hints_window_state_get(E_Client *ec);
EAPI void e_hints_window_type_set(E_Client *ec);
EAPI void e_hints_window_type_get(E_Client *ec);

EAPI void e_hints_window_visible_set(E_Client *ec);
EAPI void e_hints_window_iconic_set(E_Client *ec);
EAPI void e_hints_window_hidden_set(E_Client *ec);

EAPI void e_hints_window_shade_direction_set(E_Client *ec, E_Direction dir);
EAPI E_Direction e_hints_window_shade_direction_get(E_Client *ec);

EAPI void e_hints_window_size_set(E_Client *ec);
EAPI void e_hints_window_size_unset(E_Client *ec);
EAPI int  e_hints_window_size_get(E_Client *ec);

EAPI void e_hints_window_shaded_set(E_Client *ec, int on);
EAPI void e_hints_window_maximized_set(E_Client *ec, int horizontal, int vertical);
EAPI void e_hints_window_fullscreen_set(E_Client *ec, int on);
EAPI void e_hints_window_sticky_set(E_Client *ec, int on);
EAPI void e_hints_window_stacking_set(E_Client *ec, E_Stacking stacking);
EAPI void e_hints_window_desktop_set(E_Client *ec);

EAPI void e_hints_window_e_state_set(E_Client *ec);
EAPI void e_hints_window_e_state_get(E_Client *ec);

EAPI void e_hints_window_qtopia_soft_menu_get(E_Client *ec);
EAPI void e_hints_window_qtopia_soft_menus_get(E_Client *ec);

EAPI void e_hints_window_virtual_keyboard_state_get(E_Client *ec);
EAPI void e_hints_window_virtual_keyboard_get(E_Client *ec);


EAPI void e_hints_scale_update(void);


#ifdef E_COMP_X_H
EAPI void e_hints_window_state_update(E_Client *ec, int state, int action);
extern EAPI Ecore_X_Atom ATM__QTOPIA_SOFT_MENU;
extern EAPI Ecore_X_Atom ATM__QTOPIA_SOFT_MENUS;
extern EAPI Ecore_X_Atom ATM_GNOME_SM_PROXY;
extern EAPI Ecore_X_Atom ATM_ENLIGHTENMENT_COMMS;
extern EAPI Ecore_X_Atom ATM_ENLIGHTENMENT_VERSION;
extern EAPI Ecore_X_Atom ATM_ENLIGHTENMENT_SCALE;
extern EAPI Ecore_X_Atom ATM_NETWM_SHOW_WINDOW_MENU;
extern EAPI Ecore_X_Atom ATM_NETWM_PERFORM_BUTTON_ACTION;
#endif
#endif
#endif
