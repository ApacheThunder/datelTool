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
uint8_t chipType = TYPE1;
uint16_t currentChipID = 0xFFFF;


void closeSpi() {
	REG_AUXSPICNT = 0x40;
}

int startAddress;

void openSpi (uint8_t commandByte) {
	volatile u32 temp;

	C_REG_AUXSPICNTH = C_CARD_CR1_ENABLE | C_CARD_CR1_IRQ;
	C_REG_CARD_COMMAND[0] = 0xF0;
	C_REG_CARD_COMMAND[1] = 0x01;
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
	REG_AUXSPICNT = 0xA040;
}
inline void writeSpiByte(uint8_t byte);
void initiateCommandSequence() {
	openSpi(0);
	writeSpiByte(6);
	closeSpi();
}

void terminateCommandSequence() {
	openSpi(0);
	writeSpiByte(7);
	closeSpi();
}

const char* productName() {
	switch(productType) {
		case GAMES_N_MUSIC:
			return "GAMES n' MUSIC";
		case ACTION_REPLAY_DS:
			return "ACTION REPLAY";
		default: 
			return "UNKNOWN";
	}
}

static inline uint16_t getMainAddr() {
	switch (chipType) {
		case TYPE1: 
			return 0x0AAA;
		case TYPE2: 
			return 0x5555;
		default:
			return 0;
	}
}

static inline uint16_t getSecondAddr() {
	return getMainAddr() / 2;
}

static inline uint8_t getSectorEraseComman() {
	switch (chipType) {
		case TYPE1: 
			return 0x50;
		case TYPE2: 
			return 0x30;
		default:
			return 0;
	}
}

uint16_t getFlashSectorsCount() {
	switch(productType) {
		case GAMES_N_MUSIC:
			return 128;
		case ACTION_REPLAY_DS:
			return 1024;
		default: 
			return 128;
	}
}

static constexpr std::array ards_flash_ids {
	uint16_t{0xC8BF},
	uint16_t{0xC9BF},
};

static constexpr std::array ards_type2_flash_ids {
	uint16_t{0xD8BF},
};

static constexpr std::array gnm_flash_ids {
	uint16_t{0xD4BF},
	uint16_t{0xD5BF},
	uint16_t{0xD6BF},
	uint16_t{0xD7BF},
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

void setStartAddress(uint32_t address) {
  openSpi(0);
  writeSpiByte(0);
  auto pbVar1 = (uint8_t*)&address;
  writeSpiByte(pbVar1[0]);
  writeSpiByte(pbVar1[1]);
  writeSpiByte(pbVar1[2]);
  closeSpi();
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
	return;
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
	return;
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

// void readSector(uint32_t sectorAddr, uint8_t* outBuff) {
	// readFromFlashAddress(sectorAddr, outBuff, 0x1000);
// }

void readSector(uint32_t addr, uint8_t* param_1) {
	uint16_t uVar1;
	int iVar2;

	initiateCommandSequence();
	setStartAddress(addr);
	openSpi(0);
	writeSpiByte(5);
	iVar2 = 0;
	do {
		*param_1 = (uint8_t)readSpiByte();
		iVar2 = iVar2 + 1;
		param_1 = param_1 + 1;
	} while (iVar2 != 0x1000);
	closeSpi();
	terminateCommandSequence();
}



uint16_t readChipID() {
	uint8_t ret[2]{};
	initiateCommandSequence();
	// startAddress = 1;
	setStartAddress(1);

	openSpi(0);
	writeSpiByte(1);
	writeSpiByte(0xAA);
	writeSpiByte(0x55);
	writeSpiByte(0x90);
	closeSpi();

	openSpi(0);
	writeSpiByte(4);
	ret[1] = readSpiByte();
	closeSpi();

	openSpi(0);
	writeSpiByte(2);
	writeSpiByte(0xF0);
	closeSpi();
	terminateCommandSequence();

	initiateCommandSequence();

	startAddress = 0;
	setStartAddress(0);

	openSpi(0);
	writeSpiByte(1);
	writeSpiByte(0xAA);
	writeSpiByte(0x55);
	writeSpiByte(0x90);
	closeSpi();

	openSpi(0);
	writeSpiByte(4);
	ret[0] = readSpiByte();
	closeSpi();

	openSpi(0);
	writeSpiByte(2);
	writeSpiByte(0xF0);
	closeSpi();
	terminateCommandSequence();


	return (ret[1] << 8) | ret[0];
}

uint16_t init() {
	productType = ACTION_REPLAY_DS;
	chipType = TYPE1;
	// return readChipID();
	// openSpi(0);
	if(auto chip_id = readChipID(); std::find(ards_flash_ids.begin(), ards_flash_ids.end(), chip_id) != ards_flash_ids.end()) {
		return chip_id;
	}
	
	chipType = TYPE2;
	if(auto chip_id = readChipID(); std::find(ards_type2_flash_ids.begin(), ards_type2_flash_ids.end(), chip_id) != ards_type2_flash_ids.end()) {
		return chip_id;
	}
	
	// Also type2
	productType = GAMES_N_MUSIC;
	if(auto chip_id = readChipID(); std::find(gnm_flash_ids.begin(), gnm_flash_ids.end(), chip_id) != gnm_flash_ids.end()) {
		return chip_id;
	}
	
	// Reset to defaults and exit as invalid.
	productType = ACTION_REPLAY_DS;
	chipType = TYPE1;
	return 0xFFFF;
}

uint16_t checkFlashID() {
	openSpi(0);
	return readChipID();
}

