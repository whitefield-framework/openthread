/*
 * Copyright (C) 2017 Rahul Jadhav <nyrahul@gmail.com>
 *
 * This file is subject to the terms and conditions of the GNU
 * General Public License v2. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     stackline
 * @{
 *
 * @file
 * @brief       Whitefield interfaces to be used in Openthread
 *
 * @author      Rahul Jadhav <nyrahul@gmail.com>
 *
 * @}
 */

#define _WHITEFIELD_C_

#include "platform-whitefield.h"

int whitefield_init(int nodeid)
{
    if(cl_init(MTYPE(STACKLINE, nodeid), CL_ATTACHQ)!=CL_SUCCESS) {
        ERROR("commline init failed, exiting...\n");
        exit(1);
    }
    return 0;
}

void whitefield_deinit(void)
{
    cl_cleanup();
}
