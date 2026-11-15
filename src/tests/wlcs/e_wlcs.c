/* wlcs integration for Enlightenment.
 *
 * wlcs (https://github.com/canonical/wlcs) is a third-party conformance suite
 * that any compositor can adopt by providing this shared library. The runner
 * dlopen's it, looks up wlcs_server_integration, and drives the compositor
 * through the hooks below.
 *
 * Subprocess, not in-process
 * --------------------------
 * wlcs calls create_server/start/stop/destroy_server once per *test case*.
 * Mir, KWin and Weston all run their integrations in-process because those
 * compositors are built as libraries with a thin main(); they construct and
 * tear down a compositor per test. E is a single large main() with
 * process-global state throughout and cannot be initialised repeatedly, so we
 * fork and exec a real Enlightenment instead: the one of the build tree this
 * integration was built in, started through src/tests/e-test-compositor.
 *
 * That fits the contract better than it might sound: wlcs requires start() to
 * not block until the mainloop exits (fork+exec is naturally non-blocking) and
 * requires stop() to block until teardown is complete (waitpid is naturally
 * blocking). And create_client_socket only has to hand back something
 * wl_display_connect_fd can use, which a plain connect() to the compositor's
 * listening socket satisfies.
 *
 * The part that needs the test module
 * -----------------------------------
 * position_window_absolute is handed a *client-side* wl_surface belonging to a
 * connection that is not ours, and has to ask the compositor to move it. We
 * cannot look that up from here. Instead we bind the private wl_test interface
 * on that same client's wl_display - so the object is valid on the connection
 * we send it over - and let the compositor resolve it. See
 * src/protocol/wl-test.xml.
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <wayland-client.h>
#include <wlcs/display_server.h>
#include <wlcs/pointer.h>
#include <wlcs/touch.h>

#include "wl-test-client-protocol.h"

#define UNUSED __attribute__((unused))

#define STARTUP_TIMEOUT_MS 20000
#define STOP_TIMEOUT_MS 5000

/* wlcs drives a single touch point. Any positive id will do: E carries touch
 * as evas multi-touch, where device 0 is the pointer. */
#define TOUCH_ID 1

typedef struct
{
   WlcsDisplayServer base;

   /* Our own connection, used for input injection. Input is not associated
    * with any particular client, so it cannot ride on a test client's
    * connection the way position_window_absolute does. */
   struct wl_display *ctrl;
   struct wl_registry *ctrl_registry;
   struct wl_test *ctrl_test;

   pid_t pid;
   char runtime_dir[PATH_MAX];
   char socket_path[PATH_MAX];
} E_Server;

typedef struct
{
   WlcsPointer base;
   E_Server *server;
   int x, y;
} E_Pointer;

typedef struct
{
   WlcsTouch base;
   E_Server *server;
} E_Touch;

static void
_msleep(int ms)
{
   struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };

   nanosleep(&ts, NULL);
}

static void
_registry_global(void *data, struct wl_registry *reg, uint32_t id,
                 const char *iface, uint32_t version UNUSED)
{
   struct wl_test **out = data;

   if ((!strcmp(iface, "wl_test")) && (!*out))
     *out = wl_registry_bind(reg, id, &wl_test_interface, 1);
}

static void
_registry_global_remove(void *data UNUSED, struct wl_registry *reg UNUSED,
                        uint32_t id UNUSED)
{
}

static const struct wl_registry_listener _registry_listener =
{
   _registry_global, _registry_global_remove
};

static int
_connect_fd(const char *path)
{
   struct sockaddr_un addr;
   int fd;

   fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
   if (fd < 0) return -1;

   memset(&addr, 0, sizeof(addr));
   addr.sun_family = AF_UNIX;
   if (snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path) >=
       (int)sizeof(addr.sun_path))
     {
        fprintf(stderr, "e_wlcs: socket path too long for a unix socket: %s\n",
                path);
        close(fd);
        return -1;
     }

   if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
     {
        close(fd);
        return -1;
     }
   return fd;
}

/* E picks its own socket name and never exports WAYLAND_DISPLAY. Alone in a
 * private runtime dir, whatever wayland-* socket appears there is its. */
