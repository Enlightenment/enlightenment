/* A client that shrinks the file behind its wl_shm pool, after creating a
 * buffer in it, must not take the compositor down when the buffer is
 * drawn. It gets wl_shm.error.invalid_fd on that wl_buffer, and other
 * clients carry on. */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "e_wl_testkit.h"

#define PROG "test-shm-truncate"
#define APP_ID "e.test.shm_truncate"
#define W 200
#define H 150

static void
_frame_done(void *data, struct wl_callback *cb, uint32_t t)
{
   int *done = data;

   (void)t;
   wl_callback_destroy(cb);
   *done = 1;
}

static const struct wl_callback_listener _frame_listener = { _frame_done };

/* A buffer whose pool's file is emptied right after the buffer is made. */
static struct wl_buffer *
_truncated_buffer_make(Tk *tk)
{
   struct wl_shm *shm = tk_bind(tk, &wl_shm_interface, 1);
   struct wl_shm_pool *pool;
   struct wl_buffer *buffer;
   size_t size = W * H * 4;
   int fd;

   fd = memfd_create(PROG, MFD_CLOEXEC);
   if (fd < 0) tk_fail(tk, "memfd_create: %m");
   if (ftruncate(fd, size) < 0) tk_fail(tk, "ftruncate: %m");
   pool = wl_shm_create_pool(shm, fd, size);
   buffer = wl_shm_pool_create_buffer(pool, 0, W, H, W * 4,
                                      WL_SHM_FORMAT_ARGB8888);
   wl_shm_pool_destroy(pool);
   wl_display_roundtrip(tk_display(tk));
   if (ftruncate(fd, 0) < 0) tk_fail(tk, "ftruncate: %m");
   close(fd);
   return buffer;
}

int
main(void)
{
   Tk *tk, *other;
   Tk_Toplevel *top;
   struct wl_surface *surface;
   struct wl_buffer *buffer;
   const struct wl_interface *iface = NULL;
   int frames, done, code;

   tk = tk_connect(PROG);
   top = tk_toplevel_new(tk, APP_ID, "shm truncate", W, H);
   tk_expect(tk, APP_ID);
   surface = tk_toplevel_surface(top);

   buffer = _truncated_buffer_make(tk);
   wl_surface_attach(surface, buffer, 0, 0);
   /* A frame callback can be answered by a render that was already under
    * way when the commit arrived, so a frame done does not prove the
    * buffer was read: draw it again until the error or the frames run
    * out. */
   for (frames = 0; frames < 5; frames++)
     {
        done = 0;
        wl_surface_damage(surface, 0, 0, W, H);
        wl_callback_add_listener(wl_surface_frame(surface), &_frame_listener,
                                 &done);
        wl_surface_commit(surface);
        while (!done)
          if (wl_display_dispatch(tk_display(tk)) < 0) break;
        if (!done) break;
     }

   if (done)
     tk_fail(tk, "a buffer whose file was truncated was drawn %d times with "
             "no error", frames);
   code = tk_protocol_error(tk, &iface);
   if (code < 0)
     {
        fprintf(stderr, PROG ": the connection ended without a protocol "
                "error: %s\n", strerror(wl_display_get_error(tk_display(tk))));
        return 1;
     }
   if ((iface != &wl_buffer_interface) || (code != WL_SHM_ERROR_INVALID_FD))
     {
        fprintf(stderr, PROG ": expected wl_shm.error.invalid_fd on the "
                "wl_buffer, got error %d on %s\n", code,
                iface ? iface->name : "nothing");
        return 1;
     }

   other = tk_connect(PROG);
   tk_toplevel_new(other, APP_ID ".other", "still here", W, H);
   tk_expect(other, APP_ID ".other");

   printf(PROG ": ok\n");
   tk_disconnect(other);
   return 0;
}
