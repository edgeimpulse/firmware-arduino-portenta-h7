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

#ifndef EI_DEVICE_PORTENTA
#define EI_DEVICE_PORTENTA

/* Include ----------------------------------------------------------------- */
#include "ei_device_info_lib.h"

/** Number of sensors used */
#define EI_DEVICE_N_SENSORS		1

#if defined(EI_CLASSIFIER_SENSOR) && EI_CLASSIFIER_SENSOR == EI_CLASSIFIER_SENSOR_CAMERA
#define EI_DEVICE_N_RESOLUTIONS		4
#else
#define EI_DEVICE_N_RESOLUTIONS		3
#endif

#define EI_DEVICE_N_RESIZE_RESOLUTIONS		5

/**
 * @brief      Class description and implementation of device specific
 * 			   characteristics
 */
class EiDevicePortenta : public EiDeviceInfo
{
private:
	ei_device_sensor_t sensors[EI_DEVICE_N_SENSORS];
	ei_device_snapshot_resolutions_t snapshot_resolutions[EI_DEVICE_N_RESOLUTIONS];
	ei_device_snapshot_resolutions_t resize_resolutions[EI_DEVICE_N_RESIZE_RESOLUTIONS];
	bool camera_present;
	EiState ei_program_state;
public:
	EiDevicePortenta(EiDeviceMemory* mem);
	void init_device_id(void);

	bool get_wifi_connection_status(void);
	bool get_wifi_present_status();
	bool get_sensor_list(const ei_device_sensor_t **sensor_list, size_t *sensor_list_size);
	bool get_snapshot_list(const ei_device_snapshot_resolutions_t **resolution_list, size_t *resolution_list_size,
						   const char **color_depth);
	bool get_resize_list(const ei_device_snapshot_resolutions_t **resize_list, size_t *resize_list_size);
	
	void set_default_data_output_baudrate(void);
	void set_max_data_output_baudrate(void);
	void set_state(EiState state) override;
};

/* Function prototypes ----------------------------------------------------- */
void ei_write_string(char *data, int length);
bool ei_user_invoke_stop(void);
void ei_print_memory_info2(void);

/* Reference to object for external usage ---------------------------------- */
// extern EiDevicePortenta EiDevice;

#endif
