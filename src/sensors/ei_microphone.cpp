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
#include "ei_microphone.h"
#include <PDM.h>
#include "ei_config_types.h"
#include "sensor_aq_mbedtls_hs256.h"
#include "firmware-sdk/sensor-aq/sensor_aq.h"
#include "edge-impulse-sdk/CMSIS/DSP/Include/arm_math.h"
#include "edge-impulse-sdk/dsp/numpy.hpp"
#include "ei_device_portenta.h"
#include "ei_flash_portenta.h"

#define AUDIO_SAMPLING_FREQUENCY            16000


typedef struct {
    int16_t *buffers[2];
    uint8_t buf_select;
    uint8_t buf_ready;
    uint32_t buf_count;
    uint32_t n_samples;
} inference_t;

/* Extern declared --------------------------------------------------------- */
extern mbed::DigitalOut led;

using namespace ei;
using namespace rtos;
using namespace events;

static size_t ei_write(const void*, size_t size, size_t count, EI_SENSOR_AQ_STREAM*)
{
    ei_printf("Writing: %d\r\n", count);
    return count;
}

static int ei_seek(EI_SENSOR_AQ_STREAM*, long int offset, int origin)
{
    ei_printf("Seeking: %d\r\n", offset);
    return 0;
}

/* Private variables ------------------------------------------------------- */
// static signed short *sampleBuffer;
static signed short sampleBuffer[2048];
static bool is_uploaded = false;
static volatile bool record_ready = false;
static uint32_t headerOffset;
static uint32_t samples_required;
static uint32_t current_sample;

//static signed short *inference_buffers[2];
static inference_t inference;

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

/* Audio thread setup */
#define AUDIO_THREAD_STACK_SIZE             4096
static uint8_t AUDIO_THREAD_STACK[AUDIO_THREAD_STACK_SIZE];
Thread queue_thread(osPriorityHigh, AUDIO_THREAD_STACK_SIZE, AUDIO_THREAD_STACK, "audio-thread-stack");
static EventQueue mic_queue;




/* Private functions ------------------------------------------------------- */

static void ei_microphone_blink() {
    led = !led;
}

static void audio_buffer_callback(uint32_t n_bytes)
{
    EiDeviceMemory *mem = EiDeviceInfo::get_device()->get_memory();

    if(record_ready == true){
        mem->write_sample_data((const uint8_t *)sampleBuffer, headerOffset + current_sample, n_bytes);
    // ei_printf("write: %d %d\r\n", headerOffset + current_sample, n_bytes);
        ei_mic_ctx.signature_ctx->update(ei_mic_ctx.signature_ctx, (uint8_t*)sampleBuffer, n_bytes);

        current_sample += n_bytes;
        if(current_sample >= (samples_required << 1)) {
            record_ready = false;
        }
    }
}


static void pdm_data_ready_callback(void)
{
    int bytesAvailable = PDM.available();

    // read into the sample buffer
    int bytesRead = PDM.read(sampleBuffer, bytesAvailable);

    if(record_ready == true) {
        mic_queue.call(&audio_buffer_callback, bytesRead);
    }
}


static void audio_buffer_inference_callback(uint32_t n_bytes)
{
    for(int i = 0; i < n_bytes>>1; i++) {
        inference.buffers[inference.buf_select][inference.buf_count++] = sampleBuffer[i];

        if(inference.buf_count >= inference.n_samples) {
            inference.buf_select ^= 1;
            inference.buf_count = 0;
            inference.buf_ready = 1;
        }
    }
}

static void pdm_data_ready_inference_callback(void)
{
    int bytesAvailable = PDM.available();

    // read into the sample buffer
    int bytesRead = PDM.read(sampleBuffer, bytesAvailable);
    // ei_printf("available: %d %d\r\n", bytesAvailable, bytesRead);

    if(record_ready == true) {
        mic_queue.call(&audio_buffer_inference_callback, bytesRead);
    }
}

