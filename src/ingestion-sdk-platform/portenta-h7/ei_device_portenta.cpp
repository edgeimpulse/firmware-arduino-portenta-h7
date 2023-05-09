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
#include "ei_device_portenta.h"
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"

#include <stdarg.h>

#include <stdio.h>
#include "Arduino.h"
#include <USB/PluggableUSBSerial.h> // for _SerialUSB
#include "mbed.h"
#include "ei_flash_portenta.h"
#include "sensors/ei_camera.h"
#include "sensors/ei_microphone.h"

#ifdef EI_CAMERA_FRAME_BUFFER_SDRAM
#include "SDRAM.h"
#endif
/* Constants --------------------------------------------------------------- */

/** Max size for device id array */
#define DEVICE_ID_MAX_SIZE  32

/** Sensors */
typedef enum
{
    MICROPHONE = 0

}used_sensors_t;

#define EDGE_STRINGIZE_(x) #x
#define EDGE_STRINGIZE(x) EDGE_STRINGIZE_(x)

/** Max Data Output Baudrate
 * @note: must be 115200 for the CLI even though the baudrate is not switched internally,
 *        since USB is used to transmit data rather than UART.
 **/
const ei_device_data_output_baudrate_t ei_dev_max_data_output_baudrate = {
    "115200",
    115200,
};

/** Default Data Output Baudrate */
const ei_device_data_output_baudrate_t ei_dev_default_data_output_baudrate = {
    "115200",
    115200,
};

/* Private function declarations ------------------------------------------- */

/* Public functions -------------------------------------------------------- */

EiDevicePortenta::EiDevicePortenta(EiDeviceMemory* mem)
{   
    EiDeviceInfo::memory = mem;

    load_config();
    init_device_id();

    device_type = std::string(EDGE_STRINGIZE(TARGET_NAME));

    ei_program_state = eiStateIdle;

#ifdef EI_CAMERA_FRAME_BUFFER_SDRAM
    // initialise the SDRAM
    SDRAM.begin(SDRAM_START_ADDRESS);
#endif
    // (may) depends on the SDRAM
    camera_present = ei_camera_init();
}

EiDeviceInfo* EiDeviceInfo::get_device(void)
{
    static EiFlashMemory memory(sizeof(EiConfig));
    static EiDevicePortenta dev(&memory);

    return &dev;
}

void EiDevicePortenta::init_device_id(void)
{
    uint8_t portenta_id[48] = {0};
    char buf[DEVICE_ID_MAX_SIZE];
    getUniqueSerialNumber(portenta_id);

    /* Setup device ID ei_device_id */
    snprintf(&buf[0], DEVICE_ID_MAX_SIZE, "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c"
        , portenta_id[0]
        , portenta_id[2]
        , portenta_id[4]
        , portenta_id[6]
        , portenta_id[8]
        , portenta_id[10]
        , portenta_id[12]
        , portenta_id[14]
        , portenta_id[16]
        , portenta_id[18]
        , portenta_id[20]
        , portenta_id[22]
        );
    
    device_id = std::string(buf);
}

/**
 * @brief      Set output baudrate to max
 *
 */
void EiDevicePortenta::set_max_data_output_baudrate()
{
    ei_serial_set_baudrate(ei_dev_max_data_output_baudrate.val);
}

/**
 * @brief      Set output baudrate to default
 *
 */
void EiDevicePortenta::set_default_data_output_baudrate(void)
{
    ei_serial_set_baudrate(ei_dev_default_data_output_baudrate.val);
}

/**
 * @brief      No Wifi available for device.
 *
 * @return     Always return false
 */
bool EiDevicePortenta::get_wifi_connection_status(void)
{
    return false;
}

/**
 * @brief      No Wifi available for device.
 *
 * @return     Always return false
 */
bool EiDevicePortenta::get_wifi_present_status(void)
{
    return false;
}

/**
 * @brief      Create sensor list with sensor specs
 *             The studio and daemon require this list
 * @param      sensor_list       Place pointer to sensor list
 * @param      sensor_list_size  Write number of sensors here
 *
 * @return     False if all went ok
 */
bool EiDevicePortenta::get_sensor_list(const ei_device_sensor_t **sensor_list, size_t *sensor_list_size)
{
    /* Calculate number of bytes available on flash for sampling, reserve 1 block for header + overhead */
    uint32_t available_bytes = (memory->get_available_sample_blocks()-1) * memory->block_size;

    sensors[MICROPHONE].name = "Built-in microphone";
    sensors[MICROPHONE].start_sampling_cb = &ei_microphone_sample_start;
    sensors[MICROPHONE].max_sample_length_s = available_bytes / (16000 * 2);
    sensors[MICROPHONE].frequencies[0] = 16000.0f;

    *sensor_list      = sensors;
    *sensor_list_size = EI_DEVICE_N_SENSORS;

    return false;
}

/**
 * @brief      Create resolution list for snapshot setting
 *             The studio and daemon require this list
 * @param      snapshot_list       Place pointer to resolution list
 * @param      snapshot_list_size  Write number of resolutions here
 *
 * @return     False if all went ok
 */
