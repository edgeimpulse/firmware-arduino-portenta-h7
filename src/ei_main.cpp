/* The Clear BSD License
 *
 * Copyright (c) 2025 EdgeImpulse Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 *   * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* Include ----------------------------------------------------------------- */
#include "ei_main.h"
#include "ei_device_portenta.h"
#include "ei_flash_portenta.h"
#include "porting/ei_classifier_porting.h"
#include "ei_config_types.h"
#include "ingestion-sdk-platform/portenta-h7/ei_at_handlers.h"
#include "ei_run_impulse.h"

static void ei_at_server(void);
static unsigned char at_thread_stack[8 * 1024];
static rtos::Thread at_server_thread(osPriorityAboveNormal, sizeof(at_thread_stack), at_thread_stack, "at-server-thread");
static ATServer *at;

mbed::DigitalOut led(LED1);

void fill_memory(void) 
{
    size_t size = 8 * 1024;
    size_t allocated = 0;
    while (1) {
        void *ptr = ei_malloc(size);
        if (!ptr) {
            if (size == 1) break;
            size /= 2;
        }
        else {
            allocated += size;
        }
    }
    ei_printf("Allocated: %u bytes\n", allocated);
}

/* Public functions -------------------------------------------------------- */
void ei_main_init(void) 
{
    EiDevicePortenta *dev = static_cast<EiDevicePortenta*>(EiDeviceInfo::get_device());
    ei_sleep(500);

    dev->set_state(eiStateIdle);
    ei_printf("Hello from Edge Impulse Device SDK.\r\n"
              "Compiled on %s %s\r\n", __DATE__, __TIME__);

    at = ei_at_init(dev);   // init at handlers
    ei_printf("Type AT+HELP to see a list of commands.\r\n");
    at->print_prompt();

    at_server_thread.start(mbed::callback(ei_at_server));    
}

/**
 * @brief 
 * 
 */
void ei_main(void)
{
    rtos::ThisThread::sleep_for(10000);
}

static void ei_at_server(void)
{
    char data;
    
    while(1){
        while (ei_get_serial_available()){
            data = ei_get_serial_byte();
            at->handle(data);
        }
        rtos::ThisThread::sleep_for(10);
    }
}