static void finish_and_upload(char *filename, uint32_t sample_length_ms)
{
    EiDeviceInfo *dev = EiDeviceInfo::get_device();
    ei_printf("Done sampling, total bytes collected: %u\n", current_sample);

    mbed::Ticker t;
    t.attach(&ei_microphone_blink, 1.0f);


    ei_printf("[1/1] Uploading file to Edge Impulse...\n");

    mbed::Timer upload_timer;
    upload_timer.start();

    //ei_printf("Not uploading file, not connected to WiFi. Used buffer, type=%d, from=%lu, to=%lu, sensor_name=%s, sensor_units=%s.\n",
    //            EI_INT16, 0, sample_block*4096 + headerOffset, "audio", "wav");
    ei_printf("Not uploading file, not connected to WiFi. Used buffer, from=%lu, to=%lu.\n", 0, current_sample + headerOffset);

    upload_timer.stop();
    ei_printf("[1/1] Uploading file to Edge Impulse OK (took %d ms.)\n", upload_timer.read_ms());

    is_uploaded = true;

    led = 0;

    ei_printf("OK\n");

    dev->set_state(eiStateIdle);
}

static int insert_ref(char *buffer, int hdrLength)
{
    // 32-byte align (for flash)
    #define EXTRA_BYTES(a)  ((a & 0x1F) ? 0x20  - (a & 0x1F) : (a & 0x1F))
    const char *ref = "Ref-BINARY-i16";
    int addLength = 0;
    int padding = EXTRA_BYTES(hdrLength + 3 + 14);

    buffer[addLength++] = 0x78;
    buffer[addLength++] = 14 + padding;
    for(int i = 0; i < strlen(ref); i++) {
        buffer[addLength++] = *(ref + i);
    }
    for(int i = 0; i < padding; i++) {
        buffer[addLength++] = ' ';
    }

    buffer[addLength++] = 0xFF;

    return addLength;
}

static bool create_header(void)
{
    EiDeviceInfo *dev = EiDeviceInfo::get_device();
    EiDeviceMemory *mem = dev->get_memory();
    sensor_aq_init_mbedtls_hs256_context(&ei_mic_signing_ctx, &ei_mic_hs_ctx, dev->get_sample_hmac_key().c_str());

    sensor_aq_payload_info payload = {
        dev->get_id_pointer(),
        dev->get_type_pointer(),
        1000.0f / static_cast<float>(AUDIO_SAMPLING_FREQUENCY),
        { { "audio", "wav" } }
    };

    int tr = sensor_aq_init(&ei_mic_ctx, &payload, NULL, true);

    if (tr != AQ_OK) {
        ei_printf("sensor_aq_init failed (%d)\n", tr);
        return false;
    }

    // then we're gonna find the last byte that is not 0x00 in the CBOR buffer.
    // That should give us the whole header
    size_t end_of_header_ix = 0;
    for (size_t ix = ei_mic_ctx.cbor_buffer.len - 1; ix >= 0; ix--) {
        if (((uint8_t*)ei_mic_ctx.cbor_buffer.ptr)[ix] != 0x0) {
            end_of_header_ix = ix;
            break;
        }
    }

    if (end_of_header_ix == 0) {
        ei_printf("ERR: Failed to find end of header\n");
        return false;
    }

    int ref_size = insert_ref(((char*)ei_mic_ctx.cbor_buffer.ptr + end_of_header_ix), end_of_header_ix);

    // and update the signature
    tr = ei_mic_ctx.signature_ctx->update(ei_mic_ctx.signature_ctx, (uint8_t*)(ei_mic_ctx.cbor_buffer.ptr + end_of_header_ix), ref_size);
    if (tr != 0) {
        ei_printf("ERR: Failed to update signature from header (%d)\n", tr);
        return false;
    }

    end_of_header_ix += ref_size;

    headerOffset = end_of_header_ix;

    return true;
}


/* Public functions -------------------------------------------------------- */