static int
_socket_find(E_Server *s)
{
   DIR *dir;
   struct dirent *ent;
   int found = 0;

   dir = opendir(s->runtime_dir);
   if (!dir) return 0;

   while ((ent = readdir(dir)))
     {
        struct stat st;
        char path[PATH_MAX];

        if (strncmp(ent->d_name, "wayland-", 8)) continue;
        if (snprintf(path, sizeof(path), "%s/%s", s->runtime_dir,
                     ent->d_name) >= (int)sizeof(path))
          continue;
        if ((stat(path, &st) < 0) || (!S_ISSOCK(st.st_mode))) continue;
        memcpy(s->socket_path, path, sizeof(path));
        found = 1;
        break;
     }

   closedir(dir);
   return found;
}

static int
_wait_for_socket(E_Server *s)
{
   int waited = 0;

   while (waited < STARTUP_TIMEOUT_MS)
     {
        if (waitpid(s->pid, NULL, WNOHANG) == s->pid)
          {
             fprintf(stderr, "e_wlcs: compositor exited during startup\n");
             s->pid = 0;
             return -1;
          }

        /* The socket file appears at bind() time, a moment before listen(),
         * so only a successful connect says the compositor is serving. */
        if (_socket_find(s))
          {
             int fd = _connect_fd(s->socket_path);

             if (fd >= 0)
               {
                  close(fd);
                  return 0;
               }
          }

        _msleep(50);
        waited += 50;
     }

   fprintf(stderr, "e_wlcs: no usable socket in %s after %dms\n",
           s->runtime_dir, STARTUP_TIMEOUT_MS);
   return -1;
}

static void
_server_start(WlcsDisplayServer *server)
{
   E_Server *s = (E_Server *)server;
   const char *tmp = getenv("TMPDIR");
   pid_t pid;

   if (!tmp) tmp = "/tmp";
   snprintf(s->runtime_dir, sizeof(s->runtime_dir), "%s/e-wlcs-XXXXXX", tmp);
   if (!mkdtemp(s->runtime_dir))
     {
        fprintf(stderr, "e_wlcs: mkdtemp failed: %s\n", strerror(errno));
        s->runtime_dir[0] = 0;
        return;
     }

   pid = fork();
   if (pid < 0)
     {
        fprintf(stderr, "e_wlcs: fork failed: %s\n", strerror(errno));
        return;
     }

   if (pid == 0)
     {
        /* A process group of its own, so stop() takes down E and whatever it
         * forks in one go. */
        setpgid(0, 0);

        /* The compositor's own chatter would otherwise be interleaved with
         * the test report. Keep it unless someone asks for it. */
        if (!getenv("E_WLCS_VERBOSE"))
          {
             int devnull = open("/dev/null", O_WRONLY);

             if (devnull >= 0)
               {
                  dup2(devnull, STDOUT_FILENO);
                  dup2(devnull, STDERR_FILENO);
                  if (devnull > STDERR_FILENO) close(devnull);
               }
          }

        execl("/bin/sh", "sh", E_WLCS_COMPOSITOR, s->runtime_dir, (char *)NULL);
        _exit(127);
     }

   s->pid = pid;

   if (_wait_for_socket(s) < 0)
     {
        if (s->pid)
          {
             kill(-s->pid, SIGKILL);
             waitpid(s->pid, NULL, 0);
             s->pid = 0;
          }
        return;
     }

   s->ctrl = wl_display_connect_to_fd(_connect_fd(s->socket_path));
   if (!s->ctrl)
     {
        fprintf(stderr, "e_wlcs: could not open the control connection\n");
        return;
     }
   s->ctrl_registry = wl_display_get_registry(s->ctrl);
   wl_registry_add_listener(s->ctrl_registry, &_registry_listener,
                            &s->ctrl_test);
   wl_display_roundtrip(s->ctrl);

   if (!s->ctrl_test)
     fprintf(stderr, "e_wlcs: no wl_test global - was E built with "
                     "-Dtests=true?\n");
}

