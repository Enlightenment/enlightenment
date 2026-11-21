#define E_COMP_WL
#include "e.h"

#include <signal.h>
#include <sys/mman.h>

/* A client can shrink the file behind its wl_shm pool at any time, and
 * reading a page of the mapping past the new end of the file raises
 * SIGBUS. Evas reads shm buffers from its render threads, after the
 * commit that attached them, so the reads cannot be bracketed with
 * wl_shm_buffer_begin_access(), which only covers the calling thread.
 *
 * Instead, as wlroots does, every buffer e_pixmap holds for rendering is
 * recorded here for as long as it holds the buffer's pool. A fault inside
 * one gets a page of zeroes mapped over the faulting page, so the render
 * reads garbage instead of the compositor dying, and the client gets
 * wl_shm.error.invalid_fd on that wl_buffer once the render is over, as
 * wlcs's BadBufferTest.test_truncated_shm_file expects.
 *
 * The handler runs on whichever thread faulted, so the records are only
 * touched under _shm_lock, and nothing reads pool memory while holding it.
 * Any other fault is passed to the handler installed before this one. */

typedef struct
{
   uintptr_t start, end;
   struct wl_resource *buffer;
   struct wl_listener buffer_destroy;
   Eina_Bool faulted;
} E_Comp_Wl_Shm_Range;

typedef struct
{
   struct wl_shm_pool *pool;
   unsigned int refs;
   Eina_List *ranges;
} E_Comp_Wl_Shm_Pool_Guard;

static Eina_Spinlock _shm_lock;
static Eina_List *_shm_pool_guards = NULL;
static struct sigaction _shm_prev_sigbus;
static uintptr_t _shm_page_size = 0;
static Eina_Bool _shm_handler_installed = EINA_FALSE;

static Eina_Bool
_e_comp_wl_shm_fault_absorb(uintptr_t addr)
{
   E_Comp_Wl_Shm_Pool_Guard *guard;
   E_Comp_Wl_Shm_Range *range;
   Eina_List *l, *ll;
   Eina_Bool absorbed = EINA_FALSE;
   void *page;

   eina_spinlock_take(&_shm_lock);
   EINA_LIST_FOREACH(_shm_pool_guards, l, guard)
     EINA_LIST_FOREACH(guard->ranges, ll, range)
       {
          if ((addr < range->start) || (addr >= range->end)) continue;
          page = (void *)(addr & ~(_shm_page_size - 1));
          if (mmap(page, _shm_page_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) != MAP_FAILED)
            {
               range->faulted = EINA_TRUE;
               absorbed = EINA_TRUE;
            }
          goto done;
       }
done:
   eina_spinlock_release(&_shm_lock);
   return absorbed;
}

static void
_e_comp_wl_shm_sigbus(int sig, siginfo_t *info, void *context)
{
   struct sigaction dfl;
   int saved_errno = errno;

   if ((info->si_code == BUS_ADRERR) &&
       _e_comp_wl_shm_fault_absorb((uintptr_t)info->si_addr))
     {
        errno = saved_errno;
        return;
     }
   errno = saved_errno;

   if (_shm_prev_sigbus.sa_flags & SA_SIGINFO)
     {
        _shm_prev_sigbus.sa_sigaction(sig, info, context);
        return;
     }
   if (_shm_prev_sigbus.sa_handler == SIG_IGN) return;
   if (_shm_prev_sigbus.sa_handler != SIG_DFL)
     {
        _shm_prev_sigbus.sa_handler(sig);
        return;
     }
   /* Returning re-runs the faulting access, which now gets the default. */
   memset(&dfl, 0, sizeof(dfl));
   dfl.sa_handler = SIG_DFL;
   sigaction(SIGBUS, &dfl, NULL);
}

static Eina_Bool
_e_comp_wl_shm_handler_install(void)
{
   struct sigaction sa;

   if (_shm_handler_installed) return EINA_TRUE;

   /* eina_mmap's handler aborts on a fault outside its own files without
    * passing it on, so it has to be the one this handler passes faults to,
    * never the one installed over it. It is installed the first time EFL
    * maps a file; make sure that has happened. */
   eina_mmap_safety_enabled_set(EINA_TRUE);

   if (!eina_spinlock_new(&_shm_lock)) return EINA_FALSE;
   _shm_page_size = (uintptr_t)sysconf(_SC_PAGESIZE);

   memset(&sa, 0, sizeof(sa));
   sa.sa_sigaction = _e_comp_wl_shm_sigbus;
   sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESTART;
   sigemptyset(&sa.sa_mask);
   if (sigaction(SIGBUS, &sa, &_shm_prev_sigbus))
     {
        ERR("Cannot install a SIGBUS handler for shm buffers: %m");
        eina_spinlock_free(&_shm_lock);
        return EINA_FALSE;
     }
   _shm_handler_installed = EINA_TRUE;
   return EINA_TRUE;
}

