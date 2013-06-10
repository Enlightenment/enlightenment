#ifdef E_TYPEDEFS

typedef struct _E_Compositor E_Compositor;

#else
# ifndef E_COMP_H
#  define E_COMP_H

#  define container_of(ptr, type, mbr) \
   ({ \
      const __typeof__(((type *)0)->mbr) *__mptr = (ptr); \
      (type *)((char *)__mptr - offsetof(type, mbr)); \
   })

struct _E_Compositor
{
   struct 
     {
        struct wl_display *display;
        struct wl_event_loop *loop;
        struct wl_event_loop *input_loop;
        struct wl_event_source *input_loop_source;
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

   E_Shell_Interface shell_interface;

   E_Renderer *renderer;
   E_Plane plane; // primary plane

   Ecore_Fd_Handler *fd_hdlr;
   Ecore_Idler *idler;

   unsigned int output_pool;

   Eina_List *planes;
   Eina_List *outputs;
   Eina_List *inputs;
   Eina_List *surfaces;

   void (*attach) (E_Surface *es, struct wl_buffer *buffer);
   void (*cb_ping) (void *surface, unsigned int serial);
};

EINTERN int e_comp_init(void);
EINTERN int e_comp_shutdown(void);

EAPI Eina_Bool e_compositor_init(E_Compositor *comp, void *display);
EAPI Eina_Bool e_compositor_shutdown(E_Compositor *comp);
EAPI E_Compositor *e_compositor_get(void);
EAPI void e_compositor_plane_stack(E_Compositor *comp, E_Plane *plane, E_Plane *above);
EAPI int e_compositor_input_read(int fd EINA_UNUSED, unsigned int mask EINA_UNUSED, void *data);
EAPI void e_compositor_damage_calculate(E_Compositor *comp);
EAPI void e_compositor_damage_flush(E_Compositor *comp, E_Surface *es);

# endif
#endif
