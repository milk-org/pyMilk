
/**
 * @file    fps_CONFstart.h
 * @brief   FPS conf process start
 */

#ifndef FPS_RUNCONFONOFF_H
#define FPS_RUNCONFONOFF_H

#include "fps.h"

errno_t functionparameter_CONFstart(FUNCTION_PARAMETER_STRUCT *fps);
errno_t functionparameter_CONFstop(FUNCTION_PARAMETER_STRUCT *fps);
errno_t functionparameter_RUNstart(FUNCTION_PARAMETER_STRUCT *fps);
errno_t functionparameter_RUNstop(FUNCTION_PARAMETER_STRUCT *fps);

#endif
