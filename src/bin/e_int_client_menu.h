#ifdef E_TYPEDEFS

#else
#ifndef E_INT_BORDER_MENU_H
#define E_INT_BORDER_MENU_H

typedef void (*E_Client_Menu_Hook_Cb)(void *, E_Client *);
typedef struct E_Client_Menu_Hook
{
   E_Client_Menu_Hook_Cb cb;
   void *data;
} E_Client_Menu_Hook;

EAPI E_Client_Menu_Hook *e_int_client_menu_hook_add(E_Client_Menu_Hook_Cb cb, const void *data);
EAPI void e_int_client_menu_hook_del(E_Client_Menu_Hook *hook);
EAPI void e_int_client_menu_hooks_clear(void);
EAPI void e_int_client_menu_create(E_Client *ec);
EAPI void e_int_client_menu_show(E_Client *ec, Evas_Coord x, Evas_Coord y, int key, unsigned int timestamp);
EAPI void e_int_client_menu_del(E_Client *ec);

#endif
#endif
