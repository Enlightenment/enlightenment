/* What a client is told about an output: when it binds, and when the output
 * changes after that. */
#include <stdio.h>
#include <string.h>
#include "e_wl_testkit.h"

#define MAX_OUTPUTS 4

typedef struct
{
   struct wl_output *output;
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

   wl_output_release(outputs[0].output);
   wl_output_release(outputs[1].output);
   _settle();

   wl_registry_destroy(reg);
   wl_test_destroy(tester);
   tk_disconnect(tk);
   return 0;
}
