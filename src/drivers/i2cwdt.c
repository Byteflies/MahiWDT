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
#include <linux/i2c-dev.h>

struct i2cWDTContext {
    int fd;

    char* data;
    unsigned int dataLen;
};

static void i2cWDTDriverFree(void* context)
{
    struct i2cWDTContext* c = (struct i2cWDTContext*)context;

    if(c) {
        if(c->fd >= 0) {
            close (c->fd);
        }

        if (c->data) {
            free(c->data);
        }

        free(c);
    }
}

static void i2cWDTDriverKick(void* context)
{
    struct i2cWDTContext* c = (struct i2cWDTContext*)context;

    write(c->fd, c->data, c->dataLen);
}

WDTHWDriver* i2cWDTDriverNew(const char* bus, uint8_t addr, char* wrData, unsigned int interval)
{
    WDTHWDriver* d = (WDTHWDriver*)calloc(1, sizeof(WDTHWDriver));
    if(!d) return NULL;

    struct i2cWDTContext* c = calloc(1, sizeof(struct i2cWDTContext));
    if(!c) goto failed;

    d->wdtContext = c;
    d->wdtFreeFunc = i2cWDTDriverFree;
    d->wdtKickFunc = i2cWDTDriverKick;

    c->fd = open(bus, O_RDWR);
    if(c->fd < 0) goto failed;

    if (ioctl(c->fd, I2C_SLAVE, addr) < 0) goto failed;

    d->wdtMaxIntervalSeconds = interval;

    size_t len = strlen(wrData);
    if (len % 2) goto failed;

    c->data = (char*)malloc(len/2);
    c->dataLen = len/2;

    for (int i=0; i<len; i+=2){
        char h[] = {wrData[i], wrData[i+1], 0};
        
        int value;
        sscanf(h, "%02x", &value);
        c->data[i/2] = value;
    }

    return d;

failed:
    i2cWDTDriverFree(d);
    return NULL;
}
