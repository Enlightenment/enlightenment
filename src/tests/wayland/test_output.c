/* What a client is told about an output: when it binds, and when the output
 * changes after that. */
#include <stdio.h>
#include <string.h>
#include "e_wl_testkit.h"
#include "xdg-output-unstable-v1-client-protocol.h"

#define MAX_OUTPUTS 4

typedef struct
{
   struct zxdg_output_v1 *xdg;
   char name[64];
   int x, y, w, h;
   int names, descriptions, dones;
} Xdg_Output;

typedef struct
{
   struct wl_output *output;
   Xdg_Output *xdg;
   int xdg_w_at_done;
   uint32_t global;
   char name[64];
   char description[128];
   int x, y, w, h;
   int names, descriptions, dones;
   int name_after_done, description_after_done;
} Output;

static Tk *tk;
static Output outputs[MAX_OUTPUTS];
static int output_count = 0;
static Output old_client;
static Xdg_Output xdg_outputs[MAX_OUTPUTS];
static Xdg_Output xdg_v2;

static void
_geometry(void *data, struct wl_output *o __attribute__((unused)), int32_t x, int32_t y,
          int32_t pw __attribute__((unused)), int32_t ph __attribute__((unused)),
          int32_t subpixel __attribute__((unused)), const char *make __attribute__((unused)),
          const char *model __attribute__((unused)), int32_t transform __attribute__((unused)))
{
   Output *out = data;

   out->x = x;
   out->y = y;
}

static void
_mode(void *data, struct wl_output *o __attribute__((unused)),
      uint32_t flags __attribute__((unused)), int32_t w, int32_t h,
      int32_t refresh __attribute__((unused)))
{
   Output *out = data;

   out->w = w;
   out->h = h;
}

static void
_done(void *data, struct wl_output *o __attribute__((unused)))
{
   Output *out = data;

   out->dones++;
   if (out->xdg) out->xdg_w_at_done = out->xdg->w;
}

static void
_scale(void *data __attribute__((unused)), struct wl_output *o __attribute__((unused)),
       int32_t factor __attribute__((unused)))
{
}

static void
_name(void *data, struct wl_output *o __attribute__((unused)), const char *name)
{
   Output *out = data;

   snprintf(out->name, sizeof(out->name), "%s", name);
   out->names++;
   if (out->dones) out->name_after_done = 1;
}

static void
_description(void *data, struct wl_output *o __attribute__((unused)), const char *description)
{
   Output *out = data;

   snprintf(out->description, sizeof(out->description), "%s", description);
   out->descriptions++;
   if (out->dones) out->description_after_done = 1;
}

static const struct wl_output_listener _output_listener =
{
   _geometry, _mode, _done, _scale, _name, _description
};

static void
_xdg_position(void *data, struct zxdg_output_v1 *o __attribute__((unused)), int32_t x, int32_t y)
{
   Xdg_Output *xo = data;

   xo->x = x;
   xo->y = y;
}

static void
_xdg_size(void *data, struct zxdg_output_v1 *o __attribute__((unused)), int32_t w, int32_t h)
{
   Xdg_Output *xo = data;

   xo->w = w;
   xo->h = h;
}

static void
_xdg_done(void *data, struct zxdg_output_v1 *o __attribute__((unused)))
{
   Xdg_Output *xo = data;

   xo->dones++;
}

static void
_xdg_name(void *data, struct zxdg_output_v1 *o __attribute__((unused)), const char *name)
{
   Xdg_Output *xo = data;

   snprintf(xo->name, sizeof(xo->name), "%s", name);
   xo->names++;
}

static void
_xdg_description(void *data, struct zxdg_output_v1 *o __attribute__((unused)),
                 const char *description __attribute__((unused)))
{
   Xdg_Output *xo = data;

   xo->descriptions++;
}

static const struct zxdg_output_v1_listener _xdg_output_listener =
{
   _xdg_position, _xdg_size, _xdg_done, _xdg_name, _xdg_description
};

static void
_xdg_output_get(struct zxdg_output_manager_v1 *manager, Output *out, Xdg_Output *xo)
{
   xo->xdg = zxdg_output_manager_v1_get_xdg_output(manager, out->output);
   zxdg_output_v1_add_listener(xo->xdg, &_xdg_output_listener, xo);
}

static void
_check_xdg(const Output *out, const Xdg_Output *xo)
{
   if ((xo->x != out->x) || (xo->y != out->y) ||
       (xo->w != out->w) || (xo->h != out->h))
     tk_fail(tk, "%s: logical %d,%d %dx%d, output %d,%d %dx%d", out->name,
             xo->x, xo->y, xo->w, xo->h, out->x, out->y, out->w, out->h);
   if (xo->names != 1) tk_fail(tk, "xdg_output name sent %d times", xo->names);
   if (xo->descriptions != 1)
     tk_fail(tk, "xdg_output description sent %d times", xo->descriptions);
   if (strcmp(xo->name, out->name))
     tk_fail(tk, "xdg_output is named %s, wl_output %s", xo->name, out->name);
}

static void
_global(void *data __attribute__((unused)), struct wl_registry *reg, uint32_t id,
        const char *iface, uint32_t version)
{
   Output *out;

   if (strcmp(iface, "wl_output")) return;
   if (version != 4) tk_fail(tk, "wl_output is at version %u, not 4", version);
   if (output_count == MAX_OUTPUTS) tk_fail(tk, "too many outputs");

   out = &outputs[output_count++];
   out->global = id;
   out->output = wl_registry_bind(reg, id, &wl_output_interface, 4);
   wl_output_add_listener(out->output, &_output_listener, out);
}

