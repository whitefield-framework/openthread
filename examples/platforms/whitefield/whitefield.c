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

int wfInit(int nodeid)
{
    if(cl_init(MTYPE(STACKLINE, nodeid), CL_ATTACHQ)!=CL_SUCCESS) {
        ERROR("commline init failed, exiting...\n");
        exit(1);
    }
    return 0;
}

void wfDeinit(void)
{
    cl_cleanup();
}

void wfSendPacket(uint8_t *buf, size_t len)
{
	DEFINE_MBUF(mbuf);

	mbuf->len = len;
	memcpy(mbuf->buf, buf, len);
	mbuf->src_id = gNodeId - 1;
	mbuf->dst_id = !(gNodeId - 1);
	INFO("src:%0x dst:%0x len:%d\n", mbuf->src_id, mbuf->dst_id, mbuf->len);
	cl_sendto_q(MTYPE(AIRLINE, CL_MGR_ID), mbuf, mbuf->len + sizeof(msg_buf_t));
}

