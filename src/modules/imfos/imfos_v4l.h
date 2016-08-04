#ifndef IMFOS_V4L_H_
#define IMFOS_V4L_H_
#include "imfos_face.h"

typedef struct Imfos_V4l_Conf_
{
   double timeout;
   Eina_Bool invert : 1;
} Imfos_V4l_Conf;

void imfos_v4l_init(void);
void imfos_v4l_shutdown(void);
void imfos_v4l_run(Imfos_V4l_Conf *conf);

#endif /* IMFOS_V4L_H_ */
