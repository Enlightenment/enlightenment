#ifdef E_TYPEDEFS

typedef struct _E_Shader E_Shader;

#else
# ifndef E_SHADER_H
#  define E_SHADER_H

struct _E_Shader
{
#  ifdef HAVE_WAYLAND_EGL
   GLuint program;
   struct
     {
        GLuint vertext, fragment;
     } shader;
   struct 
     {
        GLint proj, tex[3];
        GLint alpha, color;
     } uniform;
   struct 
     {
        const char *vertext, *fragment;
     } source;
#  endif
};

# endif
#endif
