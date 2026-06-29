#ifndef L_H
#define L_H

#include <stdio.h>

#include <Ecore.h>

#define L(fmt, args...) \
   do { \
     fprintf(stderr, "[%1.5f] " fmt "\n", ecore_time_get(), ##args); \
   } while (0)

#endif
