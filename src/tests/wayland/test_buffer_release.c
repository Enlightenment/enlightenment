/* A released shm buffer may be destroyed at once, and the compositor must
 * not crash for it.
 *
 * Every commit of a new buffer releases the previous one, and the test kit
 * destroys a buffer as soon as it is released, which unmaps its pool. If
 * that happens while a render is reading the buffer from a worker thread,
 * the compositor crashes. Committing buffers of changing sizes, frame after
 * frame, makes the overlap likely. */
#include <stdio.h>

#include "e_wl_testkit.h"

#define APP_ID "e.test.buffer_release"
#define FRAMES 400

int
main(void)
{
   Tk *tk;
   Tk_Toplevel *top;
   int i;

   tk = tk_connect("test-buffer-release");
   top = tk_toplevel_new(tk, APP_ID, "buffer release", 320, 240);

   for (i = 0; i < FRAMES; i++)
     {
        tk_toplevel_paint(top, 320 + (i % 64) * 8, 240 + (i % 48) * 8);
        tk_sync(tk);
     }

   tk_expect(tk, APP_ID);
   printf("test-buffer-release: ok (%d frames)\n", FRAMES);

   tk_disconnect(tk);
   return 0;
}