bool ei_microphone_record(uint32_t sample_length_ms, uint32_t start_delay_ms, bool print_start_messages)
{
    EiDeviceMemory *mem = EiDeviceInfo::get_device()->get_memory();
    EiDeviceInfo *dev = EiDeviceInfo::get_device();
    dev->set_state(eiStateErasingFlash);

    if (print_start_messages) {
        ei_printf("Starting in %lu ms... (or until all flash was erased)\n", start_delay_ms < 2000 ? 2000 : start_delay_ms);
    }

    if (start_delay_ms < 2000) {
        ThisThread::sleep_for(2000 - start_delay_ms);
    }

    // updates headerOffset
    create_header();

    uint32_t bytesRequiredSamples = samples_required << 1;
    uint32_t totalBytesRequired = BYTE_ALIGN(bytesRequiredSamples + headerOffset, mem->block_size);
    int tr = mem->erase_sample_data(0, totalBytesRequired);
    if (tr != totalBytesRequired) {
        ei_printf("ERR: Failed to erase blockdevice (%d), (%d)\n", tr, totalBytesRequired);
        return false;
    }

    // Write to blockdevice
    tr = mem->write_sample_data((const uint8_t*)ei_mic_ctx.cbor_buffer.ptr, 0, headerOffset);
    if (tr != headerOffset) {
        ei_printf("ERR: Failed to write to header blockdevice (%d)\n", tr);
        return false;
    }

    // Initialize PDM with:
    // - one channel (mono mode)
    if (!PDM.begin(1, AUDIO_SAMPLING_FREQUENCY)) {
        ei_printf("ERR: Failed to start PDM!");
    }

    queue_thread.start(callback(&mic_queue, &EventQueue::dispatch_forever));

    if (print_start_messages) {
        ei_printf("Sampling...\n");
    }


    return true;
}

bool ei_microphone_inference_start(uint32_t n_samples)
{

    inference.buffers[0] = (int16_t *)ei_malloc(n_samples * sizeof(int16_t));

    if(inference.buffers[0] == NULL) {
        return false;
    }

    inference.buffers[1] = (int16_t *)ei_malloc(n_samples * sizeof(int16_t));

    if(inference.buffers[1] == NULL) {
        ei_free(inference.buffers[0]);
        return false;
    }

    if(sampleBuffer == NULL) {
        ei_free(inference.buffers[0]);
        ei_free(inference.buffers[1]);
        return false;
    }

    inference.buf_select = 0;
    inference.buf_count  = 0;
    inference.n_samples  = n_samples;
    inference.buf_ready  = 0;

    queue_thread.start(callback(&mic_queue, &EventQueue::dispatch_forever));

    // configure the data receive callback
    PDM.onReceive(&pdm_data_ready_inference_callback);

    // optionally set the gain, defaults to 24
    // Note: values >=52 do not work
    //PDM.setGain(40);

    //PDM.setBufferSize((n_samples >> 1) * sizeof(int16_t));
    PDM.setBufferSize(2048);

    // Initialize PDM with:
    // - one channel (mono mode)
    if (!PDM.begin(1, AUDIO_SAMPLING_FREQUENCY)) {
        ei_printf("ERR: Failed to start PDM!");
    }
    ei_microphone_inference_reset_buffers();

    record_ready = true;

    return true;

}

/**
 * @brief      Wait for a full buffer
 *
 * @return     In case of an buffer overrun return false
 */
bool ei_microphone_inference_record(void)
{
    bool ret = true;

    if (inference.buf_ready == 1) {
        ei_printf(
            "Error sample buffer overrun. Decrease the number of slices per model window "
            "(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW)\n");
        ret = false;
    }

    while (inference.buf_ready == 0) {
        ThisThread::sleep_for(1);
    }

    inference.buf_ready = 0;

    return ret;
}

/**
 * @brief      Reset buffer counters for non-continuous inferecing
 */
void ei_microphone_inference_reset_buffers(void)
{
    inference.buf_ready = 0;
    inference.buf_count = 0;
}

/**
 * Get raw audio signal data
 */
int ei_microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr)
{
    return numpy::int16_to_float(&inference.buffers[inference.buf_select ^ 1][offset], out_ptr, length);
}


bool ei_microphone_inference_end(void)
{
    PDM.end();
    ei_free(inference.buffers[0]);
    ei_free(inference.buffers[1]);
}

/**
 * Sample raw data
 */
