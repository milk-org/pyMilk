/**
 * @file    IMGID.h
 * @brief   Image identifying structure
 *
 */

#ifndef IMGID_H
#define IMGID_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "ImageStreamIO/ImageStreamIO.h"
#include "fps_streamname_parse.h"
#include "imgid_slice.h"




#ifdef __cplusplus
typedef const char *CONST_WORD;
#else
typedef const char *__restrict CONST_WORD;
#endif

#ifndef errno_t
typedef int errno_t;
#endif

#define IMGID_NB_KEYWO_MAX 10

#define IMGID_CONNECT_NOCHECK      0
#define IMGID_CONNECT_CHECK_FAIL   1
#define IMGID_CONNECT_CHECK_CREATE 2

#define IMGID_CONNECTED            0
#define IMGID_CREATED              1
#define IMGID_RECREATED            2

#define IMGID_SUCCESS              IMGID_CONNECTED

#define IMGID_ERR_BADNAME          10
#define IMGID_ERR_ALLOCATION       11
#define IMGID_ERR_NOTFOUND         12
#define IMGID_ERR_MISMATCH         13



// Image identifier for internal use by milk
//
// This is the preferred format for handling images in milk as function arguments,
// providing additional information and context for the current process/application.
//
// When used with milk-CLI, the type avoids name-resolving imageID multiple times
// provides quick and convenient access to data and metadata pointers
// Pass this as argument to functions to have both input-by-ID (ID>-1)
// and input-by-name (ID=-1).
typedef struct
{
    imageID ID; // -1 if not resolved
    // If using an array of images (as done by milk-CLI) this is the index
    // of the image in the array.
    // If not using an array of images, ID=0 if the image is loaded in memory.

    // increments when image created, used to check if re-resolving needed
    int64_t createcnt;

    // used to resolve if needed
    char            name[STRINGMAXLEN_IMAGE_NAME];

    // image content, data and metadata
    IMAGE          *im;
    // md points at im.md
    IMAGE_METADATA *md;

    // Requested image params
    // Used to create image or test if existing image matches
    // These fields do not always match the image content
    //
    uint8_t  datatype;
    int      naxis;
    uint32_t size[3];
    int      shared;
    // number of keywords
    int      NBkw;

    // fast circular buffer size
    int CBsize;

} IMGID;


static inline uint64_t imgid_mdcompare(
    IMAGE_METADATA *md1,
    IMAGE_METADATA *md2
)
{
    uint64_t diff = 0;

    if (md1->datatype != md2->datatype) { diff |= (1ULL << 0); }
    if (md1->naxis != md2->naxis) { diff |= (1ULL << 1); }
    if (md1->size[0] != md2->size[0]) { diff |= (1ULL << 2); }
    if (md1->size[1] != md2->size[1]) { diff |= (1ULL << 3); }
    if (md1->size[2] != md2->size[2]) { diff |= (1ULL << 4); }
    if (md1->shared != md2->shared) { diff |= (1ULL << 5); }
    if (md1->NBkw != md2->NBkw) { diff |= (1ULL << 6); }
    if (md1->CBsize != md2->CBsize) { diff |= (1ULL << 7); }

    return diff;
}


/**
 * @brief Check if img complies to imgtemplate
 *
 */
static inline uint64_t imgid_compare_md(
    IMGID img,
    IMGID imgtemplate
)
{
    int compErr = 0;

    printf("COMPARING %s %s\n", img.name, imgtemplate.name);

    if(imgtemplate.md->datatype != _DATATYPE_UNINITIALIZED)
    {
        printf("Checking md->datatype       ");
        if(imgtemplate.md->datatype != img.md->datatype)
        {
            printf("FAIL\n");
            compErr++;
        }
        else
        {
            printf("PASS\n");
        }
    }

    if(imgtemplate.md->naxis != 0)
    {
        printf("Checking md->naxis  %d %d    ", imgtemplate.md->naxis, img.md->naxis);
        if(imgtemplate.md->naxis != img.md->naxis)
        {
            printf("FAIL\n");
            compErr++;
        }
        else
        {
            printf("PASS\n");
        }
    }

    if(imgtemplate.md->size[0] != 0)
    {
        printf("Checking md->size[0]        ");
        if(imgtemplate.md->size[0] != img.md->size[0])
        {
            printf("FAIL\n");
            compErr++;
        }
        else
        {
            printf("PASS\n");
        }
    }

    if(imgtemplate.md->size[1] != 0)
    {
        printf("Checking md->size[1]        ");
        if(imgtemplate.md->size[1] != img.md->size[1])
        {
            printf("FAIL\n");
            compErr++;
        }
        else
        {
            printf("PASS\n");
        }
    }

    if(imgtemplate.md->size[2] != 0)
    {
        printf("Checking md->size[2]        ");
        if(imgtemplate.md->size[2] != img.md->size[2])
        {
            printf("FAIL\n");
            compErr++;
        }
        else
        {
            printf("PASS\n");
        }
    }

    printf("Checking NBkw           ");
    if(imgtemplate.md->NBkw != img.md->NBkw)
    {
        printf("FAIL\n");
        printf("   %4u  %s\n", imgtemplate.md->NBkw, imgtemplate.md->name);
        printf("   %4u  %s\n", img.md->NBkw, img.md->name);
        printf("    template : %u\n", imgtemplate.md->NBkw);
        printf("    dest     : %u\n", img.md->NBkw);
        compErr++;
    }
    else
    {
        printf("PASS\n");
    }

    return compErr;
}

// Read ImageStreamIO from shared memory
//

static inline const char* imgid_strerror(errno_t err) {
    switch (err) {
    case IMGID_CONNECTED:
        return "CONNECTED";
    case IMGID_CREATED:
        return "CREATED";
    case IMGID_RECREATED:
        return "RECREATED";
    case IMGID_ERR_BADNAME:
        return "ERR_BADNAME";
    case IMGID_ERR_ALLOCATION:
        return "ERR_ALLOCATION";
    case IMGID_ERR_NOTFOUND:
        return "ERR_NOTFOUND";
    case IMGID_ERR_MISMATCH:
        return "ERR_MISMATCH";
    default:
        return "UNKNOWN_ERROR";
    }
}

#endif
