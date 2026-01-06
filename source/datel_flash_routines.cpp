#include <stdio.h>
#include <array>
#include <algorithm>

#include <nds.h>
#include <fat.h>

#include "datel_flash_routines.h"
#include <array>
#include <algorithm>

#include <span>

typedef struct flash_chip_t {
	uint16_t id;
	const char* name;
	uint8_t foundInProducts;
	uint16_t commandSequenceOddAddress;
	uint8_t sectorEraseCommand;
	uint32_t sectorSize;
	uint16_t sectorCount;
} flash_chip_t;

namespace {

PROTOCOL_MODE protocolMode;

static flash_chip_t unknown_chip{0xFFFF, "Unknown", 0};

const flash_chip_t* selected_chip = &unknown_chip;

static constexpr std::array known_flash_chips {
	flash_chip_t{.id = 0xC8BF, .name = "SST39VF1681", .foundInProducts = ACTION_REPLAY_DSiME | ACTION_REPLAY_DS, .commandSequenceOddAddress = 0x0AAA, .sectorEraseCommand = 0x50, .sectorSize = 0x1000, .sectorCount = 512},
	flash_chip_t{.id = 0xC9BF, .name = "SST39VF1682", .foundInProducts = ACTION_REPLAY_DS   , .commandSequenceOddAddress = 0x0AAA, .sectorEraseCommand = 0x50, .sectorSize = 0x1000, .sectorCount = 512},

	// these flash chips also have top/bottom orientation with varying sector size
	flash_chip_t{.id = 0xC41C, .name = "EN29LV160BT", .foundInProducts = ACTION_REPLAY_DSiME, .commandSequenceOddAddress = 0x0AAA, .sectorEraseCommand = 0x30, .sectorSize = 0x10000, .sectorCount = 64},
	flash_chip_t{.id = 0x491C, .name = "EN29LV160BB", .foundInProducts = ACTION_REPLAY_DSiME, .commandSequenceOddAddress = 0x0AAA, .sectorEraseCommand = 0x30, .sectorSize = 0x10000, .sectorCount = 64},
	flash_chip_t{.id = 0xF61C, .name = "EN29LV320AT", .foundInProducts = ACTION_REPLAY_DSiME, .commandSequenceOddAddress = 0x0AAA, .sectorEraseCommand = 0x30, .sectorSize = 0x10000, .sectorCount = 64},
	flash_chip_t{.id = 0xF91C, .name = "EN29LV320AB", .foundInProducts = ACTION_REPLAY_DSiME, .commandSequenceOddAddress = 0x0AAA, .sectorEraseCommand = 0x30, .sectorSize = 0x10000, .sectorCount = 64},

	flash_chip_t{.id = 0xD4BF, .name = "SST39LF_VF512", .foundInProducts = GAMES_N_MUSIC    , .commandSequenceOddAddress = 0x5555, .sectorEraseCommand = 0x30, .sectorSize = 0x1000, .sectorCount = 16},
	flash_chip_t{.id = 0xD5BF, .name = "SST39LF_VF010", .foundInProducts = GAMES_N_MUSIC    , .commandSequenceOddAddress = 0x5555, .sectorEraseCommand = 0x30, .sectorSize = 0x1000, .sectorCount = 32},
	flash_chip_t{.id = 0xD6BF, .name = "SST39LF_VF020", .foundInProducts = GAMES_N_MUSIC    , .commandSequenceOddAddress = 0x5555, .sectorEraseCommand = 0x30, .sectorSize = 0x1000, .sectorCount = 64},
	flash_chip_t{.id = 0xD7BF, .name = "SST39LF_VF040", .foundInProducts = GAMES_N_MUSIC    , .commandSequenceOddAddress = 0x5555, .sectorEraseCommand = 0x30, .sectorSize = 0x1000, .sectorCount = 128},
};

static const flash_chip_t* get_flash_chip(uint16_t flash_id) {
	auto it = std::ranges::find_if(known_flash_chips, [&](const auto& flash_chip){
		return flash_chip.id == flash_id;
	});
	if(it != known_flash_chips.end()) {
		return &*it;
	}
	return &unknown_chip;
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

#define CARD_CR2_SETTINGS 0xA0586000

namespace ARDSiME {

void setSpiMode() {
	REG_AUXSPICNT = CARD_ENABLE | CARD_IRQ;
	REG_CARD_COMMAND[0] = 0xF0;
	REG_CARD_COMMAND[1] = 0x01;
	REG_CARD_COMMAND[2] = 0x00;
	REG_CARD_COMMAND[3] = 0x00;
	REG_CARD_COMMAND[4] = 0x00;
	REG_CARD_COMMAND[5] = 0x00;
	REG_CARD_COMMAND[6] = 0x00;
	REG_CARD_COMMAND[7] = 0x00;
	REG_ROMCTRL = CARD_CR2_SETTINGS;

	while (REG_ROMCTRL & CARD_BUSY) {
		volatile auto _ = REG_CARD_DATA_RD;
	}
}

#define SET_ADDRESS 0

#define COMMAND_SEQUENCE 1

#define AUTO_INCREMENT BIT(0)
#define WRITE_ENABLE BIT(1)
#define READ_ENABLE BIT(2)

struct spiGuard {
	spiGuard() {
		REG_AUXSPICNT = CARD_ENABLE | CARD_SPI_ENABLE | CARD_SPI_HOLD;
	}
	~spiGuard() {
		REG_AUXSPICNT = CARD_SPI_HOLD;
	}
};

inline auto SPI_TRANSACTION(uint8_t command_mode, auto function) {
	spiGuard _;
	writeSpiByte(command_mode);
	return function();
}

inline auto SPI_TRANSACTION(uint8_t command_mode) {
	return SPI_TRANSACTION(command_mode, [] -> void {});
}

struct spiCommandGuard {
	spiCommandGuard() {
		SPI_TRANSACTION(6);
	}
	~spiCommandGuard() {
		SPI_TRANSACTION(7);
	}
};

inline auto SPI_COMMAND(auto function) {
	struct spiCommandGuard {
		spiCommandGuard() {
			SPI_TRANSACTION(6);
		}
		~spiCommandGuard() {
			SPI_TRANSACTION(7);
		}
	} _;
	return function();
}

void setStartAddress(uint32_t startAddress) {
	SPI_TRANSACTION(SET_ADDRESS,
	[&] {
		auto addr = (uint8_t*)&startAddress;
		writeSpiByte(addr[0]);
		writeSpiByte(addr[1]);
		writeSpiByte(addr[2]);
	});
}

void sendCommandSequence(std::span<const uint8_t> seq) {
	SPI_TRANSACTION(COMMAND_SEQUENCE,
	[&] {
		for(auto byte : seq) {
			writeSpiByte(byte);
		}
	});
}

void waitWriteDone() {
	SPI_TRANSACTION(WRITE_ENABLE,
	[&] {
		uint8_t val = readSpiByte() & 0x40;
		while(true) {
			auto val2 = readSpiByte() & 0x40;
			if(val2 == val) {
				break;
			}
			val = val2;
		}
	});
}

void eraseChip() {
	SPI_COMMAND([&] {
		sendCommandSequence({0xAA, 0x55, 0x80, 0xAA, 0x55, 0x10});
		waitWriteDone();
	});
}

void eraseSector(uint32_t address) {
	SPI_COMMAND([&] {
		setStartAddress(address);
		sendCommandSequence({0xAA, 0x55, 0x80, 0xAA, 0x55});
		SPI_TRANSACTION(WRITE_ENABLE,
		[&] {
			// Erase command for this chip id
			writeSpiByte(selected_chip->sectorEraseCommand);
		});
		waitWriteDone();
	});
}

void readFromFlashAddress(uint32_t address, uint8_t* outBuff, uint32_t len) {
	SPI_COMMAND([&] {
		setStartAddress(address);
		SPI_TRANSACTION(READ_ENABLE | AUTO_INCREMENT,
		[&] {
			for (uint32_t i = 0; i < len; ++i) {
				outBuff[i] = readSpiByte();
			}
		});
	});
}

void writeSector(uint32_t address, uint8_t* inBuff) {
	SPI_COMMAND([&] {
		setStartAddress(address);
		for(int i = 0; i < 0x1000; ++i) {
			sendCommandSequence({0xAA, 0x55, 0xA0});

			SPI_TRANSACTION(WRITE_ENABLE | AUTO_INCREMENT,
			[&] {
				writeSpiByte(inBuff[i]);
			});

			waitWriteDone();
		}
	});
}

uint16_t readChipID_EON_Flash() {
	auto ret1 = SPI_COMMAND([&] {
		// Manufacturer ID
		setStartAddress(0x200);
		sendCommandSequence({0xAA, 0x55, 0x90});

		auto ret = SPI_TRANSACTION(READ_ENABLE,
		[&] {
			return readSpiByte();
		});

		SPI_TRANSACTION(WRITE_ENABLE,
		[&] {
			writeSpiByte(0xf0);
		});
		return ret;
	});

	auto ret2 = SPI_COMMAND([&] {
		// Device ID
		setStartAddress(2);
		sendCommandSequence({0xAA, 0x55, 0x90});

		auto ret = SPI_TRANSACTION(READ_ENABLE,
		[&] {
			return readSpiByte();
		});

		SPI_TRANSACTION(WRITE_ENABLE,
		[&] {
			writeSpiByte(0xf0);
		});
		return ret;
	});
	return ret1 << 8 | ret2;
}

uint16_t readChipID() {
	return SPI_COMMAND([&] {
		setStartAddress(0);
		sendCommandSequence({0xAA, 0x55, 0x90});

		auto ret = SPI_TRANSACTION(READ_ENABLE | AUTO_INCREMENT,
		[&] {
			auto ret1 = readSpiByte();
			auto ret2 = readSpiByte();
			return ret1 | (ret2 << 8);
		});

		SPI_TRANSACTION(WRITE_ENABLE,
		[&] {
			writeSpiByte(0xF0);
		});
		return ret;
	});

}
}

namespace GNM {

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

void writeAddrLine(uint32_t address) {
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
	const auto MAIN_ADDR = selected_chip->commandSequenceOddAddress;
	const auto OTHER_ADDR = selected_chip->commandSequenceOddAddress / 2;
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
	writeDataLine(selected_chip->sectorEraseCommand);

	waitWriteDone();
}

void eraseChip() {
	const auto MAIN_ADDR = selected_chip->commandSequenceOddAddress;
	const auto OTHER_ADDR = selected_chip->commandSequenceOddAddress / 2;
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
	const auto MAIN_ADDR = selected_chip->commandSequenceOddAddress;
	const auto OTHER_ADDR = selected_chip->commandSequenceOddAddress / 2;
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

uint16_t readChipID(uint32_t oddAddr) {
	writeAddrLine(oddAddr);
	writeDataLine(0xAA);

	writeAddrLine(oddAddr / 2);
	writeDataLine(0x55);

	writeAddrLine(oddAddr);
	writeDataLine(0x90);

	writeAddrLine(0);

	uint16_t ret;
	readBytesFromSpi(reinterpret_cast<uint8_t*>(&ret), 2);

	writeDataLine(0xF0);

	return ret;
}

void setSpiMode() {
	REG_AUXSPICNT = CARD_ENABLE | CARD_IRQ;
	REG_CARD_COMMAND[0] = 0xF2;
	REG_CARD_COMMAND[1] = 0x00;
	REG_CARD_COMMAND[2] = 0x00;
	REG_CARD_COMMAND[3] = 0x00;
	REG_CARD_COMMAND[4] = 0x00;
	REG_CARD_COMMAND[5] = 0x00;
	REG_CARD_COMMAND[6] = 0x00;
	REG_CARD_COMMAND[7] = 0x00;
	REG_ROMCTRL = CARD_CR2_SETTINGS;

	while (REG_ROMCTRL & CARD_BUSY) {
		volatile auto _ = REG_CARD_DATA_RD;
	}
}

}

}

bool init() {
	protocolMode = PROTOCOL_MODE::AR_DSiME;
	ARDSiME::setSpiMode();
	if(auto* flash_chip = get_flash_chip(ARDSiME::readChipID()); flash_chip->id != 0xFFFF) {
		selected_chip = flash_chip;
		return true;
	}
	if(auto* flash_chip = get_flash_chip(ARDSiME::readChipID_EON_Flash()); flash_chip->id != 0xFFFF) {
		selected_chip = flash_chip;
		return true;
	}
	protocolMode = PROTOCOL_MODE::GNM;
	GNM::setSpiMode();
	if(auto* flash_chip = get_flash_chip(GNM::readChipID(0x5555)); flash_chip->id != 0xFFFF) {
		selected_chip = flash_chip;
		return true;
	}
	if(auto* flash_chip = get_flash_chip(GNM::readChipID(0x0AAA)); flash_chip->id != 0xFFFF) {
		selected_chip = flash_chip;
		return true;
	}

	selected_chip = &unknown_chip;
	return false;
}

void writeSector(uint32_t sectorAddr, uint8_t* sectorBuff) {
	switch(protocolMode) {
		case PROTOCOL_MODE::AR_DSiME:
			return ARDSiME::writeSector(sectorAddr, sectorBuff);
		case PROTOCOL_MODE::GNM:
			return GNM::writeSector(sectorAddr, sectorBuff);
	}
}

void readSector(uint32_t sectorAddr, uint8_t* outBuff) {
	switch(protocolMode) {
		case PROTOCOL_MODE::AR_DSiME:
			return ARDSiME::readFromFlashAddress(sectorAddr, outBuff, 0x1000);
		case PROTOCOL_MODE::GNM:
			return GNM::readFromFlashAddress(sectorAddr, outBuff, 0x1000);
	}
}

void eraseChip() {
	switch(protocolMode) {
		case PROTOCOL_MODE::AR_DSiME:
			return ARDSiME::eraseChip();
		case PROTOCOL_MODE::GNM:
			return GNM::eraseChip();
	}
}

void eraseSector(uint32_t sectorAddr) {
	switch(protocolMode) {
		case PROTOCOL_MODE::AR_DSiME:
			return ARDSiME::eraseSector(sectorAddr);
		case PROTOCOL_MODE::GNM:
			return GNM::eraseSector(sectorAddr);
	}
}

const char* productName() {
	if(protocolMode == PROTOCOL_MODE::AR_DSiME) {
		return "Action Replay DS(i) (Media Edition)";
	}
	switch(selected_chip->foundInProducts & ~ACTION_REPLAY_DSiME) {
		case GAMES_N_MUSIC:
			return "Games n' Music";
		case ACTION_REPLAY_DS:
			return "Action Replay DS";
		default:
			return "unknown";
	}
}

const char* getFlashChipName() {
	return selected_chip->name;
}

uint16_t getFlashChipId() {
	return selected_chip->id;
}

uint16_t getFlashSectorsCount() {
	auto count = selected_chip->sectorCount;

	if(protocolMode == PROTOCOL_MODE::GNM) {
		return std::min<uint16_t>(256, count);
	}
	
	return count;
}

PROTOCOL_MODE getProtocolMode() {
	return protocolMode;
}