static void
_global_remove(void *data __attribute__((unused)), struct wl_registry *reg __attribute__((unused)),
               uint32_t id __attribute__((unused)))
{
}

static const struct wl_registry_listener _registry_listener =
{
   _global, _global_remove
};

static void
_settle(void)
{
   tk_sync(tk);
   wl_display_roundtrip(tk_display(tk));
}

static void
_check_bound(const Output *out)
{
   if (out->names != 1) tk_fail(tk, "%d name events on bind", out->names);
   if (out->descriptions != 1)
     tk_fail(tk, "%d description events on bind", out->descriptions);
   if (!out->name[0]) tk_fail(tk, "empty output name");
   if (!out->description[0]) tk_fail(tk, "empty output description");
   if (out->name_after_done || out->description_after_done)
     tk_fail(tk, "%s: name or description came after done", out->name);
   if (out->dones != 1) tk_fail(tk, "%d done events on bind", out->dones);
}

int
main(void)
{
   struct wl_registry *reg;
   struct wl_test *tester;
   struct zxdg_output_manager_v1 *manager, *manager_v2;
   int dones;

   tk = tk_connect("test-output");
   tester = tk_bind(tk, &wl_test_interface, 5);
   if (!tester) tk_fail(tk, "no wl_test version 5");

   reg = wl_display_get_registry(tk_display(tk));
   wl_registry_add_listener(reg, &_registry_listener, NULL);
   _settle();
   if (output_count != 1) tk_fail(tk, "%d outputs, expected 1", output_count);

   wl_test_zone_add(tester, 1024, 0, 800, 600);
   _settle();
   _settle();
   if (output_count != 2) tk_fail(tk, "zone_add made no new output");

   _check_bound(&outputs[0]);
   _check_bound(&outputs[1]);
   if (!strcmp(outputs[0].name, outputs[1].name))
     tk_fail(tk, "two outputs are both named %s", outputs[0].name);

   old_client.global = outputs[1].global;
   old_client.output = wl_registry_bind(reg, old_client.global,
                                        &wl_output_interface, 2);
   wl_output_add_listener(old_client.output, &_output_listener, &old_client);
   _settle();
   if (old_client.names || old_client.descriptions)
     tk_fail(tk, "a version 2 object was sent name or description");
   if (old_client.dones != 1) tk_fail(tk, "version 2 object got no done");

   manager = tk_bind(tk, &zxdg_output_manager_v1_interface, 3);
   manager_v2 = tk_bind(tk, &zxdg_output_manager_v1_interface, 2);
   if ((!manager) || (!manager_v2)) tk_fail(tk, "no zxdg_output_manager_v1");
   _xdg_output_get(manager, &outputs[0], &xdg_outputs[0]);
   _xdg_output_get(manager, &outputs[1], &xdg_outputs[1]);
   _xdg_output_get(manager_v2, &outputs[1], &xdg_v2);
   outputs[1].xdg = &xdg_outputs[1];
   _settle();
   _check_xdg(&outputs[0], &xdg_outputs[0]);
   _check_xdg(&outputs[1], &xdg_outputs[1]);
   _check_xdg(&outputs[1], &xdg_v2);
   if (xdg_outputs[1].dones)
     tk_fail(tk, "xdg_output.done sent to a version 3 object");
   if (xdg_v2.dones != 1) tk_fail(tk, "version 2 xdg_output got no done");

   dones = outputs[1].dones;
   wl_test_output_configure(tester, 1, 1100, 20, 640, 480);
   _settle();

   if (outputs[1].dones != (dones + 1))
     tk_fail(tk, "reconfigure sent %d done events", outputs[1].dones - dones);
   if ((outputs[1].x != 1100) || (outputs[1].y != 20) ||
       (outputs[1].w != 640) || (outputs[1].h != 480))
     tk_fail(tk, "output is %d,%d %dx%d after reconfigure",
             outputs[1].x, outputs[1].y, outputs[1].w, outputs[1].h);
   if (outputs[1].names != 1)
     tk_fail(tk, "name sent %d times: the protocol allows once",
             outputs[1].names);
   if (outputs[1].descriptions != 1)
     tk_fail(tk, "unchanged description sent %d times",
             outputs[1].descriptions);
   if (old_client.w != 640) tk_fail(tk, "version 2 object missed the change");
   if (old_client.names) tk_fail(tk, "version 2 object was sent a name");

   _check_xdg(&outputs[1], &xdg_outputs[1]);
   _check_xdg(&outputs[1], &xdg_v2);
   if (outputs[1].xdg_w_at_done != 640)
     tk_fail(tk, "wl_output.done came before the new logical size");
   if (xdg_outputs[1].dones)
     tk_fail(tk, "xdg_output.done sent to a version 3 object");
   if (xdg_v2.dones != 2)
     tk_fail(tk, "version 2 xdg_output got %d done events", xdg_v2.dones);

   zxdg_output_v1_destroy(xdg_outputs[0].xdg);
   zxdg_output_v1_destroy(xdg_outputs[1].xdg);
   zxdg_output_v1_destroy(xdg_v2.xdg);
   zxdg_output_manager_v1_destroy(manager);
   zxdg_output_manager_v1_destroy(manager_v2);
   _settle();
   wl_test_output_configure(tester, 1, 1200, 0, 640, 480);
   _settle();

   wl_output_release(outputs[0].output);
   wl_output_release(outputs[1].output);
   _settle();

   wl_registry_destroy(reg);
   wl_test_destroy(tester);
   tk_disconnect(tk);
   return 0;
}
