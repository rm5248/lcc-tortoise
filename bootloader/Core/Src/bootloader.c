/*
 * bootloader.c
 *
 *  Created on: Aug 9, 2024
 *      Author: robert
 */

// adapted from:
// https://github.com/viktorvano/STM32-Bootloader
// STM32 BootLoader sample project
// mcuboot

#include "bootloader.h"

#define PRIMARY_OFFSET   0x08003000
#define SECONDARY_OFFSET 0x08011000
#define PAGE_0_START     0x08000000
#define PAGE_SIZE        2048

#define IMAGE_MAGIC                 0x96f3b83d
#define IMAGE_HEADER_SIZE           32

struct arm_vector_table {
    uint32_t msp;
    uint32_t reset;
};

// Header struct - from mcuboot
struct image_version {
    uint8_t iv_major;
    uint8_t iv_minor;
    uint16_t iv_revision;
    uint32_t iv_build_num;
};

/** Image header.  All fields are in little endian byte order. */
struct image_header {
    uint32_t ih_magic;
    uint32_t ih_load_addr;
    uint16_t ih_hdr_size;           /* Size of image header (bytes). */
    uint16_t ih_protect_tlv_size;   /* Size of protected TLV area (bytes). */
    uint32_t ih_img_size;           /* Does not include header. */
    uint32_t ih_flags;              /* IMAGE_F_[...]. */
    struct image_version ih_ver;
    uint32_t _pad1;
};

static void panic(){
	while(1){
		HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_11);
		HAL_Delay(100);
	}
}

static void panic_bad_primary(){
	while(1){
		HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_10);
		HAL_Delay(100);
	}
}

static void erase_primary(){
	while(HAL_FLASH_Unlock() != HAL_OK);
	while(HAL_FLASH_OB_Unlock() != HAL_OK);

	FLASH_EraseInitTypeDef erase_init;
	erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
	erase_init.Page = (PRIMARY_OFFSET - PAGE_0_START) / PAGE_SIZE; // this is the page number that we want to erase from
	erase_init.NbPages = (SECONDARY_OFFSET - PRIMARY_OFFSET) / PAGE_SIZE;
	erase_init.Banks = FLASH_BANK_1;

	HAL_StatusTypeDef status_erase;
	uint32_t page_error;
	status_erase = HAL_FLASHEx_Erase(&erase_init, &page_error);
	if(status_erase != HAL_OK){
		panic();
	}
}

static void copy_secondary_to_primary(){
	uint32_t to_address = PRIMARY_OFFSET;
	uint32_t from_address = SECONDARY_OFFSET;

	while(to_address < SECONDARY_OFFSET){
		HAL_StatusTypeDef status;
		uint64_t data_to_flash = *(uint64_t*)from_address;

		status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, to_address, data_to_flash);
		if(status != HAL_OK){
			panic();
		}

		to_address += sizeof(uint64_t);
		from_address += sizeof(uint64_t);
	}
}

static int vector_table_valid(struct arm_vector_table* vt){
	if(vt->msp != 0 && vt->msp != ~0 &&
			vt->reset != 0 && vt->reset != ~0){
		return 1;
	}

	return 0;
}

void bootloader_run(){
	struct arm_vector_table* primary = (struct arm_vector_table*) (PRIMARY_OFFSET + 0x200);
	struct image_header* primary_header = (struct image_header*)(PRIMARY_OFFSET);
	struct arm_vector_table* secondary = (struct arm_vector_table*)(SECONDARY_OFFSET + 0x200);
	struct image_header* secondary_header = (struct image_header*)(SECONDARY_OFFSET);

	// If the data in secondary offset is good, copy that over to primary
	int primary_valid = vector_table_valid(primary);
	int secondary_valid = vector_table_valid(secondary);
	int secondary_is_newer = 0;

	if(vector_table_valid(primary) && vector_table_valid(secondary)){
		secondary_is_newer = secondary_header->ih_ver.iv_major > primary_header->ih_ver.iv_major &&
					secondary_header->ih_ver.iv_minor > primary_header->ih_ver.iv_minor &&
					secondary_header->ih_ver.iv_revision > primary_header->ih_ver.iv_revision;
	}

	if((secondary_valid && secondary_is_newer) || (secondary_valid && !primary_valid)){
		// Secondary offset good and newer, let's go copy it over
		erase_primary();
		copy_secondary_to_primary();

		while(HAL_FLASH_Lock() != HAL_OK);
		while(HAL_FLASH_OB_Lock() != HAL_OK);
	}

	// Execute primary offset
	primary = (struct arm_vector_table*) (PRIMARY_OFFSET + 0x200);
	void (*startup_function)(void) = (void (*)(void))primary->reset;

	if(!vector_table_valid(primary)){
		panic_bad_primary();
	}
    __set_MSP(primary->msp);
    startup_function();
}
