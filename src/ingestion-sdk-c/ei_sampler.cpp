/* Edge Impulse ingestion SDK
 * Copyright (c) 2020 EdgeImpulse Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Include ----------------------------------------------------------------- */
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#include <stdint.h>
#include <stdlib.h>

#include "ei_sampler.h"
#include "ei_config_types.h"
#include "ei_device_portenta.h"

#include "mbed.h"
#include "ei_flash_portenta.h"
#include "sensor_aq_mbedtls_hs256.h"


using namespace rtos;
using namespace events;

/* Private variables ------------------------------------------------------- */
static uint32_t samples_required;
static uint32_t current_sample;
static uint32_t sample_buffer_size;
static uint32_t headerOffset = 0;


static char write_word_buf[4];
static int write_addr = 0;

static size_t ei_write(const void *buffer, size_t size, size_t count, EI_SENSOR_AQ_STREAM*)
{
    EiDeviceMemory *mem = EiDeviceInfo::get_device()->get_memory();

    for(int i=0; i<count; i++) {

        write_word_buf[write_addr&0x3] = *((char *)buffer++);

        if((++write_addr & 0x03) == 0x00) {
            mem->write_sample_data((const uint8_t*)write_word_buf, (write_addr - 4) + headerOffset, 4);
        }

    }

    return count;
}

static int ei_seek(EI_SENSOR_AQ_STREAM*, long int offset, int origin)
{
    return 0;
}

static unsigned char ei_mic_ctx_buffer[1024];
static sensor_aq_signing_ctx_t ei_mic_signing_ctx;
static sensor_aq_mbedtls_hs256_ctx_t ei_mic_hs_ctx;
static sensor_aq_ctx ei_mic_ctx = {
    { ei_mic_ctx_buffer, 1024 },
    &ei_mic_signing_ctx,
    &ei_write,
    &ei_seek,
    NULL,
};

static void ei_write_last_data(void)
{
    EiDeviceMemory *mem = EiDeviceInfo::get_device()->get_memory();

    char fill = (write_addr & 0x03);

    if(fill != 0x00) {
        for(int i=fill; i<4; i++) {
            write_word_buf[i] = 0xFF;
        }

        mem->write_sample_data((const uint8_t*)write_word_buf, (write_addr & ~0x03) + headerOffset, 4);
    }
}

EI_SENSOR_AQ_STREAM stream;


/* Private function prototypes --------------------------------------------- */
static void finish_and_upload(char *filename, uint32_t sample_length_ms);
static bool sample_data_callback(const void *sample_buf, uint32_t byteLenght);

static bool create_header(sensor_aq_payload_info *payload);


/**
 * @brief      Setup and start sampling, write CBOR header to flash
 *
 * @param      v_ptr_payload  sensor_aq_payload_info pointer hidden as void
 * @param[in]  sample_size    Number of bytes for 1 sample (include all axis)
 *
 * @return     true if successful
 */
bool ei_sampler_start_sampling(void *v_ptr_payload, starter_callback ei_sample_start, uint32_t sample_size)
{
    return true;
}


static bool create_header(sensor_aq_payload_info *payload)
{


    return true;
}

/**
 * @brief      Sampling is finished, signal no uploading file
 *
 * @param      filename          The filename
 * @param[in]  sample_length_ms  The sample length milliseconds
 */
static void finish_and_upload(char *filename, uint32_t sample_length_ms)
{

    ei_printf("Done sampling, total bytes collected: %u\n", samples_required);


    ei_printf("[1/1] Uploading file to Edge Impulse...\n");

    mbed::Timer upload_timer;
    upload_timer.start();

    ei_printf("Not uploading file, not connected to WiFi. Used buffer, from=%lu, to=%lu.\n", 0, write_addr + headerOffset);//sample_buffer_size + headerOffset);


    upload_timer.stop();
    ei_printf("[1/1] Uploading file to Edge Impulse OK (took %d ms.)\n", upload_timer.read_ms());

    ei_printf("OK\n");
}

/**
 * @brief      Write samples to FLASH in CBOR format
 *
 * @param[in]  sample_buf  The sample buffer
 * @param[in]  byteLenght  The byte lenght
 *
 * @return     true if all required samples are received. Caller should stop sampling,
 */
static bool sample_data_callback(const void *sample_buf, uint32_t byteLenght)
{
    sensor_aq_add_data(&ei_mic_ctx, (float *)sample_buf, byteLenght / sizeof(float));

    if(++current_sample > samples_required) {
        return true;
    }
    else {
        return false;
    }
}
