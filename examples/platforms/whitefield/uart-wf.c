/*
 *  Copyright (c) 2016, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform-whitefield.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include <openthread/platform/debug_uart.h>
#include <openthread/platform/uart.h>

#include "utils/code_utils.h"
#include <commline/commline.h>  //Whitefield COMMLINE

static uint8_t        s_receive_buffer[128];
static const uint8_t *s_write_buffer;
static uint16_t       s_write_length;
static int            g_uds_serv = -1;
static int            g_uds_childfd = -1;

void platformUartRestore(void)
{
}

int uds_get_path(char *path, int maxlen)
{
    char *uds = getenv("UDSPATH");
    if(uds)
    {
        return snprintf(path, maxlen, "%s/%04x.uds", uds, gNodeId);
    }
    return snprintf(path, maxlen, "%04x.uds", gNodeId);
}

void uds_close(void)
{
    char udspath[512];
    if(g_uds_serv)
    {
        uds_get_path(udspath, sizeof(udspath));
        unlink(udspath);
        CLOSE(g_uds_serv);
    }
    INFO("exiting...\r\n");
}

int uds_open(void)
{
    struct sockaddr_un addr;

    g_uds_serv = socket(AF_UNIX, SOCK_STREAM, 0);
    if(g_uds_serv <= -1)
    {
        ERROR("UDS socket failed errno=%d\n", errno);
        return -1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    uds_get_path(addr.sun_path, sizeof(addr.sun_path));
    unlink(addr.sun_path);

    INFO("binding to unix domain sock:[%s]\r\n", addr.sun_path);
    if (bind(g_uds_serv, (struct sockaddr*)&addr, sizeof(addr)) ||
        listen(g_uds_serv, 5))
    {
        CLOSE(g_uds_serv);
        ERROR("UDS bind/listen error %d\n", errno);
        return -1;
    }
    atexit(uds_close);
    return g_uds_serv;
}

otError otPlatUartEnable(void)
{
#ifdef OPENTHREAD_TARGET_LINUX
    // Ensure we terminate this process if the
    // parent process dies.
    prctl(PR_SET_PDEATHSIG, SIGHUP);
#endif

    // We need this signal to make sure that this
    // process terminates properly.
    signal(SIGPIPE, SIG_DFL);

    if(uds_open() < 0)
    {
        return OT_ERROR_FAILED;
    }

    return OT_ERROR_NONE;
}

otError otPlatUartDisable(void)
{
    otError error = OT_ERROR_NONE;

    CLOSE(g_uds_serv);

    return error;
}

otError otPlatUartSend(const uint8_t *aBuf, uint16_t aBufLength)
{
    otError error = OT_ERROR_NONE;

    otEXPECT_ACTION(s_write_length == 0, error = OT_ERROR_BUSY);

    if (g_uds_childfd >= 0)
    {
        s_write_buffer = aBuf;
        s_write_length = aBufLength;
    }
    INFO("uart-send: [%.*s]\n", aBufLength-2, aBuf);

exit:
    return error;
}

#define ADD_TO_FDSET(FDSET, FD)         \
    if(FDSET && (FD >= 0))              \
    {                                   \
        FD_SET(FD, FDSET);              \
        if(aErrorFdSet)                 \
        {                               \
            FD_SET(FD, aErrorFdSet);    \
        }                               \
        if(aMaxFd && *aMaxFd < FD)      \
        {                               \
            *aMaxFd = FD;               \
        }                               \
    }

void platformUartUpdateFdSet(fd_set *aReadFdSet, fd_set *aWriteFdSet, fd_set *aErrorFdSet, int *aMaxFd)
{
    ADD_TO_FDSET(aReadFdSet, g_uds_serv);
    ADD_TO_FDSET(aReadFdSet, g_uds_childfd);

    if (s_write_length > 0)
    {
        ADD_TO_FDSET(aWriteFdSet, g_uds_childfd);
    }
}

void platformUartProcess(void)
{
    ssize_t       rval;
    const int     error_flags = POLLERR | POLLNVAL | POLLHUP;
    struct pollfd pollfd[]    = {
        {
            g_uds_serv,  POLLIN | error_flags, 0
        },
        {
            g_uds_childfd, POLLOUT | POLLIN | error_flags, 0
        },
    };

    errno = 0;

    rval = poll(pollfd, g_uds_childfd>=0?2:1, 0);

    if (rval < 0)
    {
        ERROR("poll");
        exit(EXIT_FAILURE);
    }

    if (rval > 0)
    {
        if ((pollfd[0].revents & error_flags) != 0)
        {
            ERROR("g_uds_serv");
            exit(EXIT_FAILURE);
        }

        if (pollfd[1].revents & error_flags && (g_uds_childfd >= 0))
        {
            CLOSE(g_uds_childfd);
        }

        if(g_uds_childfd >= 0)
        {
            if (pollfd[1].revents & POLLIN)
            {
                rval = read(g_uds_childfd, s_receive_buffer, sizeof(s_receive_buffer));

                if (rval > 0)
                {
                    otPlatUartReceived(s_receive_buffer, (uint16_t)rval);
                }
            }

            if ((s_write_length > 0) && (pollfd[1].revents & POLLOUT))
            {
                rval = write(g_uds_childfd, s_write_buffer, s_write_length);

                if (rval <= 0)
                {
                    ERROR("write");
                    exit(EXIT_FAILURE);
                }

                /* Rahul: There is no freaking check on rval.
                   This can cause buffer overrun for s_write_buffer.*/
                s_write_buffer += (uint16_t)rval;
                s_write_length -= (uint16_t)rval;

                if (s_write_length == 0)
                {
                    otPlatUartSendDone();
                }
            }
        }

        // Check for events on server unix domain socket
        if (pollfd[0].revents & POLLIN)
        {
            CLOSE(g_uds_childfd);
            g_uds_childfd = accept(g_uds_serv, NULL, NULL);
            if(g_uds_childfd < 0)
            {
                ERROR("uds accept");
                exit(1);
            }
        }
    }
}

#if OPENTHREAD_CONFIG_ENABLE_DEBUG_UART && (OPENTHREAD_CONFIG_LOG_OUTPUT == OPENTHREAD_CONFIG_LOG_OUTPUT_DEBUG_UART)

static FILE *posix_logfile;

otError otPlatDebugUart_logfile(const char *filename)
{
    posix_logfile = fopen(filename, "wt");

    return posix_logfile ? OT_ERROR_NONE : OT_ERROR_FAILED;
}

void otPlatDebugUart_putchar_raw(int c)
{
    FILE *fp;

    /* note: log file will have a mix of cr/lf and
     * in some/many cases duplicate cr because in
     * some cases the log function {ie: Mbed} already
     * includes the CR or LF... but other log functions
     * do not include cr/lf and expect it appened
     */
    fp = posix_logfile;

    if (fp != NULL)
    {
        /* log is lost ... until a file is setup */
        fputc(c, fp);
        /* we could "fflush" but will not */
    }
}

int otPlatDebugUart_kbhit(void)
{
    /* not supported */
    return 0;
}

int otPlatDebugUart_getc(void)
{
    /* not supported */
    return -1;
}

#endif
