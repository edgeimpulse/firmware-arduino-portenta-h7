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

#include "ei_camera.h"
#include "ei_main.h"
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include "firmware-sdk/at_base64_lib.h"
#ifdef EI_CAMERA_FRAME_BUFFER_SDRAM
#include "SDRAM.h"
#endif

// for debugging the camera
#ifdef DEBUG_ARDUINO_CAMERA
#include "Arduino.h"
#include <USB/PluggableUSBSerial.h> // for _SerialUSB
#endif

#define ALIGN_PTR(p,a)   ((p & (a-1)) ?(((uintptr_t)p + a) & ~(uintptr_t)(a-1)) : p)
#define DWORD_ALIGN_PTR(p)  ALIGN_PTR(p,4)

HM01B0 himax;
static Camera cam(himax);
static bool is_initialised = false;
static bool is_ll_initialised = false;

/*
** @brief used to store the raw frame
*/

FrameBuffer fb;
#if defined(EI_CAMERA_FRAME_BUFFER_SDRAM) || defined(EI_CAMERA_FRAME_BUFFER_HEAP)
static uint8_t *ei_camera_frame_mem = NULL;
static uint8_t *ei_camera_frame_buffer = NULL; // 32-byte aligned
#else
static uint8_t ei_camera_frame_buffer[EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS] __attribute__((aligned(32)));
#endif

/*
** @brief points to the output of the capture
*/
static uint8_t *ei_camera_capture_out = NULL;

static bool prepare_snapshot(size_t width, size_t height, bool use_max_baudrate);
static bool take_snapshot(size_t width, size_t height, bool print_oks);
static void finish_snapshot();

/**
 * @brief      Convert monochrome data to rgb values
 *
 * @param[in]  mono_data  The mono data
 * @param      r          red pixel value
 * @param      g          green pixel value
 * @param      b          blue pixel value
 */
static inline void mono_to_rgb(uint8_t mono_data, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint8_t v = mono_data;
    *r = *g = *b = v;
}

/**
 * @brief      Determine whether to resize and to which dimension
 *
 * @param[in]  out_width     width of output image
 * @param[in]  out_height    height of output image
 * @param[out] resize_col_sz       pointer to frame buffer's column/width value
 * @param[out] resize_row_sz       pointer to frame buffer's rows/height value
 * @param[out] do_resize     returns whether to resize (or not)
 *
 */
static int calculate_resize_dimensions(uint32_t out_width, uint32_t out_height, uint32_t *resize_col_sz, uint32_t *resize_row_sz, bool *do_resize)
{
    EiDevicePortenta *dev = static_cast<EiDevicePortenta*>(EiDeviceInfo::get_device());
    const ei_device_snapshot_resolutions_t *list;
    size_t list_size;

    int dl = dev->get_resize_list((const ei_device_snapshot_resolutions_t **)&list, &list_size);
    if (dl) { /* apparently false is OK here?! */
        ei_printf("ERR: Device has no image resize feature\n");
        return 1;
    }

    // (default) conditions
    *resize_col_sz = EI_CAMERA_RAW_FRAME_BUFFER_COLS;
    *resize_row_sz = EI_CAMERA_RAW_FRAME_BUFFER_ROWS;
    *do_resize = false;

    for (size_t ix = 0; ix < list_size; ix++) {
        if ((out_width <= list[ix].width) && (out_height <= list[ix].height)) {
            *resize_col_sz = list[ix].width;
            *resize_row_sz = list[ix].height;
            *do_resize = true;
            break;
        }
    }

    return 0;
}

/**
 * @brief   Setup image sensor & start streaming
 *
 * @retval  false if initialisation failed
 */
bool ei_camera_init(void) {
    if (is_initialised) return true;

    if (is_ll_initialised == false) {

        if (!cam.begin(CAMERA_R320x240, CAMERA_GRAYSCALE, 30)) {
            ei_printf("ERR: Failed to initialise camera\r\n");
            return false;
        }
#ifdef DEBUG_ARDUINO_CAMERA
        cam.debug(_SerialUSB);
#endif

    #ifdef EI_CAMERA_FRAME_BUFFER_SDRAM
        if(ei_camera_frame_mem == NULL) {
            ei_camera_frame_mem = (uint8_t *) SDRAM.malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS + 32 /*alignment*/);
            if(ei_camera_frame_mem == NULL) {
                ei_printf("failed to create ei_camera_frame_mem\r\n");
                return false;
            }
            ei_camera_frame_buffer = (uint8_t *)ALIGN_PTR((uintptr_t)ei_camera_frame_mem, 32);
        }
    #endif

        is_ll_initialised = true;
    }

    // initialize frame buffer
