#ifndef TRANSPORT_ENUMS_HPP
#define TRANSPORT_ENUMS_HPP

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    CPU_MEMORY = 0,
    GPU_MEMORY
} InternalStorageEnum;

typedef enum
{
    TYPE_A = 0,
    TYPE_ISIO
} TransportTypeEnum;

#ifdef __cplusplus
}
#endif

#endif
