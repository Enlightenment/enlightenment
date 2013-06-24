#ifdef E_TYPEDEFS

typedef struct _E_Buffer E_Buffer;
typedef struct _E_Buffer_Reference E_Buffer_Reference;

#else
# ifndef E_BUFFER_H
#  define E_BUFFER_H

struct _E_Buffer
{
   struct 
     {
        struct wl_resource *resource;
     } wl;

   struct 
     {
        struct wl_signal destroy;
     } signals;

   struct wl_listener buffer_destroy;

   union
     {
        struct wl_shm_buffer *shm_buffer;
        struct wl_buffer *legacy_buffer;
     };

   Evas_Coord w, h;
   unsigned int busy_count;
};

struct _E_Buffer_Reference
{
   E_Buffer *buffer;
   struct wl_listener buffer_destroy;
};

EAPI E_Buffer *e_buffer_resource_get(struct wl_resource *resource);
EAPI void e_buffer_reference(E_Buffer_Reference *br, E_Buffer *buffer);

# endif
#endif
