/* Dump every global the compositor advertises, one "interface<TAB>version"
 * per line, sorted.
 *
 * This exists rather than shelling out to wayland-info because wayland-info's
 * output format is not a stable contract and it is an extra dependency in CI.
 * The output here is compared against src/tests/wayland/globals.expected, so
 * any change to E's advertised protocol surface has to be made deliberately,
 * in the same commit, and shows up in review as a diff of that file.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>

#define UNUSED __attribute__((unused))

typedef struct
{
   char *iface;
   uint32_t version;
} Global;

static Global *globals = NULL;
static size_t globals_count = 0;
static size_t globals_alloc = 0;

static void
_global_add(void *data UNUSED, struct wl_registry *reg UNUSED,
            uint32_t id UNUSED, const char *iface, uint32_t version)
{
   if (globals_count == globals_alloc)
     {
        globals_alloc = globals_alloc ? globals_alloc * 2 : 32;
        globals = realloc(globals, globals_alloc * sizeof(*globals));
        if (!globals)
          {
             fprintf(stderr, "e_test_globals: out of memory\n");
             exit(1);
          }
     }
   globals[globals_count].iface = strdup(iface);
   globals[globals_count].version = version;
   globals_count++;
}

static void
_global_remove(void *data UNUSED, struct wl_registry *reg UNUSED,
               uint32_t id UNUSED)
{
}

static const struct wl_registry_listener _registry_listener =
{
   _global_add,
   _global_remove
};

static int
_global_cmp(const void *a, const void *b)
{
   const Global *ga = a, *gb = b;
   int r = strcmp(ga->iface, gb->iface);

   if (r) return r;
   if (ga->version < gb->version) return -1;
   if (ga->version > gb->version) return 1;
   return 0;
}

int
main(void)
{
   struct wl_display *disp;
   struct wl_registry *reg;
   size_t i;

   disp = wl_display_connect(NULL);
   if (!disp)
     {
        fprintf(stderr, "e_test_globals: cannot connect to the compositor\n");
        return 1;
     }

   reg = wl_display_get_registry(disp);
   wl_registry_add_listener(reg, &_registry_listener, NULL);

   /* Two roundtrips: the first delivers the registry's initial globals, the
    * second catches anything the compositor advertises in response to that
    * first batch being processed. */
   if ((wl_display_roundtrip(disp) < 0) || (wl_display_roundtrip(disp) < 0))
     {
        fprintf(stderr, "e_test_globals: roundtrip failed\n");
        return 1;
     }

   qsort(globals, globals_count, sizeof(*globals), _global_cmp);
   for (i = 0; i < globals_count; i++)
     {
        printf("%s\t%u\n", globals[i].iface, globals[i].version);
     }

   for (i = 0; i < globals_count; i++)
     {
        free(globals[i].iface);
     }
   free(globals);
   wl_registry_destroy(reg);
   wl_display_disconnect(disp);

   return 0;
}