bool EiDevicePortenta::get_snapshot_list(const ei_device_snapshot_resolutions_t **snapshot_list, size_t *snapshot_list_size,
                                         const char **color_depth)
{
    snapshot_resolutions[0].width = 320;
    snapshot_resolutions[0].height = 240;
    snapshot_resolutions[1].width = 160;
    snapshot_resolutions[1].height = 120;
    snapshot_resolutions[2].width = 128;
    snapshot_resolutions[2].height = 96;

#if defined(EI_CLASSIFIER_SENSOR) && EI_CLASSIFIER_SENSOR == EI_CLASSIFIER_SENSOR_CAMERA
    snapshot_resolutions[2].width = EI_CLASSIFIER_INPUT_WIDTH;
    snapshot_resolutions[2].height = EI_CLASSIFIER_INPUT_HEIGHT;
#endif

    *snapshot_list      = snapshot_resolutions;
    *snapshot_list_size = EI_DEVICE_N_RESOLUTIONS;
    *color_depth = "Grayscale";

    return false;
}

/**
 * @brief      Create resolution list for resizing
 * @param      resize_list       Place pointer to resolution list
 * @param      resize_list_size  Write number of resolutions here
 *
 * @return     False if all went ok
 */
bool EiDevicePortenta::get_resize_list(const ei_device_snapshot_resolutions_t **resize_list, size_t *resize_list_size)
{
    resize_resolutions[0].width = 128;
    resize_resolutions[0].height = 96;

    resize_resolutions[1].width = 160;
    resize_resolutions[1].height = 120;

    resize_resolutions[2].width = 200;
    resize_resolutions[2].height = 150;

    resize_resolutions[3].width = 256;
    resize_resolutions[3].height = 192;

    resize_resolutions[4].width = 320;
    resize_resolutions[4].height = 240;

    *resize_list      = resize_resolutions;
    *resize_list_size = EI_DEVICE_N_RESIZE_RESOLUTIONS;

    return false;
}

mbed::DigitalOut led_red(LED_RED);
mbed::DigitalOut led_green(LED_GREEN);
mbed::DigitalOut led_blue(LED_BLUE);

void EiDevicePortenta::set_state(EiState state)
{
    ei_program_state = state;
    static uint8_t upload_toggle = 0;

    if((state == eiStateFinished) || (state == eiStateIdle)){
        ei_program_state = eiStateIdle;

        led_red.write(1);
        led_green.write(1);
        led_blue.write(1);
        upload_toggle = 0;
    }
    else if (state == eiStateSampling) {
        led_red.write(1);
        led_green.write(0);
        led_blue.write(1);
    }
    else if (state == eiStateUploading) {

        if(upload_toggle) {
            led_red.write(0);
            led_green.write(1);
            led_blue.write(1);
        }
        else {
            led_red.write(1);
            led_green.write(1);
            led_blue.write(1);
        }
        upload_toggle ^= 1;
    }

}

/**
 * @brief      Call this function periocally during inference to
 *             detect a user stop command
 *
 * @return     true if user requested stop
 */
bool ei_user_invoke_stop(void) {
    return false;
}

/**
 * @brief      Write serial data with length to Serial output
 *
 * @param      data    The data
 * @param[in]  length  The length
 */
void ei_write_string(char *data, int length) {
    while (length--) {
        Serial.write(*(data++));
    }
}

/**
 * @brief      Get Arduino serial object
 *
 * @return     pointer to Serial
 */
mbed::Stream* ei_get_serial() {
    return static_cast<mbed::Stream*>(&_SerialUSB);
}

/**
 * @brief      Check if new serial data is available
 *
 * @return     Returns number of available bytes
 */
int ei_get_serial_available(void) {
    return Serial.available();
}

/**
 * @brief      Get next available byte
 *
 * @return     byte
 */
char ei_get_serial_byte(void) {
    return Serial.read();
}

void ei_print_memory_info2(void) 
{
    // allocate enough room for every thread's stack statistics
    int cnt = osThreadGetCount();    
    mbed_stats_thread_t *thread_stats = (mbed_stats_thread_t*) ei_malloc(cnt * sizeof(mbed_stats_stack_t));

    size_t thread_cnt =  mbed_stats_thread_get_each(thread_stats, cnt);
    for (int i = 0; i < thread_cnt; i++) {
        ei_printf("Thread: 0x%lX, Name: %s, Stack size: %lu, Stack space: %lu\r\n", thread_stats[i].id, thread_stats[i].name, thread_stats[i].stack_size, thread_stats[i].stack_space);
    }
    ei_free(thread_stats);

    cnt = osThreadGetCount();
    mbed_stats_stack_t *stats = (mbed_stats_stack_t*) ei_malloc(cnt * sizeof(mbed_stats_stack_t));
    cnt = mbed_stats_stack_get_each(stats, cnt);
    for (int i = 0; i < cnt; i++) {
        ei_printf("Thread: 0x%lX, Stack size: %lu / %lu\r\n", stats[i].thread_id, stats[i].max_size, stats[i].reserved_size);
    }
    ei_free(stats);

    // Grab the heap statistics
    mbed_stats_heap_t heap_stats;
    mbed_stats_heap_get(&heap_stats);
    ei_printf("Heap size: %lu / %lu bytes (max: %lu)\r\n", heap_stats.current_size, heap_stats.reserved_size, heap_stats.max_size);
}
/* Private functions ------------------------------------------------------- */
