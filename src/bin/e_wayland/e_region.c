#include "e.h"

/* local function prototypes */
static void _e_region_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource);
static void _e_region_cb_add(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int x, int y, int w, int h);
static void _e_region_cb_subtract(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int x, int y, int w, int h);

/* local wayland interfaces */
static const struct wl_region_interface _e_region_interface = 
{
   _e_region_cb_destroy,
   _e_region_cb_add,
   _e_region_cb_subtract
};

EAPI E_Region *
e_region_new(unsigned int id)
{
   E_Region *reg;

   /* try to allocation space for a new region */
   if (!(reg = E_NEW_RAW(E_Region, 1))) return NULL;

   reg->resource.object.id = id;
   reg->resource.object.interface = &wl_region_interface;
   reg->resource.object.implementation = (void (**)(void))&_e_region_interface;
   reg->resource.data = reg;

   reg->region = eina_rectangle_new(0, 0, 0, 0);

   return reg;
}

/* local functions */
static void 
_e_region_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void 
_e_region_cb_add(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int x, int y, int w, int h)
{
   E_Region *reg;

   /* try to cast resource to our region */
   if (!(reg = resource->data)) return;

   eina_rectangle_union(reg->region, &(Eina_Rectangle){ x, y, w, h });
}

static void 
_e_region_cb_subtract(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int x, int y, int w, int h)
{
   E_Region *reg;
   Eina_Rectangle *rect;
   Eina_Bool ret = EINA_FALSE;

   /* try to cast resource to our region */
   if (!(reg = resource->data)) return;

   rect = eina_rectangle_new(x, y, w, h);
   ret = eina_rectangle_intersection(reg->region, rect);
   eina_rectangle_free(rect);
}
