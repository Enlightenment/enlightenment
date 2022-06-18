#ifdef E_TYPEDEFS

#else
#ifndef E_SOUND_H
#define E_SOUND_H
E_API int e_sound_init(void);
E_API int e_sound_shutdown(void);
E_API void e_sound_file_play(const char *file, double vol);
#endif
#endif
