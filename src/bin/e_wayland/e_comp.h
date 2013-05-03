#ifdef E_TYPEDEFS

typedef struct _E_Compositor E_Compositor;

#else
# ifndef E_COMP_H
#  define E_COMP_H

struct _E_Compositor
{
   struct 
     {
        struct wl_display *display;
        struct wl_event_loop *loop;
        struct wl_event_loop *input_loop;
     } wl;

   struct 
     {
        struct wl_signal destroy;
        struct wl_signal activate;
        struct wl_signal kill;
        struct wl_signal seat;
     } signals;

#ifdef HAVE_WAYLAND_EGL
   struct 
     {
        EGLDisplay display;
        EGLContext context;
        EGLConfig config;
     } egl;
#endif

   E_Plane plane; // primary plane

   Ecore_Fd_Handler *fd_hdlr;
   Ecore_Idler *idler;

   unsigned int output_pool;

   Eina_List *planes;
   Eina_List *outputs;
   Eina_List *inputs;
   Eina_List *surfaces;

   void (*cb_ping) (void *surface, unsigned int serial);
};

EINTERN int e_comp_init(void);
EINTERN int e_comp_shutdown(void);

EAPI Eina_Bool e_compositor_init(E_Compositor *comp);
EAPI Eina_Bool e_compositor_shutdown(E_Compositor *comp);
EAPI E_Compositor *e_compositor_get(void);
EAPI void e_compositor_plane_stack(E_Compositor *comp, E_Plane *plane, E_Plane *above);

# endif
#endif