#if defined(EI_CAMERA_FRAME_BUFFER_HEAP)
    ei_camera_frame_mem = (uint8_t *) ei_malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS + 32 /*alignment*/);
    if(ei_camera_frame_mem == NULL) {
        ei_printf("failed to create ei_camera_frame_mem\r\n");
        return false;
    }
    ei_camera_frame_buffer = (uint8_t *)ALIGN_PTR((uintptr_t)ei_camera_frame_mem, 32);
#endif

    fb.setBuffer(ei_camera_frame_buffer);
    is_initialised = true;

    return true;
}

/**
 * @brief      Stop streaming of sensor data
 */
void ei_camera_deinit(void) {

#if defined(EI_CAMERA_FRAME_BUFFER_HEAP)
    ei_free(ei_camera_frame_mem);
    ei_camera_frame_mem = NULL;
    ei_camera_frame_buffer = NULL;
#endif

    is_initialised = false;
}

/**
 * @brief      Capture, rescale and crop image
 *
 * @param[in]  img_width     width of output image
 * @param[in]  img_height    height of output image
 * @param[in]  out_buf       pointer to store output image, NULL may be used
 *                           if ei_camera_frame_buffer is to be used for capture and resize/cropping.
 *
 * @retval     false if not initialised, image captured, rescaled or cropped failed
 *
 */
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) {
    EiDevicePortenta *dev = static_cast<EiDevicePortenta*>(EiDeviceInfo::get_device());
    bool do_resize = false;
    bool do_crop = false;

    if (!is_initialised) {
        ei_printf("ERR: Camera is not initialized\r\n");
        return false;
    }

    dev->set_state(eiStateSampling);

    int snapshot_response = cam.grabFrame(fb, 3000);
    if (snapshot_response != 0) {
        ei_printf("ERR: Failed to get snapshot (%d)\r\n", snapshot_response);
        return false;
    }

    uint32_t resize_col_sz;
    uint32_t resize_row_sz;
    // choose resize dimensions
    int res = calculate_resize_dimensions(img_width, img_height, &resize_col_sz, &resize_row_sz, &do_resize);
    if (res) {
        ei_printf("ERR: Failed to calculate resize dimensions (%d)\r\n", res);
        return false;
    }

    if ((img_width != resize_col_sz)
        || (img_height != resize_row_sz)) {
        do_crop = true;
    }

    // The following variables should always be assigned
    // if this routine is to return true
    // cutout values
    ei_camera_capture_out = ei_camera_frame_buffer;

    if (do_resize) {

        // if only resizing then and out_buf provided then use it instead.
        if (out_buf && !do_crop) ei_camera_capture_out = out_buf;

        //ei_printf("resize cols: %d, rows: %d\r\n", resize_col_sz,resize_row_sz);
        ei::image::processing::resize_image(
            ei_camera_frame_buffer,
            EI_CAMERA_RAW_FRAME_BUFFER_COLS,
            EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
            ei_camera_capture_out,
            resize_col_sz,
            resize_row_sz,
            1); // bytes per pixel
    }

    if (do_crop) {
        uint32_t crop_col_sz;
        uint32_t crop_row_sz;
        uint32_t crop_col_start;
        uint32_t crop_row_start;
        crop_row_start = (resize_row_sz - img_height) / 2;
        crop_col_start = (resize_col_sz - img_width) / 2;
        crop_col_sz = img_width;
        crop_row_sz = img_height;

        // if (also) cropping and out_buf provided then use it instead.
        if (out_buf) ei_camera_capture_out = out_buf;

        //ei_printf("crop cols: %d, rows: %d\r\n", crop_col_sz,crop_row_sz);
        ei::image::processing::cropImage(
            ei_camera_frame_buffer,
            resize_col_sz,
            resize_row_sz,
            crop_col_start,
            crop_row_start,
            ei_camera_capture_out,
            crop_col_sz,
            crop_row_sz,
            8); // bits per pixel
    }

    dev->set_state(eiStateIdle);

    return true;
}

/**
 * @brief      Takes a snapshot, base64 encodes and outputs it to uart
 *
 * @param[in]  img_width     width of output image
 * @param[in]  img_height    height of output image
 *
 * @retval     true if snapshot was taken successfully
 *
 */
