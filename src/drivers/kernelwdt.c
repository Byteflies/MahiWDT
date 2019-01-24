/*
 * Copyright (c) 2019, Bertold Van den Bergh (vandenbergh@bertold.org, https://projectmahi.com/)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the author nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR DISTRIBUTOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "../project.h"
#include <linux/watchdog.h>

struct kernelWDTContext {
    int fd;
};

static void kernelWDTDriverFree(void* context)
{
    struct kernelWDTContext* c = (struct kernelWDTContext*)context;

    if(c) {
        if(c->fd >= 0) {
            close (c->fd);
        }

        free(c);
    }
}

static void kernelWDTDriverKick(void* context)
{
    struct kernelWDTContext* c = (struct kernelWDTContext*)context;

    ioctl(c->fd, WDIOC_KEEPALIVE, NULL);
}

WDTHWDriver* kernelWDTDriverNew(const char* path, int interval)
{
    WDTHWDriver* d = (WDTHWDriver*)calloc(1, sizeof(WDTHWDriver));
    if(!d) return NULL;

    struct kernelWDTContext* c = calloc(1, sizeof(struct kernelWDTContext));
    if(!c) goto failed;

    d->wdtContext = c;
    d->wdtFreeFunc = kernelWDTDriverFree;
    d->wdtKickFunc = kernelWDTDriverKick;

    c->fd = open(path, O_RDWR);
    if(c->fd < 0) goto failed;

    /* Failure is harmless */
    ioctl(c->fd, WDIOC_SETTIMEOUT, &interval);

    int realInterval;
    if (ioctl(c->fd, WDIOC_GETTIMEOUT, &realInterval)) {
        /* Failed to read WDT interval, play safe */
        realInterval = 0;
    }

    if(realInterval < 2) {
        realInterval = 2;
    }

    realInterval /= 2;

    d->wdtMaxIntervalSeconds = realInterval;

    return d;

failed:
    kernelWDTDriverFree(d);
    return NULL;
}
