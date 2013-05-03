#ifdef E_TYPEDEFS

typedef struct _E_Shell_Interface E_Shell_Interface;

#else
# ifndef E_SHELL_H
#  define E_SHELL_H

struct _E_Shell_Interface
{
   void *shell;

   /* TODO: surface_create */
};

EINTERN int e_shell_init(void);
EINTERN int e_shell_shutdown(void);

# endif
#endif
