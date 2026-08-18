/* xdg-foreign: set_parent_of right after an import that failed.
 *
 * A client cannot have read the 'destroyed' event for a failed import
 * before sending set_parent_of on the same object, so the compositor has to
 * accept the request on an object it has already given up on. The test only
 * checks that the connection survives: before the fix, E crashed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "e_wl_testkit.h"
#include "xdg-foreign-unstable-v1-client-protocol.h"

#define PROG "test-xdg-foreign"

static int destroyed_v1;

static void
_imported_v1_destroyed(void *data, struct zxdg_imported_v1 *i)
{
   (void)data; (void)i;
   destroyed_v1++;
}

static const struct zxdg_imported_v1_listener _imported_v1_listener =
{
   _imported_v1_destroyed
};

int
main(void)
{
   Tk *tk;
   Tk_Toplevel *kid;
   struct zxdg_importer_v1 *importer_v1;
   struct zxdg_imported_v1 *bad;

   tk = tk_connect(PROG);

   if (tk_global_version(tk, "zxdg_importer_v1") < 1)
     tk_fail(tk, "no zxdg_importer_v1");

   importer_v1 = tk_bind(tk, &zxdg_importer_v1_interface, 1);
   if (!importer_v1) tk_fail(tk, "advertised but would not bind");

   kid = tk_toplevel_new(tk, "foreign-child", "child", 200, 150);
   tk_settle(tk);
   tk_expect(tk, "foreign-child");

   /* Back to back, deliberately: the whole point is that the client has not
    * read 'destroyed' yet when it sends the second request. */
   bad = zxdg_importer_v1_import(importer_v1, "no-such-handle");
   zxdg_imported_v1_add_listener(bad, &_imported_v1_listener, NULL);
   zxdg_imported_v1_set_parent_of(bad, tk_toplevel_surface(kid));

   /* tk_sync exits the process with "disconnected during sync" if the
    * compositor has gone, so reaching the line after it is the assertion. */
   tk_sync(tk);
   tk_expect(tk, "foreign-child");

   if (!destroyed_v1)
     tk_fail(tk, "importing a handle nobody exported was not answered with "
                 "'destroyed' - the client is left holding an object that "
                 "will never resolve");

   printf(PROG ": set_parent_of after a failed import - compositor survived\n");
   printf(PROG ": ok\n");
   tk_disconnect(tk);
   return 0;
}
