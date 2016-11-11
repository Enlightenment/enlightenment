#ifndef _E_MOD_MAIN_H
#define _E_MOD_MAIN_H

EINTERN Eina_Bool   e_input_panel_init(void);
EINTERN void        e_input_panel_shutdown(void);

EINTERN Eina_Hash *shell_resources;
EINTERN Eina_Hash *xdg_shell_resources;

EINTERN void e_shell_surface_destroy(struct wl_resource *resource);
EINTERN void e_shell_surface_cb_destroy(struct wl_resource *resource);
EINTERN void e_shell_surface_parent_set(E_Client *ec, struct wl_resource *parent_resource);
EINTERN void e_shell_surface_mouse_down_helper(E_Client *ec, E_Binding_Event_Mouse_Button *ev, Eina_Bool move);

EINTERN Eina_Bool e_xdg_shell_v5_init(void);
EINTERN Eina_Bool e_xdg_shell_v6_init(void);
EINTERN void wl_shell_cb_bind(struct wl_client *client, void *data EINA_UNUSED, uint32_t version, uint32_t id);

struct E_Shell_Data
{
   uint32_t edges;
   int32_t width;
   int32_t height;
   Eina_List *pending;
   struct wl_resource *surface;
   void *shell;
   Eina_Bool fullscreen : 1;
   Eina_Bool maximized : 1;
   Eina_Bool activated : 1;
};

#endif
