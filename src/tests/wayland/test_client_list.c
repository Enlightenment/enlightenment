/* wl_test.get_clients: the compositor's own names for windows.
 *
 * The test finds its window by app_id, never through its wl_surface, as a
 * test driving a browser has to. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "e_wl_testkit.h"

#define APP_ID "e.test.client_list"
#define TITLE  "client list under test"
#define W 320
#define H 240

int
main(void)
{
   Tk *tk;
   Tk_Toplevel *top;
   Tk_Client *mine;
   unsigned int first;
   int i;

   tk = tk_connect("test-client-list");

   top = tk_toplevel_new(tk, APP_ID, TITLE, W, H);
   mine = tk_expect(tk, APP_ID);

   if (mine->id == 0)
     tk_fail(tk, "id 0 is reserved for 'no window', so no window may have it");
   if (strcmp(mine->title, TITLE))
     tk_fail(tk, "title is '%s', expected '%s'", mine->title, TITLE);
   if (mine->pid != (int)getpid())
     tk_fail(tk, "pid is %d, expected %d", mine->pid, (int)getpid());
   if ((mine->w != W) || (mine->h != H))
     tk_fail(tk, "frame is %dx%d, expected %dx%d", mine->w, mine->h, W, H);
   if (!(mine->states & WL_TEST_CLIENT_STATE_VISIBLE))
     tk_fail(tk, "a mapped window with a buffer is not reported visible "
                 "(states=0x%x)", mine->states);
   if (mine->parent != 0)
     tk_fail(tk, "a toplevel with no parent reports parent=%u", mine->parent);
   first = mine->id;

   /* A new window is often allocated where a destroyed one was, so its id
    * must not come from the address. */
   for (i = 0; i < 8; i++)
     {
        tk_toplevel_del(top);
        if (tk_find(tk, APP_ID))
          tk_fail(tk, "window %u is still listed after being destroyed", first);
        top = tk_toplevel_new(tk, APP_ID, TITLE, W, H);
        mine = tk_expect(tk, APP_ID);
        if (mine->id <= first)
          tk_fail(tk, "a new window got id %u, not newer than %u",
                  mine->id, first);
        first = mine->id;
     }

   printf("test-client-list: ok (id=%u %dx%d+%d+%d states=0x%x)\n",
          mine->id, mine->w, mine->h, mine->x, mine->y, mine->states);

   tk_disconnect(tk);
   return 0;
}