static void
_e_comp_wl_shm_cb_buffer_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Comp_Wl_Shm_Range *range;

   range = container_of(listener, E_Comp_Wl_Shm_Range, buffer_destroy);
   range->buffer = NULL;
   wl_list_remove(&listener->link);
   wl_list_init(&listener->link);
}

static void
_e_comp_wl_shm_fault_post(E_Comp_Wl_Shm_Range *range)
{
   if (!range->faulted) return;
   range->faulted = EINA_FALSE;
   if (!range->buffer)
     {
        ERR("A client shrank the file behind a shm buffer it has destroyed");
        return;
     }
   wl_resource_post_error(range->buffer, WL_SHM_ERROR_INVALID_FD,
                          "the file behind this buffer's pool was shrunk");
}

static E_Comp_Wl_Shm_Pool_Guard *
_e_comp_wl_shm_pool_guard_find(struct wl_shm_pool *pool)
{
   E_Comp_Wl_Shm_Pool_Guard *guard;
   Eina_List *l;

   EINA_LIST_FOREACH(_shm_pool_guards, l, guard)
     if (guard->pool == pool) return guard;
   return NULL;
}

/* Called with each reference e_pixmap takes on a buffer's pool to render
 * it. The pool keeps the whole mapping alive, so every buffer recorded for
 * it stays covered until the last of those references is dropped. */
EINTERN void
e_comp_wl_shm_guard_add(struct wl_shm_pool *pool, E_Comp_Wl_Buffer *buffer)
{
   E_Comp_Wl_Shm_Pool_Guard *guard;
   E_Comp_Wl_Shm_Range *range;
   struct wl_shm_buffer *shm = buffer->shm_buffer;

   if (!_e_comp_wl_shm_handler_install()) return;

   range = E_NEW(E_Comp_Wl_Shm_Range, 1);
   range->start = (uintptr_t)wl_shm_buffer_get_data(shm);
   range->end = range->start +
     ((size_t)wl_shm_buffer_get_stride(shm) * (size_t)wl_shm_buffer_get_height(shm));
   range->buffer = buffer->resource;
   range->buffer_destroy.notify = _e_comp_wl_shm_cb_buffer_destroy;
   wl_resource_add_destroy_listener(buffer->resource, &range->buffer_destroy);

   eina_spinlock_take(&_shm_lock);
   guard = _e_comp_wl_shm_pool_guard_find(pool);
   if (!guard)
     {
        guard = E_NEW(E_Comp_Wl_Shm_Pool_Guard, 1);
        guard->pool = pool;
        _shm_pool_guards = eina_list_append(_shm_pool_guards, guard);
     }
   guard->refs++;
   guard->ranges = eina_list_append(guard->ranges, range);
   eina_spinlock_release(&_shm_lock);
}

/* Called before each of those references is dropped. */
EINTERN void
e_comp_wl_shm_guard_del(struct wl_shm_pool *pool)
{
   E_Comp_Wl_Shm_Pool_Guard *guard;
   E_Comp_Wl_Shm_Range *range;

   if (!_shm_handler_installed) return;

   eina_spinlock_take(&_shm_lock);
   guard = _e_comp_wl_shm_pool_guard_find(pool);
   if ((!guard) || (--guard->refs))
     {
        eina_spinlock_release(&_shm_lock);
        return;
     }
   _shm_pool_guards = eina_list_remove(_shm_pool_guards, guard);
   eina_spinlock_release(&_shm_lock);

   EINA_LIST_FREE(guard->ranges, range)
     {
        _e_comp_wl_shm_fault_post(range);
        wl_list_remove(&range->buffer_destroy.link);
        free(range);
     }
   free(guard);
}

/* Tells the clients whose buffers faulted during the render just over. */
EINTERN void
e_comp_wl_shm_faults_post(void)
{
   E_Comp_Wl_Shm_Pool_Guard *guard;
   E_Comp_Wl_Shm_Range *range;
   Eina_List *l, *ll;

   if (!_shm_handler_installed) return;

   eina_spinlock_take(&_shm_lock);
   EINA_LIST_FOREACH(_shm_pool_guards, l, guard)
     EINA_LIST_FOREACH(guard->ranges, ll, range)
       _e_comp_wl_shm_fault_post(range);
   eina_spinlock_release(&_shm_lock);
}