bool ei_camera_take_snapshot_encode_and_output(size_t width, size_t height, bool use_max_baudrate)
{
    bool result = true;

    if (!prepare_snapshot(width, height, use_max_baudrate)) result = false;

    if (result) {
        if (!take_snapshot(width, height, true)) {
            result = false;
        }
    }

    finish_snapshot();
    //print_memory_info2();

    return result;
}

/**
 * @brief      Starts a snapshot stream, base64 encodes and outputs it to uart
 *
 * @param[in]  img_width     width of output image
 * @param[in]  img_height    height of output image
 *
 * @retval     true if successful and/or terminated gracefully
 *
 */
bool ei_camera_start_snapshot_stream_encode_and_output(size_t width, size_t height, bool use_max_baudrate)
{
    bool result = true;
    EiDevicePortenta *dev = static_cast<EiDevicePortenta*>(EiDeviceInfo::get_device());

    ei_printf("Starting snapshot stream...\r\n");

    if (!prepare_snapshot(width, height, use_max_baudrate)) result = false;

    while (result) {
        if (!take_snapshot(width, height, true)) {
            result = false;
        }

        bool stopped_by_user = false;
        while (ei_get_serial_available() > 0) {
            if (ei_get_serial_byte() == 'b') {
                stopped_by_user = true;
                break;
            }
        }

        if (stopped_by_user) {
            ei_printf("Snapshot streaming stopped by user\r\n");
            dev->set_state(eiStateIdle);
            break;
        }
    }

    finish_snapshot();

    return result;
}

/**
 * @brief      Helper function: Takes a snapshot, base64 encodes and prints it to uart
 *
 * @param[in]  img_width     width of output image
 * @param[in]  img_height    height of output image
 * @param[in]  print_oks     whether to print OK indicator or not for CLI
 *
 * @retval     bool
 *
 * @note       Expects the camera and `ei_camera_frame_buffer` buffer to be
 * initialised
 */
