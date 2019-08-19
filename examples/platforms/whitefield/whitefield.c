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
#include <commline/cl_stackline_helpers.h>
#include <commline/pcap_util.h>

void *g_pcapHandle;

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
	mbuf->src_id = gNodeId;
	mbuf->dst_id = CL_DSTID_MACHDR_PRESENT;
	INFO("src:%0x dst:%0x (CL_DSTID_MACHDR_PRESENT=%0x) len:%d\n",
         mbuf->src_id, mbuf->dst_id, CL_DSTID_MACHDR_PRESENT, mbuf->len);

    if (!g_pcapHandle && getenv("OT-PCAP")) {
        char fname[512];
        snprintf(fname, sizeof(fname), "%s/openthread-%04x.pcap", getenv("OT-PCAP"), gNodeId);
        g_pcapHandle = pcap_init(fname);
    }
    if (g_pcapHandle) {
        pcap_write(g_pcapHandle, buf, len);
    }

	cl_sendto_q(MTYPE(AIRLINE, CL_MGR_ID), mbuf, mbuf->len + sizeof(msg_buf_t));
}

