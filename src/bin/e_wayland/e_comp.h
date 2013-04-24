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

   Ecore_Fd_Handler *fd_hdlr;
   Ecore_Idler *idler;

   Eina_List *outputs;
   Eina_List *inputs;
   Eina_List *surfaces;

   void (*cb_ping) (void *surface, unsigned int serial);
};

EINTERN int e_comp_init(void);
EINTERN int e_comp_shutdown(void);

# endif
#endif
