/* Edge Impulse ingestion SDK
 * Copyright (c) 2022 EdgeImpulse Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
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
