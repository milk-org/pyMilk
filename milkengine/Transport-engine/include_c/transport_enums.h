#ifndef TRANSPORT_ENUMS_H
#define TRANSPORT_ENUMS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    UNINITIALIZED = -2,
    CPU_MEMORY = -1,
    GPU0_MEMORY = 0,
    GPU1_MEMORY = 1,
    GPU2_MEMORY = 2,
    GPU3_MEMORY = 3,
    GPU4_MEMORY = 4,
    GPU5_MEMORY = 5,
    GPU6_MEMORY = 6,
    GPU7_MEMORY = 7
} InternalStorageEnum;

typedef enum
{
    RECVTYPE_A = 0,
    RECVTYPE_ISIO = 1,
    RECVTYPE_UDP = 2,
    RECVTYPE_ZMQSUB = 3,
} TransportTypeRecvEnum;

typedef enum
{
    EMITTYPE_ISIO = 0,
    EMITTYPE_ZMQPUB = 1
} TransportTypeEmitEnum;

#ifdef __cplusplus
}
#endif


// TODO find a more programmatic way to generate elegant C/C++ enums
#ifdef __cplusplus
enum class SyncEnum {
    SUCCESS = 0,
    TIMEOUT = 1,
    FAILED = 2,
    FATAL = -1,
};
#else // #ifdef __cplusplus
typedef enum {
    SyncEnum_SUCCESS = 0,
    SyncEnum_TIMEOUT = 1,
    SyncEnum_FAILED = 2,
    SyncEnum_FATAL = -1,
} SyncEnum;
#endif // #ifdef __cplusplus #else

#endif // #ifndef TRANSPORT_ENUMS_H
