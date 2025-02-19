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
