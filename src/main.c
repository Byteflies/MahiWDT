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


static volatile bool die = false;

static void termHandler(int sig, siginfo_t *siginfo, void *context)
{
    die = true;

}

int main(int argc, char** argv)
{
    /* Avoid broken pipe issues */
    signal(SIGPIPE, SIG_IGN);

    /* Catch SIGTERM to do cleanup */
    struct sigaction action;
    action.sa_sigaction = &termHandler;
    action.sa_flags = SA_SIGINFO;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    WDTSystem s;
    memset(&s, 0, sizeof(s));

    /* Set defaults */
    s.rebootDelaySeconds = 30;

    int opt;
    while ((opt = getopt(argc, argv, "n:w:p:r:c:u:")) != -1) {
        switch (opt) {
            case 'n':
                ;
                char* filename = strtok(optarg, ":");
                char* time = strtok(NULL, ":");

                if(filename && time) {
                    s.uptimeNotificationFile = strdup(filename);
                    s.uptimeNotificationSeconds = atoll(time);
                    break;
                }

                fprintf(stderr, "Could not parse uptime file description\n");
                break;
            case 'w':
                ;
                char* driver = strtok(optarg, ":");
                WDTHWDriver* newDriver = NULL;

                if(!strcmp(driver, "kernel")) {
                    char* path = strtok(NULL, ":");
                    char* interval = strtok(NULL, ":");

                    if(interval && path) {
                        newDriver = kernelWDTDriverNew(path, atoi(interval));
                    } else {
                        errno = EINVAL;
                    }
                } else if(!strcmp(driver, "dummy")) {
                    char* interval = strtok(NULL, ":");
                    if(interval) {
                        newDriver = dummyWDTDriverNew(atoi(interval));
                    } else {
                        errno = EINVAL;
                    }
                } else {
                    errno = EINVAL;
                }

                if(!newDriver) {
                    fprintf(stderr, "Failed to initialize hardware watchdog (errno=%s)\n", strerror(errno));
                    goto cleanup;
                }

                newDriver->next = s.wdtDriver;
                s.wdtDriver = newDriver;
                break;
            case 'r':
                s.rebootDelaySeconds = atoi(optarg);
                break;
            case 'p':
                ;
                char* path = strtok(optarg, ":");
                char* startupInterval = strtok(NULL, ":");
                char* normalInterval = strtok(NULL, ":");
                char* portOwner = strtok(NULL, ":");

                WDTPort* newPort = NULL;

                if(path && startupInterval && normalInterval) {
                    newPort = portInit(path, atoi(startupInterval), atoi(normalInterval), portOwner);
                }

                if(!newPort) {
                    fprintf(stderr, "Failed to init port: %s\n", strerror(errno));
                    goto cleanup;
                }

                newPort->next = s.port;
                s.port = newPort;
                break;
            case 'c':
                s.rebootCmd = strdup(optarg);
                break;
            case 'u':
                s.dropPrivUser = strdup(optarg);
                break;
            default: /* '?' */
                fprintf(stderr, "Invalid usage\n");
                goto cleanup;
        }
    }

    if(!s.wdtDriver) {
        fprintf(stderr, "Please specify at least one watchdog device\n");
        goto cleanup;
    }

    if(s.dropPrivUser && changeUser(s.dropPrivUser)) {
        fprintf(stderr, "Failed to drop privileges\n");
        goto cleanup;
    }

    /* Run the wdt logic */
    bool cleanExit = logicRun(&s, &die);

    if(!cleanExit) {
        /* 1) A channel timed out, reset the HW wdt */
        for(WDTHWDriver* driver = s.wdtDriver; driver; driver=driver->next) {
            wdtDriverKick(driver);
        }

        /* 2) and try to do a clean reboot. */
        if(s.rebootCmd) {
            printf("Running: %s\n", s.rebootCmd);
            int retVal = system(s.rebootCmd);
            printf("System return value: %d\n", retVal);

            /* 3) Give the reboot time. Note that this program will be killed during reboot, so
             * you should configure the watchdog with CONFIG_WATCHDOG_NOWAYOUT, or at least
             * using magic close to prevent the system getting stuck should the reboot fail */
            while(s.rebootDelaySeconds && !die) {
                sleep(1);
                s.rebootDelaySeconds--;
                for(WDTHWDriver* driver = s.wdtDriver; driver; driver=driver->next) {
                    wdtDriverKick(driver);
                }
            }
        }
    }

cleanup:
    ;
    WDTPort* port = s.port;
    while(port) {
        WDTPort* nextPort = port->next;
        portUninit(port);
        port = nextPort;
    }

    WDTHWDriver* driver = s.wdtDriver;
    while(driver) {
        WDTHWDriver* nextDriver = driver->next;
        wdtDriverFree(driver);
        driver=nextDriver;
    }

    if(s.rebootCmd) free(s.rebootCmd);
    if(s.uptimeNotificationFile) free(s.uptimeNotificationFile);
    if(s.dropPrivUser) free(s.dropPrivUser);
}

