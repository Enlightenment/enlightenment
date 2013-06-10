#ifdef E_TYPEDEFS

typedef struct _E_Renderer E_Renderer;

#else
# ifndef E_RENDERER_H
#  define E_RENDERER_H

#  ifdef HAVE_WAYLAND_EGL
#   ifndef EGL_WL_bind_wayland_display
#    define EGL_WL_bind_wayland_display 1
#    define EGL_WAYLAND_BUFFER_WL 0x31D5
#    define EGL_WAYLAND_PLANE_WL 0x31D6
#    define EGL_TEXTURE_Y_U_V_WL 0x31D7
#    define EGL_TEXTURE_Y_UV_WL 0x31D8
#    define EGL_TEXTURE_Y_XUXV_WL 0x31D9

struct wl_display;
struct wl_buffer;
#    ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglBindWaylandDisplayWL(EGLDisplay dpy, struct wl_display *display);
EGLAPI EGLBoolean EGLAPIENTRY eglUnbindWaylandDisplayWL(EGLDisplay dpy, struct wl_display *display);
EGLAPI EGLBoolean EGLAPIENTRY eglQueryWaylandBufferWL(EGLDisplay dpy, struct wl_buffer *buffer, EGLint attribute, EGLint *value);
#    endif
typedef EGLBoolean (EGLAPIENTRYP PFNEGLBINDWAYLANDDISPLAYWL) (EGLDisplay dpy, struct wl_display *display);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLUNBINDWAYLANDDISPLAYWL) (EGLDisplay dpy, struct wl_display *display);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYWAYLANDBUFFERWL) (EGLDisplay dpy, struct wl_buffer *buffer, EGLint attribute, EGLint *value);
#   endif

#   ifndef EGL_TEXTURE_EXTERNAL_WL
#    define EGL_TEXTURE_EXTERNAL_WL 0x31DA
#   endif

#    ifndef EGL_BUFFER_AGE_EXT
#     define EGL_BUFFER_AGE_EXT 0x313D
#    endif

#  endif

struct _E_Renderer
{
#  ifdef HAVE_WAYLAND_EGL
   struct 
     {
        EGLDisplay display;
        EGLContext context;
        EGLConfig config;
     } egl;

   struct wl_array vertices;
   struct wl_array indices;
   struct wl_array vtxcnt;

   PFNGLEGLIMAGETARGETTEXTURE2DOESPROC image_target_texture_2d;
   PFNEGLCREATEIMAGEKHRPROC image_create;
   PFNEGLDESTROYIMAGEKHRPROC image_destroy;

   PFNEGLBINDWAYLANDDISPLAYWL display_bind;
   PFNEGLUNBINDWAYLANDDISPLAYWL display_unbind;
   PFNEGLQUERYWAYLANDBUFFERWL buffer_query;

   Eina_Bool have_unpack : 1;
   Eina_Bool have_bind : 1;
   Eina_Bool have_external : 1;
   Eina_Bool have_buffer_age : 1;

   struct 
     {
        E_Shader texture_rgba;
        E_Shader texture_rgbx;
        E_Shader texture_egl_external;
        E_Shader texture_y_uv;
        E_Shader texture_y_u_v;
        E_Shader texture_y_xuxv;
        E_Shader invert_color;
        E_Shader solid;
        E_Shader *current;
     } shaders;
#  endif

   Eina_Bool (*pixels_read)(E_Output *output, int format, void *pixels, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
   void (*output_buffer_set)(E_Output *output, pixman_image_t *buffer);
   void (*output_repaint)(E_Output *output, pixman_region32_t *damage);
   void (*damage_flush)(E_Surface *surface);
   void (*attach)(E_Surface *surface, struct wl_buffer *buffer);
   Eina_Bool (*output_create)(E_Output *output, unsigned int window);
   void (*output_destroy)(E_Output *output);
   Eina_Bool (*surface_create)(E_Surface *surface);
   void (*surface_destroy)(E_Surface *surface);
   void (*surface_color_set)(E_Surface *surface, int r, int g, int b, int a);
   void (*destroy)(E_Compositor *comp);
};

EAPI Eina_Bool e_renderer_create(E_Compositor *comp);

# endif
#endif
