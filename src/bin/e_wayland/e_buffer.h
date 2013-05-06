#ifdef E_TYPEDEFS

typedef struct _E_Buffer_Reference E_Buffer_Reference;

#else
# ifndef E_BUFFER_H
#  define E_BUFFER_H

struct _E_Buffer_Reference
{
   struct wl_buffer *buffer;
   struct wl_listener buffer_destroy;
};

EAPI void e_buffer_reference(E_Buffer_Reference *br, struct wl_buffer *buffer);

# endif
#endif
