#ifndef IMFOS_V4L_H_
#define IMFOS_V4L_H_

#include "e_mod_main.h"

void imfos_v4l_init(void);
void imfos_v4l_shutdown(void);
void imfos_v4l_run(Imfos_Device *dev);
void imfos_v4l_clean(Imfos_Device *dev);

#endif /* IMFOS_V4L_H_ */
