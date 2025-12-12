#include <stdio.h>
#include <array>
#include <algorithm>

#include <nds.h>
#include <fat.h>

#include "datel_flash_routines.h"

// The following were taken from libnds
#define	C_REG_CARD_DATA_RD	(*(vu32*)0x04100010)
#define C_REG_AUXSPICNT		(*(vu16*)0x040001A0)
#define C_REG_AUXSPICNTH	(*(vu8*)0x040001A1)
#define C_REG_AUXSPIDATA	(*(vu8*)0x040001A2)
#define C_REG_ROMCTRL		(*(vu32*)0x040001A4)
#define C_REG_CARD_COMMAND	((vu8*)0x040001A8)
#define C_CARD_CR1_ENABLE	0x80	// in byte 1, i.e. 0x8000
#define C_CARD_CR1_IRQ		0x40	// in byte 1, i.e. 0x4000
#define C_CARD_BUSY			(1<<31)	// when reading, still expecting incomming data?
#define C_CARD_SPI_BUSY		(1<<7)
#define C_CARD_CR1_EN		0x8000
#define	C_CARD_CR1_SPI_EN	0x2000
#define	C_CARD_CR1_SPI_HOLD	0x40
//---------------------------------------------------------------------------------

#define SD_COMMAND_TIMEOUT 0xFFF
#define SD_WRITE_TIMEOUT 0xFFFF
#define C_CARD_CR2_SETTINGS 0xA0586000

#define MAIN_ADDR getMainAddr()
#define OTHER_ADDR getSecondAddr()
#define SECTOR_ERASE_COMMAND getSectorEraseComman()

uint8_t productType = ACTION_REPLAY_DS;

const char* productName() {
	switch(productType) {
		case GAMES_N_MUSIC:
			return "GAMES n' MUSIC";
		case ACTION_REPLAY_DS:
			return "ACTION REPLAY";
	}
	return "UNKNOWN";
}

static inline uint16_t getMainAddr() {
	switch(productType) {
		case GAMES_N_MUSIC:
			return 0x5555;
		case ACTION_REPLAY_DS:
			return 0x0AAA;
	}
	return 0;
}

static inline uint16_t getSecondAddr() {
	return getMainAddr() / 2;
}

static inline uint8_t getSectorEraseComman() {
	switch(productType) {
		case GAMES_N_MUSIC:
			return 0x30;
		case ACTION_REPLAY_DS:
			return 0x50;
	}
	return 0;
}

uint16_t getFlashSectorsCount() {
	switch(productType) {
		case GAMES_N_MUSIC:
			return 128;
		case ACTION_REPLAY_DS:
			// return 128;
			// return 512; // Flashing full chip disabled until corruption issue is found. // Dumps mirror at 256 sector count. Possible cause?
			return 256;
	}
	return 128;
}

static constexpr std::array gnm_flash_ids {
	uint16_t{0xD4BF},
	uint16_t{0xD5BF},
	uint16_t{0xD6BF},
	uint16_t{0xD7BF},
};

static constexpr std::array ards_flash_ids {
	uint16_t{0xC8BF},
	uint16_t{0xC9BF},
};

inline void writeSpiByte(uint8_t byte) {
	REG_AUXSPIDATA = byte;
	while ((REG_AUXSPICNT & CARD_SPI_BUSY) != 0);
}

inline uint8_t readSpiByte() {
	REG_AUXSPIDATA = 0;
	while ((REG_AUXSPICNT & CARD_SPI_BUSY) != 0);
	return REG_AUXSPIDATA;
}

void writeDataLines(uint8_t* buff, int len) {
	REG_AUXSPICNT = CARD_ENABLE | CARD_SPI_ENABLE | CARD_SPI_HOLD;

	writeSpiByte(0xE4);
	for(int i = 0; i < len; ++i) {
		writeSpiByte(buff[i]);
	}

	REG_AUXSPICNT = CARD_SPI_HOLD;
}

auto writeDataLine(uint8_t val) {
	return writeDataLines(&val, 1);
}

template<uint8_t command_byte>
void writeSpiu24(uint32_t word) {
	REG_AUXSPICNT = CARD_ENABLE | CARD_SPI_ENABLE | CARD_SPI_HOLD;

	writeSpiByte(command_byte);
	writeSpiByte(static_cast<uint8_t>(word >> 0x10));
	writeSpiByte(static_cast<uint8_t>(word >> 8));
	writeSpiByte(static_cast<uint8_t>(word));

	REG_AUXSPICNT = CARD_SPI_HOLD;
}

void writeAddrLine(uint address) {
	writeSpiu24<0xE3>(address);
}

