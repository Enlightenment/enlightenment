#ifndef EDBUS_UTILS_H
#define EDBUS_UTILS_H 1

typedef struct _EDBus_Error_Info
{
   const char *error;
   const char *message;
} EDBus_Error_Info;

typedef void (*EDBus_Codegen_Property_Set_Cb)(void *data, const char *propname, EDBus_Proxy *proxy, EDBus_Pending *p, EDBus_Error_Info *error_info);

typedef void (*EDBus_Codegen_Property_String_Get_Cb)(void *data, EDBus_Pending *p, const char *propname, EDBus_Proxy *proxy, EDBus_Error_Info *error_info, const char *value);
typedef void (*EDBus_Codegen_Property_Int32_Get_Cb)(void *data, EDBus_Pending *p, const char *propname, EDBus_Proxy *proxy, EDBus_Error_Info *error_info, int value);
typedef void (*EDBus_Codegen_Property_Byte_Get_Cb)(void *data, EDBus_Pending *p, const char *propname, EDBus_Proxy *proxy, EDBus_Error_Info *error_info, unsigned char value);
typedef void (*EDBus_Codegen_Property_Bool_Get_Cb)(void *data, EDBus_Pending *p, const char *propname, EDBus_Proxy *proxy, EDBus_Error_Info *error_info, Eina_Bool value);
typedef void (*EDBus_Codegen_Property_Int16_Get_Cb)(void *data, EDBus_Pending *p, const char *propname, EDBus_Proxy *proxy, EDBus_Error_Info *error_info, short int value);
typedef void (*EDBus_Codegen_Property_Uint16_Get_Cb)(void *data, EDBus_Pending *p, const char *propname, EDBus_Proxy *proxy, EDBus_Error_Info *error_info, unsigned short int value);
typedef void (*EDBus_Codegen_Property_Uint32_Get_Cb)(void *data, EDBus_Pending *p, const char *propname, EDBus_Proxy *proxy, EDBus_Error_Info *error_info, unsigned int value);
typedef void (*EDBus_Codegen_Property_Double_Get_Cb)(void *data, EDBus_Pending *p, const char *propname, EDBus_Proxy *proxy, EDBus_Error_Info *error_info, double value);
typedef void (*EDBus_Codegen_Property_Int64_Get_Cb)(void *data, EDBus_Pending *p, const char *propname, EDBus_Proxy *proxy, EDBus_Error_Info *error_info, int64_t value);
typedef void (*EDBus_Codegen_Property_Uint64_Get_Cb)(void *data, EDBus_Pending *p, const char *propname, EDBus_Proxy *proxy, EDBus_Error_Info *error_info, uint64_t value);
typedef void (*EDBus_Codegen_Property_Complex_Get_Cb)(void *data, EDBus_Pending *p, const char *propname, EDBus_Proxy *proxy, EDBus_Error_Info *error_info, Eina_Value *value);

#endif