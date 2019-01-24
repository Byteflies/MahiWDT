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

#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#ifndef SRC_PROJECT_H_
#define SRC_PROJECT_H_

typedef struct WDTPort {
    /* Socket */
    struct sockaddr_un laddr;
    int fd;
    bool bound;

    /* Timing settings */
    uint32_t startupTimeoutSeconds;
    uint32_t normalTimeoutSeconds;

    /* When will this timer expire */
    uint64_t expirySeconds;

    struct WDTPort* next;
} WDTPort;

typedef struct WDTHWDriver {
    uint64_t wdtMaxIntervalSeconds;
    void* wdtContext;

    void(*wdtKickFunc)(void* context);
    void(*wdtFreeFunc)(void* context);

    struct WDTHWDriver* next;
} WDTHWDriver;

typedef struct {
    WDTPort* port;
    uint32_t rebootDelaySeconds;

    WDTHWDriver* wdtDriver;

    uint64_t uptimeNotificationSeconds;
    char* uptimeNotificationFile;

    char* rebootCmd;

    char* dropPrivUser;
} WDTSystem;

void wdtDriverKick(WDTHWDriver* driver);
void wdtDriverFree(WDTHWDriver* driver);

void portKick(WDTPort* port, bool initial);
void portUninit(WDTPort* port);
WDTPort* portInit(const char* path, uint32_t startupTimeoutSeconds, uint32_t normalTimeoutSeconds, char* portOwner);

bool logicRun(WDTSystem* s, volatile bool* die);

WDTHWDriver* kernelWDTDriverNew(const char* path, int interval);
WDTHWDriver* dummyWDTDriverNew(unsigned int interval);

uint64_t utilGetUptimeSeconds();

int changeUser(char* username);
int getUidGid(char* username, uid_t* uid, gid_t* gid);

#endif /* SRC_PROJECT_H_ */