void readBytesFromSpi(uint8_t* outBuff, int len) {
	REG_AUXSPICNT = CARD_ENABLE | CARD_SPI_ENABLE | CARD_SPI_HOLD;

	writeSpiByte(0xE7);

	for(int i = 0; i < len; ++i) {
		outBuff[i] = readSpiByte();
	}

	REG_AUXSPICNT = CARD_SPI_HOLD;
}

void readFromFlashAddress(uint32_t address, uint8_t* outBuff, uint32_t len) {
	writeAddrLine(address);
	readBytesFromSpi(outBuff, len);
}

void waitWriteDone() {
	REG_AUXSPICNT = CARD_ENABLE | CARD_SPI_ENABLE | CARD_SPI_HOLD;

	writeSpiByte(0xE8);

	uint8_t val = readSpiByte() & 0x40;

	while(true) {
		auto val2 = readSpiByte() & 0x40;
		if(val2 == val) {
			break;
		}
		val = val2;
	}

	REG_AUXSPICNT = CARD_SPI_HOLD;
}

void eraseSector(uint32_t sectorAddr) {
	writeAddrLine(MAIN_ADDR);
	writeDataLine(0xAA);

	writeAddrLine(OTHER_ADDR);
	writeDataLine(0x55);

	writeAddrLine(MAIN_ADDR);
	writeDataLine(0x80);

	writeAddrLine(MAIN_ADDR);
	writeDataLine(0xAA);

	writeAddrLine(OTHER_ADDR);
	writeDataLine(0x55);

	writeAddrLine(sectorAddr);
	writeDataLine(SECTOR_ERASE_COMMAND);

	waitWriteDone();
}

void eraseChip() {
	writeAddrLine(MAIN_ADDR);
	writeDataLine(0xAA);

	writeAddrLine(OTHER_ADDR);
	writeDataLine(0x55);

	writeAddrLine(MAIN_ADDR);
	writeDataLine(0x80);

	writeAddrLine(MAIN_ADDR);
	writeDataLine(0xAA);

	writeAddrLine(OTHER_ADDR);
	writeDataLine(0x55);

	writeAddrLine(MAIN_ADDR);
	writeDataLine(0x10);

	waitWriteDone();
}

void writeSector(uint32_t sectorAddr, uint8_t* sectorBuff) {
	for(int i = 0; i < 0x1000; ++i) {
		writeAddrLine(MAIN_ADDR);
		writeDataLine(0xAA);
		
		writeAddrLine(OTHER_ADDR);
		writeDataLine(0x55);
		
		writeAddrLine(MAIN_ADDR);
		writeDataLine(0xA0);
		
		writeAddrLine(sectorAddr + i);
		writeDataLine(sectorBuff[i]);
		
		waitWriteDone();
	}
}

void readSector(uint32_t sectorAddr, uint8_t* outBuff) {
	readFromFlashAddress(sectorAddr, outBuff, 0x1000);
}

uint16_t readChipID() {
	writeAddrLine(MAIN_ADDR);
	writeDataLine(0xAA);

	writeAddrLine(OTHER_ADDR);
	writeDataLine(0x55);

	writeAddrLine(MAIN_ADDR);
	writeDataLine(0x90);

	writeAddrLine(0);

	uint16_t ret;
	readBytesFromSpi(reinterpret_cast<uint8_t*>(&ret), 2);

	writeDataLine(0xF0);

	return ret;
}

u32 openSpi (uint8_t commandByte) {
	volatile u32 temp;

	C_REG_AUXSPICNTH = C_CARD_CR1_ENABLE | C_CARD_CR1_IRQ;
	C_REG_CARD_COMMAND[0] = 0xF2;
	C_REG_CARD_COMMAND[1] = 0x00;
	C_REG_CARD_COMMAND[2] = 0x00;
	C_REG_CARD_COMMAND[3] = 0x00;
	C_REG_CARD_COMMAND[4] = 0x00;
	C_REG_CARD_COMMAND[5] = commandByte;			// 0xCC == enable microSD ?
	C_REG_CARD_COMMAND[6] = 0x00;
	C_REG_CARD_COMMAND[7] = 0x00;
	C_REG_ROMCTRL = C_CARD_CR2_SETTINGS;

	while (REG_ROMCTRL & CARD_BUSY) {
		temp = C_REG_CARD_DATA_RD;
	}
	return temp;
}

uint16_t init() {
	productType = GAMES_N_MUSIC;
	openSpi(0);
	if(auto chip_id = readChipID(); std::find(gnm_flash_ids.begin(), gnm_flash_ids.end(), chip_id) != gnm_flash_ids.end()) {
		return chip_id;
	}

	productType = ACTION_REPLAY_DS;
	if(auto chip_id = readChipID(); std::find(ards_flash_ids.begin(), ards_flash_ids.end(), chip_id) != ards_flash_ids.end()) {
		return chip_id;
	}
	return 0xFFFF;
}