/* Whether /proc/<pid>/environ holds exactly the given NAME=value entry. */
static int
_environ_has(long pid, const char *want)
{
   char path[64], buf[4096];
   size_t want_len = strlen(want), have = 0;
   ssize_t n;
   int fd, hit = 0, skipping = 0;

   snprintf(path, sizeof(path), "/proc/%ld/environ", pid);
   fd = open(path, O_RDONLY);
   if (fd < 0) return 0;

   /* Entries are NUL terminated and the environment can be far larger than
    * any buffer, so match one entry at a time as it streams past. */
   while ((!hit) && ((n = read(fd, buf + have, sizeof(buf) - have)) > 0))
     {
        size_t len = have + n, start = 0, i;

        for (i = 0; i < len; i++)
          {
             if (buf[i]) continue;
             if ((!skipping) && (i - start == want_len) &&
                 (!memcmp(buf + start, want, want_len)))
               {
                  hit = 1;
                  break;
               }
             skipping = 0;
             start = i + 1;
          }
        if (hit) break;

        have = len - start;
        if (have == sizeof(buf))
          {
             /* An entry longer than the buffer cannot be ours. */
             skipping = 1;
             have = 0;
          }
        else
          memmove(buf, buf + start, have);
     }

   close(fd);
   return hit;
}

/* Count - and optionally signal - every process still living in our private
 * runtime dir, identified by its HOME.
 *
 * The process group is not enough on its own: E's helpers (efreetd and
 * friends) detach into a session of their own, so kill(-pgid) misses them.
 * They do exit by themselves, but slowly, and on the way out they recreate
 * the cache tree we are trying to delete. The path is a fresh mkdtemp per
 * server, so matching HOME against it cannot hit anything else.
 */
static int
_procs_in_runtime_dir(E_Server *s, int sig)
{
   DIR *proc;
   struct dirent *ent;
   char want[PATH_MAX + 8];
   int count = 0;

   snprintf(want, sizeof(want), "HOME=%s", s->runtime_dir);

   proc = opendir("/proc");
   if (!proc) return 0;

   while ((ent = readdir(proc)))
     {
        long pid = strtol(ent->d_name, NULL, 10);

        if (pid <= 0) continue;
        if (!_environ_has(pid, want)) continue;

        count++;
        if (sig) kill((pid_t)pid, sig);
     }

   closedir(proc);
   return count;
}

static int
_remove_entry(const char *path, const struct stat *st UNUSED,
              int type UNUSED, struct FTW *ftw UNUSED)
{
   remove(path);
   return 0;
}

static void
_runtime_dir_remove(E_Server *s)
{
   struct stat st;
   int i;

   _procs_in_runtime_dir(s, SIGTERM);
   for (i = 0; i < 50; i++)
     {
        if (!_procs_in_runtime_dir(s, 0)) break;
        _msleep(100);
     }
   if (_procs_in_runtime_dir(s, SIGKILL))
     {
        for (i = 0; i < 20; i++)
          {
             if (!_procs_in_runtime_dir(s, 0)) break;
             _msleep(100);
          }
     }

   /* Check the directory is gone rather than trusting the walk: a helper
    * winding down can put part of it back right behind us. */
   for (i = 0; i < 20; i++)
     {
        nftw(s->runtime_dir, _remove_entry, 16, FTW_DEPTH | FTW_PHYS);
        if (stat(s->runtime_dir, &st) != 0) break;
        _msleep(100);
     }
   if (stat(s->runtime_dir, &st) == 0)
     fprintf(stderr, "e_wlcs: could not remove %s\n", s->runtime_dir);
}

static void
_server_stop(WlcsDisplayServer *server)
{
   E_Server *s = (E_Server *)server;
   int waited = 0;

   if (s->ctrl)
     {
        wl_display_disconnect(s->ctrl);
        s->ctrl = NULL;
        s->ctrl_registry = NULL;
        s->ctrl_test = NULL;
     }

   /* wlcs requires this to block until teardown is complete, so that one
    * test's compositor cannot leak into the next. */
   if (s->pid)
     {
        kill(-s->pid, SIGTERM);
        while (waited < STOP_TIMEOUT_MS)
          {
             if (waitpid(s->pid, NULL, WNOHANG) == s->pid) break;
             _msleep(50);
             waited += 50;
          }

        /* Whatever in the group outlived the compositor, and the compositor
         * itself if it would not go. */
        kill(-s->pid, SIGKILL);
        if (waited >= STOP_TIMEOUT_MS) waitpid(s->pid, NULL, 0);
        s->pid = 0;
     }

   if (s->runtime_dir[0])
     {
        if (!getenv("E_WLCS_KEEP")) _runtime_dir_remove(s);
        s->runtime_dir[0] = 0;
     }
}

static int
_server_create_client_socket(WlcsDisplayServer *server)
{
   return _connect_fd(((E_Server *)server)->socket_path);
}