bool ei_microphone_sample_start(void)
{
    EiDeviceInfo *dev = EiDeviceInfo::get_device();
    EiDeviceMemory *mem = EiDeviceInfo::get_device()->get_memory();
    // this sensor does not have settable interval...
    // ei_config_set_sample_interval(static_cast<float>(1000) / static_cast<float>(AUDIO_SAMPLING_FREQUENCY));
    int sample_length_blocks;

    ei_printf("Sampling settings:\n");
    ei_printf("\tInterval: %.5f ms.\n", dev->get_sample_interval_ms());
    ei_printf("\tLength: %lu ms.\n", dev->get_sample_length_ms());
    ei_printf("\tName: %s\n", dev->get_sample_label().c_str());
    ei_printf("\tHMAC Key: %s\n", dev->get_sample_hmac_key().c_str());

    char filename[256];
    int fn_r = snprintf(filename, 256, "/fs/%s", dev->get_sample_label().c_str());
    if (fn_r <= 0) {
        ei_printf("ERR: Failed to allocate file name\n");
        return false;
    }
    ei_printf("\tFile name: %s\n", filename);

    samples_required = (uint32_t)((dev->get_sample_length_ms()) / dev->get_sample_interval_ms());

    /* Round to even number of samples for word align flash write */
    if(samples_required & 1) { samples_required++; }

    current_sample = 0;

    is_uploaded = false;

    // sampleBuffer = (int16_t *)ei_malloc(mem->block_size);

    if (sampleBuffer == NULL) {
        return false;
    }

    // configure the data receive callback
    PDM.onReceive(&pdm_data_ready_callback);

    // optionally set the gain, defaults to 24.
    // Note: values >=52 do not work
    //PDM.setGain(40);

    ei_printf("Sector size: %d req: %d\r\n", mem->block_size, samples_required);

    //PDM.setBufferSize(mem->block_size);
    PDM.setBufferSize(2048);

    bool r = ei_microphone_record(dev->get_sample_length_ms(), (((samples_required <<1)/ mem->block_size) * PORTENTA_FS_BLOCK_ERASE_TIME_MS), true);
    if (!r) {
        return r;
    }
    record_ready = true;
    dev->set_state(eiStateSampling);


    while(record_ready == true) {
        ThisThread::sleep_for(10);
    };

    PDM.end();

    int ctx_err = ei_mic_ctx.signature_ctx->finish(ei_mic_ctx.signature_ctx, ei_mic_ctx.hash_buffer.buffer);
    if (ctx_err != 0) {
        ei_printf("ERR: Failed to finish signature (%d)\n", ctx_err);
        return false;
    }

    // load the first block in flash...
    uint8_t *block_buffer = (uint8_t*)ei_malloc(mem->block_size);
    if (!block_buffer) {
        ei_printf("ERR: Failed to allocate a block buffer to write the hash\n");
        return false;
    }

    int j = mem->read_sample_data(block_buffer, 0, mem->block_size);
    if (j != mem->block_size) {
        ei_printf("ERR: Failed to read first block (%d)\n", j);
        ei_free(block_buffer);
        return false;
    }

    // update the hash
    uint8_t *hash = ei_mic_ctx.hash_buffer.buffer;
    // we have allocated twice as much for this data (because we also want to be able to represent in hex)
    // thus only loop over the first half of the bytes as the signature_ctx has written to those
    for (size_t hash_ix = 0; hash_ix < ei_mic_ctx.hash_buffer.size / 2; hash_ix++) {
        // this might seem convoluted, but snprintf() with %02x is not always supported e.g. by newlib-nano
        // we encode as hex... first ASCII char encodes upper 4 bits
        uint8_t first = (hash[hash_ix] >> 4) & 0xf;
        // second encodes lower 4 bytes
        uint8_t second = hash[hash_ix] & 0xf;

        // if 0..9 -> '0' (48) + value, if >10, then use 'a' (97) - 10 + value
        char first_c = first >= 10 ? 87 + first : 48 + first;
        char second_c = second >= 10 ? 87 + second : 48 + second;

        block_buffer[ei_mic_ctx.signature_index + (hash_ix * 2) + 0] = first_c;
        block_buffer[ei_mic_ctx.signature_index + (hash_ix * 2) + 1] = second_c;
    }

    j = mem->erase_sample_data(0, mem->block_size);
    if (j != mem->block_size) {
        ei_printf("ERR: Failed to erase first block (%d)\n", j);
        ei_free(block_buffer);
        return false;
    }

    j = mem->write_sample_data(block_buffer, 0, mem->block_size);

    ei_free(block_buffer);

    if (j != mem->block_size) {
        ei_printf("ERR: Failed to write first block with updated hash (%d)\n", j);
        return false;
    }


    finish_and_upload(filename, dev->get_sample_length_ms());

    return true;
}
