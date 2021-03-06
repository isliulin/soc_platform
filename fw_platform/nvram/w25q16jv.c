#include "spiflashdev.h"


spiflash_info_t spiflash_info_W25Q16JV = {
	.spiflash_type = SPIFLASH_W25Q16JV,
	.spiflash_id={	
	/* Manufacture and device IDs for winbond Electronics W25Q16JV SPI Flash. */
		.manufacturer_id = 0xEF,
		.device_id = {0x40, 0x15},
		.device_id_num = 2, 
		.extern_id_num = 0
	},
	.nvram_feature_struct = {
		.type = NVRAM_TYPE_SPIFLASH,		// nvram type
		.size = 2*1024*1024,			// size of nvram device (in Bytes)
		.pagesize = SECTOR_SIZE_64KB		// size of a nvram device page (in Bytes)
	}
};