static void
_server_position_window_absolute(WlcsDisplayServer *server UNUSED,
                                 struct wl_display *client,
                                 struct wl_surface *surface, int x, int y)
{
   struct wl_event_queue *queue;
   struct wl_display *wrapper;
   struct wl_registry *registry;
   struct wl_test *tester = NULL;

   /* The surface belongs to this client, not to us, so bind wl_test on the
    * client's own connection: the object reference is then valid on the
    * connection we send it over.
    *
    * Everything goes on a private event queue via a proxy wrapper. Without
    * that, our roundtrip would dispatch - and swallow - events the test
    * itself is waiting for on the default queue. */
   queue = wl_display_create_queue(client);
   if (!queue) return;

   wrapper = wl_proxy_create_wrapper(client);
   if (!wrapper)
     {
        wl_event_queue_destroy(queue);
        return;
     }
   wl_proxy_set_queue((struct wl_proxy *)wrapper, queue);

   registry = wl_display_get_registry(wrapper);
   wl_proxy_wrapper_destroy(wrapper);
   if (!registry)
     {
        wl_event_queue_destroy(queue);
        return;
     }

   wl_registry_add_listener(registry, &_registry_listener, &tester);
   wl_display_roundtrip_queue(client, queue);

   if (tester)
     {
        wl_test_move_surface(tester, surface, x, y);
        /* wl_test.sync, not wl_display.sync: it answers from an ecore job, so
         * the move has been applied by the time we return. */
        wl_test_sync(tester);
        wl_display_roundtrip_queue(client, queue);
        wl_test_destroy(tester);
     }
   else
     fprintf(stderr, "e_wlcs: no wl_test on the client connection; "
                     "cannot position window\n");

   wl_registry_destroy(registry);
   wl_event_queue_destroy(queue);
}

/* Posted input is dispatched by the compositor's main loop, so wait for
 * wl_test.sync rather than wl_display.sync before the test looks for it. */
static void
_ctrl_sync(E_Server *s)
{
   wl_test_sync(s->ctrl_test);
   wl_display_roundtrip(s->ctrl);
}

static void
_pointer_move_absolute(WlcsPointer *pointer, wl_fixed_t x, wl_fixed_t y)
{
   E_Pointer *p = (E_Pointer *)pointer;

   p->x = wl_fixed_to_int(x);
   p->y = wl_fixed_to_int(y);
   if (!p->server->ctrl_test) return;
   wl_test_pointer_warp(p->server->ctrl_test, p->x, p->y);
   _ctrl_sync(p->server);
}

static void
_pointer_move_relative(WlcsPointer *pointer, wl_fixed_t dx, wl_fixed_t dy)
{
   E_Pointer *p = (E_Pointer *)pointer;

   /* The compositor only takes absolute positions, so the current location is
    * tracked here. */
   p->x += wl_fixed_to_int(dx);
   p->y += wl_fixed_to_int(dy);
   if (!p->server->ctrl_test) return;
   wl_test_pointer_warp(p->server->ctrl_test, p->x, p->y);
   _ctrl_sync(p->server);
}

static void
_pointer_button(E_Pointer *p, int button, int pressed)
{
   if (!p->server->ctrl_test) return;
   wl_test_pointer_button(p->server->ctrl_test, (uint32_t)button, pressed);
   _ctrl_sync(p->server);
}

static void
_pointer_button_up(WlcsPointer *pointer, int button)
{
   _pointer_button((E_Pointer *)pointer, button, 0);
}

static void
_pointer_button_down(WlcsPointer *pointer, int button)
{
   _pointer_button((E_Pointer *)pointer, button, 1);
}

static void
_pointer_destroy(WlcsPointer *pointer)
{
   free(pointer);
}

static WlcsPointer *
_server_create_pointer(WlcsDisplayServer *server)
{
   E_Pointer *p = calloc(1, sizeof(*p));

   if (!p) return NULL;
   p->server = (E_Server *)server;
   p->base.version = WLCS_POINTER_VERSION;
   p->base.move_absolute = _pointer_move_absolute;
   p->base.move_relative = _pointer_move_relative;
   p->base.button_up = _pointer_button_up;
   p->base.button_down = _pointer_button_down;
   p->base.destroy = _pointer_destroy;
   return &p->base;
}

/* <wlcs/touch.h> declares the coordinates wl_fixed_t, but the runner passes
 * plain pixels: wlcs::Touch::down_at(int x, int y) hands them on unconverted,
 * unlike Pointer::move_to. Checked in v1.7.0. The wlcs driver test fails if
 * that ever changes, as every touch would land 256 times too close to 0,0. */
