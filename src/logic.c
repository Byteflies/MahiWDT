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

bool logicRun(WDTSystem* s, volatile bool* die)
{
    unsigned int numPorts=0;
    for(WDTPort* port = s->port; port; port=port->next) {
        portKick(port, true);
        numPorts++;
    }

    struct pollfd fds[numPorts];
    unsigned int i=0;
    for(WDTPort* port = s->port; port; port=port->next) {
        fds[i].fd = port->fd;
        fds[i].events = POLLIN;
        i++;
    }

    uint8_t rxBuf[16];
    uint64_t hwDriverNextKick = 0;
    uint64_t hwDriverMinimumIncrement = -1ULL;

    for(WDTHWDriver* driver = s->wdtDriver; driver; driver=driver->next) {
        if (driver->wdtMaxIntervalSeconds < hwDriverMinimumIncrement) {
            hwDriverMinimumIncrement = driver->wdtMaxIntervalSeconds;
        }
    }

    while(!*die) {
        /* Calculate timeout */
        uint64_t earliest = -1ULL;
        WDTPort* earlyPort = NULL;

        for(WDTPort* port = s->port; port; port=port->next) {
            if(port->expirySeconds < earliest) {
                earliest = port->expirySeconds;
                earlyPort = port;
            }
        }

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if(earliest <= now.tv_sec) {
            fprintf(stderr, "Watchdog timeout on channel %s\n", earlyPort->laddr.sun_path);
            return false;
        }

        if(hwDriverNextKick <= now.tv_sec) {
            /* Check if we need to put the system up flag */
            if(s->uptimeNotificationSeconds) {
                uint64_t uptime = utilGetUptimeSeconds();
                if(uptime >= s->uptimeNotificationSeconds) {
                    s->uptimeNotificationSeconds = 0;
                    int fd = open(s->uptimeNotificationFile, O_RDWR | O_CREAT, 0644);
                    if(fd >= 0) close(fd);
                }
            }

            /* Kick the HW wdt */
            for(WDTHWDriver* driver = s->wdtDriver; driver; driver=driver->next) {
                wdtDriverKick(driver);
            }

            hwDriverNextKick = now.tv_sec + hwDriverMinimumIncrement;
        }

        /* Limit timeout to max hw WDT delay */
        if(earliest > hwDriverNextKick) {
            earliest = hwDriverNextKick;
        }

        /* Make timeout relative */
        earliest -= now.tv_sec;

        int retVal = poll(fds, numPorts, earliest * 1000);
        if(retVal < 0) {
            if(errno != EINTR) {
                return false;
            }
        } else if(retVal) {
            for(unsigned int i=0; i<numPorts; i++) {
                i=0;
                for(WDTPort* port = s->port; port; port=port->next) {
                    if(fds[i].revents & POLLIN) {
                        struct sockaddr_un raddr;
                        socklen_t len = sizeof(raddr);
                        ssize_t recvLen = recvfrom(port->fd, rxBuf, sizeof(rxBuf), 0, (struct sockaddr*)&raddr, &len);
                        if(recvLen < 0) {
                            if(errno != EINTR) {
                                return false;
                            }
                        } else if(recvLen == 4 && memcmp(rxBuf, "KICK", 4) == 0) {
                            portKick(port, false);
                        } else if(recvLen == 5 && memcmp(rxBuf, "ERROR", 5) == 0) {
                            fprintf(stderr, "Watchdog ERROR on channel %s\n", port->laddr.sun_path);
                            return false;
                        }
                    }
                    i++;
                }
            }
        }
    }

    return true;
}