static bool take_snapshot(size_t width, size_t height, bool print_oks)
{
    EiDevicePortenta *dev = static_cast<EiDevicePortenta*>(EiDeviceInfo::get_device());
    if (print_oks) {
        ei_printf("OK\r\n");
    }

    //ei_printf("take snapshot cols: %d, rows: %d\r\n", width, height);
    if (ei_camera_capture(width, height, NULL) == false) {
        ei_printf("ERR: Failed to capture image\r\n");
        return false;
    }

    ei::signal_t signal;
    signal.total_length = width * height;
    signal.get_data = &ei_camera_cutout_get_data;

    size_t signal_chunk_size = 1024;

    // loop through the signal
    float *signal_buf = (float*)ei_malloc(signal_chunk_size * sizeof(float));
    if (!signal_buf) {
        ei_printf("ERR: Failed to allocate signal buffer\r\n");
        return false;
    }

    uint8_t *per_pixel_buffer = (uint8_t*)ei_malloc(513); // 171 x 3 pixels
    if (!per_pixel_buffer) {
        ei_free(signal_buf);
        ei_printf("ERR: Failed to allocate per_pixel buffer\r\n");
        return false;
    }

    size_t per_pixel_buffer_ix = 0;

    for (size_t ix = 0; ix < signal.total_length; ix += signal_chunk_size) {
        size_t items_to_read = signal_chunk_size;
        if (items_to_read > signal.total_length - ix) {
            items_to_read = signal.total_length - ix;
        }

        int r = signal.get_data(ix, items_to_read, signal_buf);
        if (r != 0) {
            ei_printf("ERR: Failed to get data from signal (%d)\r\n", r);
            ei_free(signal_buf);
            ei_free(per_pixel_buffer);
            break;
        }

        for (size_t px = 0; px < items_to_read; px++) {
            uint32_t pixel = static_cast<uint32_t>(signal_buf[px]);

            // grab rgb
            uint8_t r = static_cast<float>(pixel >> 16 & 0xff);
            uint8_t g = static_cast<float>(pixel >> 8 & 0xff);
            uint8_t b = static_cast<float>(pixel & 0xff);

            // is monochrome anyway now, so just print 1 pixel at a time
            const bool print_rgb = false;

            if (print_rgb) {
                per_pixel_buffer[per_pixel_buffer_ix + 0] = r;
                per_pixel_buffer[per_pixel_buffer_ix + 1] = g;
                per_pixel_buffer[per_pixel_buffer_ix + 2] = b;
                per_pixel_buffer_ix += 3;
            }
            else {
                per_pixel_buffer[per_pixel_buffer_ix + 0] = r;
                per_pixel_buffer_ix++;
            }

            if (per_pixel_buffer_ix >= 513) {
                const size_t base64_output_size = 684;

                char *base64_buffer = (char*)ei_malloc(base64_output_size);
                if (!base64_buffer) {
                    ei_printf("ERR: Cannot allocate base64 buffer of size %lu, out of memory\r\n", base64_output_size);
                    ei_free(signal_buf);
                    ei_free(per_pixel_buffer);
                    return false;
                }

                int r = base64_encode_buffer((const char*)per_pixel_buffer, per_pixel_buffer_ix, base64_buffer, base64_output_size);
                if (r < 0) {
                    ei_printf("ERR: Failed to base64 encode (%d)\r\n", r);
                    ei_free(signal_buf);
                    ei_free(per_pixel_buffer);
                    return false;
                }

                ei_write_string(base64_buffer, r);
                per_pixel_buffer_ix = 0;
                ei_free(base64_buffer);
            }
            dev->set_state(eiStateUploading);
        }
    }

    const size_t new_base64_buffer_output_size = floor(per_pixel_buffer_ix / 3 * 4) + 4;
    char *base64_buffer = (char*)ei_malloc(new_base64_buffer_output_size);
    if (!base64_buffer) {
        ei_printf("ERR: Cannot allocate base64 buffer of size %lu, out of memory\r\n", new_base64_buffer_output_size);
        ei_free(signal_buf);
        ei_free(per_pixel_buffer);
        return false;
    }

    int r = base64_encode_buffer((const char*)per_pixel_buffer, per_pixel_buffer_ix, base64_buffer, new_base64_buffer_output_size);
    if (r < 0) {
        ei_printf("ERR: Failed to base64 encode (%d)\r\n", r);
        ei_free(signal_buf);
        ei_free(per_pixel_buffer);
        return false;
    }

    ei_write_string(base64_buffer, r);
    ei_printf("\r\n");

    ei_free(base64_buffer);
    ei_free(signal_buf);
    ei_free(per_pixel_buffer);
    dev->set_state(eiStateIdle);

    if (print_oks) {
        ei_printf("OK\r\n");
    }

    return true;
}

static bool verify_inputs(size_t width, size_t height)
{
    EiDevicePortenta *dev = static_cast<EiDevicePortenta*>(EiDeviceInfo::get_device());
    const ei_device_snapshot_resolutions_t *list;
    size_t list_size;
    const char *color_depth;

    int dl = dev->get_snapshot_list((const ei_device_snapshot_resolutions_t **)&list, &list_size, &color_depth);
    if (dl) { /* apparently false is OK here?! */
        ei_printf("ERR: Device has no snapshot feature\r\n");
        return false;
    }

    bool found_res = false;
    for (size_t ix = 0; ix < list_size; ix++) {
        if (list[ix].width == width && list[ix].height == height) {
            found_res = true;
        }
    }

    if (!found_res) {
        ei_printf("ERR: Invalid resolution %lux%lu\r\n", width, height);
        return false;
    }

    return true;
}


static bool prepare_snapshot(size_t width, size_t height, bool use_max_baudrate)
{
    if (!verify_inputs(width, height)) { return false; }

    if (ei_camera_init() == false) {
        ei_printf("ERR: Failed to initialize image sensor\r\n");
        return false;
    }

    return true;
}

static void finish_snapshot() {
    ei_camera_deinit();
}

int ei_camera_cutout_get_data(size_t offset, size_t length, float *out_ptr) {
    size_t bytes_left = length;
    size_t out_ptr_ix = 0;

    // read byte for byte
    while (bytes_left != 0) {

        // grab the value and convert to r/g/b
        uint8_t pixel = ei_camera_capture_out[offset];

        uint8_t r, g, b;
        mono_to_rgb(pixel, &r, &g, &b);

        // then convert to out_ptr format
        float pixel_f = (r << 16) + (g << 8) + b;
        out_ptr[out_ptr_ix] = pixel_f;

        // and go to the next pixel
        out_ptr_ix++;
        offset++;
        bytes_left--;
    }

    // and done!
    return 0;
}
