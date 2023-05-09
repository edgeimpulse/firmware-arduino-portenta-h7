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
/* Include ----------------------------------------------------------------- */
#include "ei_flash_portenta.h"
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"

/** 32-bit align write buffer size */
#define WORD_ALIGN(a)	((a & 0x3) ? (a & ~0x3) + 0x4 : a)
/** Align addres to given sector size */
#define SECTOR_ALIGN(a, sec_size)	((a & (sec_size-1)) ? (a & ~(sec_size-1)) + sec_size : a)

/* Public functions -------------------------------------------------------- */
EiFlashMemory::EiFlashMemory(uint32_t config_size):
    EiDeviceMemory(config_size, 90, 0, 0)
{
    iap.init();

    /* Setup addresses for fs */
    block_size = iap.get_sector_size(iap.get_flash_start() + iap.get_flash_size() - 1UL);
    base_address = SECTOR_ALIGN(FLASHIAP_APP_ROM_END_ADDR, block_size);
    memory_size = iap.get_flash_size() - base_address;
    used_blocks = (config_size < block_size) ? 1 : ceil(float(config_size) / block_size);
    memory_blocks = memory_size / block_size;
}


uint32_t EiFlashMemory::read_data(uint8_t *data, uint32_t address, uint32_t num_bytes)
{
    int ret;

    ret = iap.read((void *)data, base_address + address, num_bytes);
    if (ret != 0) {
        ei_printf("ERR: Failed to read data! (%d)\n", ret);
        return 0;
    }

    return num_bytes;
}

uint32_t EiFlashMemory::write_data(const uint8_t *data, uint32_t address, uint32_t num_bytes)
{
    int ret;

    ret = iap.program(data, WORD_ALIGN(base_address + address), WORD_ALIGN(num_bytes));
    if (ret != 0) {
        ei_printf("ERR: Failed to write data! (%d)\n", ret);
        return 0;
    }

    return num_bytes;
}

uint32_t EiFlashMemory::erase_data(uint32_t address, uint32_t num_bytes)
{
    int ret;
    
    ret = iap.erase(base_address + address, SECTOR_ALIGN(num_bytes, block_size));

    if(ret != 0) {
        ei_printf("ERR: Failed to erase flash (%d)\n", ret);
        return 0;
    }

    return num_bytes;
}
