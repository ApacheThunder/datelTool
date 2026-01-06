#ifndef DATEL_FLASH_ROUTINES
#define DATEL_FLASH_ROUTINES

#include <nds/ndstypes.h>

enum class PROTOCOL_MODE {
	GNM,
	AR_DSiME,
};

enum DATEL_TYPE {
	GAMES_N_MUSIC = 1,
	ACTION_REPLAY_DS = 2,
	ACTION_REPLAY_DSiME = 4,
};

bool init();
PROTOCOL_MODE getProtocolMode();
void writeSector(uint32_t sectorAddr, uint8_t* sectorBuff);
void readSector(uint32_t sectorAddr, uint8_t* outBuff);
void eraseChip();
void eraseSector(uint32_t sectorAddr);
const char* productName();
const char* getFlashChipName();
uint16_t getFlashChipId();
uint16_t getFlashSectorsCount();

#endif // DATEL_FLASH_ROUTINES

