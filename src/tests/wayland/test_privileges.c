/* zwp_input_method_manager_v2 is only advertised to a program a .desktop
 * file in wayland-privileges grants input_method to. The files are read
 * again for every client, so each case is a new connection. The test runs
 * with its directory in E's PATH. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "e_wl_testkit.h"

#define PROG "test-privileges"
#define FILE_NAME "test-privileges"

static void
_expect(int advertised, const char *what)
{
   Tk *tk = tk_connect(PROG);
   uint32_t version = tk_global_version(tk, "zwp_input_method_manager_v2");

   if (!tk_global_version(tk, "zwp_text_input_manager_v3"))
     tk_fail(tk, "%s: zwp_text_input_manager_v3 is hidden", what);
   if (advertised && (!version))
     tk_fail(tk, "%s: zwp_input_method_manager_v2 is hidden", what);
   if ((!advertised) && version)
     tk_fail(tk, "%s: zwp_input_method_manager_v2 is advertised", what);
   tk_disconnect(tk);
}

int
main(void)
{
   char path[PATH_MAX], quoted[PATH_MAX + 16];
   ssize_t n;

   n = readlink("/proc/self/exe", path, sizeof(path) - 1);
   if (n <= 0)
     {
        perror("/proc/self/exe");
        return 1;
     }
   path[n] = 0;
   snprintf(quoted, sizeof(quoted), "\"%s\" --flag %%U", path);

   _expect(0, "no file");
   tk_privileges_file(FILE_NAME, NULL, NULL);
   _expect(0, "a file without the key");
   tk_privileges_file(FILE_NAME, NULL, "input_methods;virtual_keyboard;");
   _expect(0, "a file without input_method");
   tk_privileges_file(FILE_NAME, "/bin/true", "input_method;");
   _expect(0, "a file for another program");
   tk_privileges_file(FILE_NAME, "true", "input_method;");
   _expect(0, "a file for another program found in PATH");
   tk_privileges_file(FILE_NAME, quoted, "virtual_keyboard;input_method");
   _expect(1, "a file for this program");
   tk_privileges_file(FILE_NAME, strrchr(path, '/') + 1, "input_method;");
   _expect(1, "a file for this program found in PATH");

   snprintf(path, sizeof(path), "%s/" FILE_NAME ".desktop", getenv("E_TEST_PRIVILEGES_DIR"));
   unlink(path);
   _expect(0, "the file is gone");

   printf(PROG ": ok\n");
   return 0;
}
