# include <dlfcn.h>

static int crude_hack_fd;
static void *e_drm2_lib;

void (*sym_ecore_drm2_output_crtc_size_get_120)(Ecore_Drm2_Output *output, int *w, int *h);
void (*sym_ecore_drm2_output_geometry_get_120)(Ecore_Drm2_Output *output, int *x, int *y, int *w, int *h);
void (*sym_ecore_drm2_output_resolution_get_120)(Ecore_Drm2_Output *output, int *w, int *h, unsigned int *refresh);
Ecore_Drm2_Device *(*sym_ecore_drm2_device_find_120)(const char *seat, unsigned int tty);
int (*sym_ecore_drm2_device_open_120)(Ecore_Drm2_Device *device);
Ecore_Drm2_Device *(*sym_ecore_drm2_device_open_121)(const char *seat, unsigned int tty);
void (*sym_ecore_drm2_device_free_120)(Ecore_Drm2_Device *device);
void (*sym_ecore_drm2_output_info_get_121)(Ecore_Drm2_Output *output, int *x, int *y, int *w, int *h, unsigned int *refresh);
Ecore_Drm2_Fb *(*sym_ecore_drm2_fb_create_120)(int fd, int width, int height, int depth, int bpp, unsigned int format);
Ecore_Drm2_Fb *(*sym_ecore_drm2_fb_create_121)(Ecore_Drm2_Device *dev, int width, int height, int depth, int bpp, unsigned int format);

#define E_DRM2_EFL_VERSION_MINIMUM(MAJ, MIN, MIC) \
  ((eina_version->major > MAJ) || (eina_version->minor > MIN) ||\
   ((eina_version->minor == MIN) && (eina_version->micro >= MIC)))

static Eina_Bool
e_drm2_compat_init(void)
{
#define EDRM2SYM(sym, ver) \
   sym_##sym##_##ver = dlsym(e_drm2_lib, #sym); \
   if (!sym_##sym##_##ver) \
     { \
        dlclose(e_drm2_lib); \
        return EINA_FALSE; \
     }

   e_drm2_lib = dlopen("libecore_drm2.so", RTLD_NOW | RTLD_LOCAL);
   if (E_DRM2_EFL_VERSION_MINIMUM(1, 20, 99))
     {
        EDRM2SYM(ecore_drm2_device_open, 121);
        EDRM2SYM(ecore_drm2_output_info_get, 121);
        EDRM2SYM(ecore_drm2_fb_create, 121);
        return EINA_TRUE;
     }

   EDRM2SYM(ecore_drm2_output_crtc_size_get, 120);
   EDRM2SYM(ecore_drm2_output_geometry_get, 120);
   EDRM2SYM(ecore_drm2_output_resolution_get, 120);
   EDRM2SYM(ecore_drm2_device_find, 120);
   EDRM2SYM(ecore_drm2_device_open, 120);
   EDRM2SYM(ecore_drm2_device_free, 120);
   EDRM2SYM(ecore_drm2_fb_create, 120);
   return EINA_TRUE;

#undef EDRM2SYM
}

static void
e_drm2_compat_shutdown(void)
{
   dlclose(e_drm2_lib);
}

static inline Ecore_Drm2_Device *
e_drm2_device_open(const char *seat, int vt)
{
   Ecore_Drm2_Device *out;

   if (E_DRM2_EFL_VERSION_MINIMUM(1, 20, 99))
     {
        return sym_ecore_drm2_device_open_121(seat, vt);
     }

   out = sym_ecore_drm2_device_find_120(seat, vt);
   if (!out) return NULL;

   crude_hack_fd = sym_ecore_drm2_device_open_120(out);
   if (crude_hack_fd < 0)
     {
        ecore_drm2_device_close(out);
        return NULL;
     }
   return out;
}

static inline void
e_drm2_device_close(Ecore_Drm2_Device *device)
{
   if (E_DRM2_EFL_VERSION_MINIMUM(1, 20, 99))
     {
        ecore_drm2_device_close(device);
        return;
     }
   ecore_drm2_device_close(device);
   sym_ecore_drm2_device_free_120(device);
}

static inline void
e_drm2_output_info_get(Ecore_Drm2_Output *op, int *x, int *y, int *w, int *h, unsigned int *refresh)
{
   if (E_DRM2_EFL_VERSION_MINIMUM(1, 20, 99))
     {
        sym_ecore_drm2_output_info_get_121(op, x, y, w, h, refresh);
        return;
     }
   sym_ecore_drm2_output_geometry_get_120(op, x, y, w, h);
   sym_ecore_drm2_output_resolution_get_120(op, NULL, NULL, refresh);
}

static inline Ecore_Drm2_Fb *
e_drm2_fb_create(Ecore_Drm2_Device *device, int width, int height, int depth, int bpp, unsigned int format)
{
   if (E_DRM2_EFL_VERSION_MINIMUM(1, 20, 99))
     {
        return sym_ecore_drm2_fb_create_121(device, width, height, depth, bpp, format);
     }
   return sym_ecore_drm2_fb_create_120(crude_hack_fd, width, height, depth, bpp, format);
}

#undef E_DRM2_EFL_VERSION_MINIMUM