static void
_touch_down(WlcsTouch *touch, wl_fixed_t x, wl_fixed_t y)
{
   E_Touch *t = (E_Touch *)touch;

   if (!t->server->ctrl_test) return;
   wl_test_touch_down(t->server->ctrl_test, TOUCH_ID, x, y);
   _ctrl_sync(t->server);
}

static void
_touch_move(WlcsTouch *touch, wl_fixed_t x, wl_fixed_t y)
{
   E_Touch *t = (E_Touch *)touch;

   if (!t->server->ctrl_test) return;
   wl_test_touch_move(t->server->ctrl_test, TOUCH_ID, x, y);
   _ctrl_sync(t->server);
}

static void
_touch_up(WlcsTouch *touch)
{
   E_Touch *t = (E_Touch *)touch;

   if (!t->server->ctrl_test) return;
   wl_test_touch_up(t->server->ctrl_test, TOUCH_ID);
   _ctrl_sync(t->server);
}

static void
_touch_destroy(WlcsTouch *touch)
{
   free(touch);
}

static WlcsTouch *
_server_create_touch(WlcsDisplayServer *server)
{
   E_Touch *t = calloc(1, sizeof(*t));

   if (!t) return NULL;
   t->server = (E_Server *)server;
   t->base.version = WLCS_TOUCH_VERSION;
   t->base.touch_down = _touch_down;
   t->base.touch_move = _touch_move;
   t->base.touch_up = _touch_up;
   t->base.destroy = _touch_destroy;
   return &t->base;
}

/* What we tell wlcs we support.
 *
 * The header suggests this makes wlcs skip whole groups of tests. Checked
 * against wlcs 1.7.0's own source, that is not what happens - there is even a
 * TODO in bind_if_supported() proposing it. What the list actually drives is:
 *
 *   - version negotiation. bind_if_supported() takes the version from here
 *     when binding a global, so declaring a version *lower* than E advertises
 *     silently makes every test exercise the older protocol;
 *   - shell selection: the_wl_shell() and the_xdg_shell_v6() consult it, and
 *     tests needing a shell we do not declare raise
 *     ExtensionExpectedlyNotSupported rather than a hard failure.
 *
 * Everything else runs regardless of what is listed, which is why
 * e_wlcs_driver checks this list against the compositor's real registry
 * instead of trusting it.
 */
static const WlcsExtensionDescriptor _extensions[] =
{
   { "wl_compositor", 4 },
   { "wl_subcompositor", 1 },
   { "wl_shm", 1 },
   { "wl_seat", 4 },
   { "wl_output", 2 },
   { "wl_data_device_manager", 3 },
   { "xdg_wm_base", 1 },
   { "zxdg_shell_v6", 1 },
   { "wl_shell", 1 },
};

static const WlcsIntegrationDescriptor _descriptor =
{
   1,
   sizeof(_extensions) / sizeof(_extensions[0]),
   _extensions
};

static const WlcsIntegrationDescriptor *
_server_get_descriptor(const WlcsDisplayServer *server UNUSED)
{
   return &_descriptor;
}

static WlcsDisplayServer *
_create_server(int argc UNUSED, const char **argv UNUSED)
{
   E_Server *s;

   s = calloc(1, sizeof(*s));
   if (!s) return NULL;

   s->base.version = 3;
   s->base.start = _server_start;
   s->base.stop = _server_stop;
   s->base.create_client_socket = _server_create_client_socket;
   s->base.position_window_absolute = _server_position_window_absolute;
   s->base.create_pointer = _server_create_pointer;
   s->base.create_touch = _server_create_touch;
   s->base.get_descriptor = _server_get_descriptor;
   /* start_on_this_thread is deliberately NULL: it is for compositors whose
    * mainloop must own the calling thread, which only helps an in-process
    * integration. wlcs requires one of {start, start_on_this_thread} and
    * prefers start when both are present. */
   s->base.start_on_this_thread = NULL;

   return &s->base;
}

static void
_destroy_server(WlcsDisplayServer *server)
{
   E_Server *s = (E_Server *)server;

   if ((s->pid) || (s->runtime_dir[0])) _server_stop(server);
   free(s);
}

WlcsServerIntegration const wlcs_server_integration =
{
   1,
   _create_server,
   _destroy_server,
};
