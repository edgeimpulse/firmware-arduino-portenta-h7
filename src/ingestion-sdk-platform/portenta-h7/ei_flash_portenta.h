/*
 * Copyright (c) 2022 Edge Impulse Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS
 * IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EI_FLASH_PORTENTA_H
#define EI_FLASH_PORTENTA_H

#include "firmware-sdk/ei_device_memory.h"
#include "FlashIAP.h"

#define PORTENTA_FS_BLOCK_ERASE_TIME_MS     965

/** Align `a` to `byte` bytes */
#define BYTE_ALIGN(a, byte) (((a) & ((byte)-1)) ? ((a) & ~((byte)-1)) + (byte) : a)

class EiFlashMemory : public EiDeviceMemory {
protected:
    uint32_t read_data(uint8_t *data, uint32_t address, uint32_t num_bytes);
    uint32_t write_data(const uint8_t *data, uint32_t address, uint32_t num_bytes);
    uint32_t erase_data(uint32_t address, uint32_t num_bytes);
    mbed::FlashIAP iap;
    uint32_t base_address;
public:
    EiFlashMemory(uint32_t config_size);
};

#endif /* EI_FLASH_PORTENTA_H */