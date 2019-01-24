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

#include "project.h"

void portKick(WDTPort* port, bool initial)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if(initial) {
        port->expirySeconds = now.tv_sec + port->startupTimeoutSeconds;
    } else {
        port->expirySeconds = now.tv_sec + port->normalTimeoutSeconds;
    }
}

void portUninit(WDTPort* port)
{
    if(!port) return;

    if(port->fd >= 0) {
        close(port->fd);
    }

    if(port->bound) {
        unlink(port->laddr.sun_path);
    }

    free(port);
}

WDTPort* portInit(const char* path, uint32_t startupTimeoutSeconds, uint32_t normalTimeoutSeconds, char* portOwner)
{
    WDTPort* port = (WDTPort*)calloc(1, sizeof(WDTPort));
    if(!port) return false;
    memset(port, 0, sizeof(*port));

    /* Set path */
    port->laddr.sun_family = AF_UNIX;
    strncpy(port->laddr.sun_path, path, sizeof(port->laddr.sun_path) - 1);

    /* Delete path */
    unlink(path);

    /* Set timing */
    port->startupTimeoutSeconds = startupTimeoutSeconds;
    port->normalTimeoutSeconds = normalTimeoutSeconds;

    port->fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (port->fd < 0) goto error;

    fchmod(port->fd, 0700);

    uid_t uid;
    gid_t gid;

    if(portOwner) {
        if(getUidGid(portOwner, &uid, &gid)) {
            errno=EACCES;
            goto error;
        }
    }

    /* Bind the UNIX domain address to the created socket */
    if (bind(port->fd, (struct sockaddr*)&port->laddr, sizeof(port->laddr))) goto error;
    port->bound = true;

    if(portOwner) {
        if(chown(path, uid, gid)) {
            goto error;
        }
    }

    portKick(port, true);

    return port;

error:
    portUninit(port);
    return NULL;
}
