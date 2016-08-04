#ifndef _E_MOD_MAIN_H
#define _E_MOD_MAIN_H

#ifdef _cplusplus
extern "C++"
#endif
int imfos_face_search(char *data, int width, int height, int stride);

enum _Imfos_Orient
{
   IMFOS_ORIENT_DEFAULT,
   IMFOS_ORIENT_90,
   IMFOS_ORIENT_180,
   IMFOS_ORIENT_270
};



typedef struct _Config_Item Config_Item;
typedef struct _Imfos_Config Imfos_Config;
typedef enum _Imfos_Orient Imfos_Orient;

struct _Imfos_Config
{
   Imfos_Orient orient;
   //E_Config_Dialog *cfd;
};

struct _Config_Item {
     Imfos_Orient orient;
     const char *path;
};

extern Imfos_Config *imfos_config;

#endif /* _E_MOD_MAIN_H */
