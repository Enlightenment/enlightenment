#ifdef E_TYPEDEFS
#else
# ifndef E_COMP_WL_COMP_H
#  define E_COMP_WL_COMP_H

Eina_Bool e_comp_wl_comp_init(void);
void e_comp_wl_comp_shutdown(void);
Wayland_Compositor *e_comp_wl_comp_get(void);
void e_comp_wl_comp_repick(struct wl_seat *seat, uint32_t timestamp);

# endif
#endif
