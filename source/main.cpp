#include <nds.h>
#include <array>
#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

#include "bootsplash.h"
#include "card.h"
#include "datel_flash_routines.h"

#define CONSOLE_SCREEN_WIDTH 32
#define CONSOLE_SCREEN_HEIGHT 20

#define SECTOR_SIZE (u32)0x1000

extern void consoleClearTop(bool KeepTopSelected);
extern void PrintToTop(const char* Message);
extern void PrintToTop(const char* Message, int data, bool clearScreen);
extern void PrintToTop(const char* Message, const char* SecondMessage, bool clearScreen);
extern uint8_t productType;

DTCM_DATA u32 NUM_SECTORS = 0x200;

DTCM_DATA ALIGN(16) std::array<u8, SECTOR_SIZE> ReadBuffer;
DTCM_DATA ALIGN(16) std::array<u8, SECTOR_SIZE> WriteBuffer;

DTCM_DATA bool ErrorState = false;
DTCM_DATA bool fatMounted = false;
DTCM_DATA bool cardEjected = false;
DTCM_DATA bool activeIO = false;
DTCM_DATA bool isDSi = false;

DTCM_DATA char gameTitle[13] = {0};

DTCM_DATA int ProgressTracker;
DTCM_DATA bool UpdateProgressText;

DTCM_DATA const char* textBuffer = "X------------------------------X\nX------------------------------X";
DTCM_DATA const char* textProgressBuffer = "X------------------------------X\nX------------------------------X";

DTCM_DATA const char* ValidPaths[] = {
	"/datelTool/datel-backup.bin",
	"/datelTool/ards-backup.bin",
	"/datelTool/gnm-backup.bin"
};


const char* DumpFilePath() {
	switch (productType) {
		case ACTION_REPLAY_DS: 
			return ValidPaths[1];
		case GAMES_N_MUSIC:
			return ValidPaths[2];
		default:
			return ValidPaths[0];
	}
}


void DoWait(int waitTime = 30) {
	if (waitTime > 0)for (int i = 0; i < waitTime; i++) { swiWaitForVBlank(); }
}

void DoFATerror(bool isFatel) {
	consoleClear();
	printf("FAT Init Failed!\n");
	ErrorState = isFatel;
	while(1) {
		swiWaitForVBlank();
		scanKeys();
		if(keysDown() == 0) break;
	}
	while(1) {
		swiWaitForVBlank();
		scanKeys();
		if(keysDown() != 0) return;
	}
}

u16 CardInit() {
	consoleClear();
	u16 chipID = init();
	PrintToTop("Cart Chip Id: %4X \n\n", chipID, true);
	PrintToTop("Cart Type: %s\n", productName(), false);
	NUM_SECTORS = getFlashSectorsCount();
	return chipID;
}


void DoFlashDump() {
	consoleClear();
	DoWait(60);	
	printf("About to dump %d sectors.\n\n", (int)NUM_SECTORS);
	printf("Press [A] to continue\n");
	printf("Press [B] to abort\n");
	while(1) {
		if (cardEjected)return;
		swiWaitForVBlank();
		scanKeys();
		if(keysDown() & KEY_A)break;
		if(keysDown() & KEY_B)return;
	}
	
	consoleClear();
	
	if (!fatMounted) { DoFATerror(true); return; }
	FILE *dest = fopen(DumpFilePath(), "wb");
	
	if (dest == NULL) {
		printf("Error accessing dump file!\n\n");
		printf("Press [B] to abort.\n");
		while(1) {
			swiWaitForVBlank();
			scanKeys();
			if(keysDown() & KEY_B)return;
		}
	}
	
	textBuffer = "Dumping sectors to file.\n\n\nPlease Wait...\n\n\n";
	textProgressBuffer = "Sectors Remaining: ";
	ProgressTracker = NUM_SECTORS;
	activeIO = true;
	for (uint i = 0; i < (NUM_SECTORS * SECTOR_SIZE); i += SECTOR_SIZE) {
		if (isDSi && cardEjected) {
			activeIO = false;
			fflush(dest);
			fclose(dest);
			return;
		}
		readSector(i, ReadBuffer.data());
		fwrite(ReadBuffer.data(), SECTOR_SIZE, 1, dest);
		if (ProgressTracker >= 0)ProgressTracker--;
		UpdateProgressText = true;
	}
	activeIO = false;
	fflush(dest);
	fclose(dest);
	swiWaitForVBlank();
	while (UpdateProgressText)swiWaitForVBlank();
	if (cardEjected)return;
	consoleClear();
	printf("Flash dump finished!\n\n");
	printf("Press [A] to return to main menu\n");
	printf("Press [B] to exit\n");
	while(1) {
		if (cardEjected)return;
		swiWaitForVBlank();
		scanKeys();
		if(keysDown() & KEY_A) return;
		if(keysDown() & KEY_B) { 
			ErrorState = true;
			return;
		}
	}
}

void DoFlashWrite() {
	consoleClear();
	
	if (!fatMounted) { DoFATerror(true); return; }
	 
	FILE *src = fopen(DumpFilePath(), "rb");
	
	if (src == NULL) {
		printf("Error accessing dump file!\n");
		printf("Press [B] to abort.\n");
		while(1) {
			if (cardEjected)return;
			swiWaitForVBlank();
			scanKeys();
			if(keysDown() & KEY_B)return;
		}
	}
	
	fseek(src, 0, SEEK_END);
    u32 fileLength = ftell(src);
    fseek(src, 0, SEEK_SET);
	
	u32 wholeSectorsToWrite = fileLength / SECTOR_SIZE;
	u32 leftoverBytes = fileLength % SECTOR_SIZE;
	u32 totalSectors = wholeSectorsToWrite + (leftoverBytes != 0);
	
	if (totalSectors > NUM_SECTORS) {
		consoleClear();
		printf("Flash dump too big!\n");
		printf("Press [B] to abort.\n");
		while(1) {
			if (isDSi && cardEjected)return;
			swiWaitForVBlank();
			scanKeys();
			if(keysDown() & KEY_B)return;
		}
	}

	consoleClear();
	DoWait(60);	
	printf("About to write %d sectors.\n\n", (int)totalSectors);
	printf("Press [A] to continue\n");
	printf("Press [B] to abort\n");
	while(1) {
		if (isDSi && cardEjected)return;
		swiWaitForVBlank();
		scanKeys();
		if(keysDown() & KEY_A)break;
		if(keysDown() & KEY_B)return;
	}

	const char* finalMessage = "Flash write finished!\n\n";
	textBuffer = "Writing file to Datel Cart.\n\n\nPlease Wait...\n\n\n";
	textProgressBuffer = "Sectors Remaining: ";
	ProgressTracker = totalSectors;
	activeIO = true;
	for (u32 sector = 0; sector < wholeSectorsToWrite; ++sector) {
		if (isDSi && cardEjected) {
			activeIO = false;
			fclose(src);
			return;
		}
		u32 read = fread(WriteBuffer.data(), SECTOR_SIZE, 1, src);
		if(read != 1) {
			finalMessage = "Flash write failed!\n\n";
			goto error;
		}
		readSector(sector * SECTOR_SIZE, ReadBuffer.data());
		if(ReadBuffer != WriteBuffer) {
			eraseSector(sector * SECTOR_SIZE);
			writeSector(sector * SECTOR_SIZE, WriteBuffer.data());
		}
		ProgressTracker--;
		UpdateProgressText = true;
	}
	if(leftoverBytes != 0) {
		readSector(wholeSectorsToWrite * SECTOR_SIZE, ReadBuffer.data());
		WriteBuffer = ReadBuffer;
		u32 read = fread(WriteBuffer.data(), leftoverBytes, 1, src);
		if(read != 1) {
			finalMessage = "Flash write failed!\n\n";
			goto error;
		}
		if(ReadBuffer != WriteBuffer) {
			eraseSector(wholeSectorsToWrite * SECTOR_SIZE);
			writeSector(wholeSectorsToWrite * SECTOR_SIZE, WriteBuffer.data());
		}
		ProgressTracker--;
		UpdateProgressText = true;
	}
	error:
	activeIO = false;
	while (UpdateProgressText)swiWaitForVBlank();
	consoleClear();
	printf(finalMessage);
	printf("Press [A] to return to main menu\n");
	printf("Press [B] to exit\n");
	while(1) {
		if (cardEjected)return;
		swiWaitForVBlank();
		scanKeys();
		if(keysDown() & KEY_A) return;
		if(keysDown() & KEY_B) { 
			ErrorState = true;
			return;
		}
	}
}


void vblankHandler (void) {
	if (UpdateProgressText) {
		consoleClear();
		printf(textBuffer);
		printf(textProgressBuffer);
		printf("%d \n", ProgressTracker);
		UpdateProgressText = false;
	}
	
	if (isDSi) {
		if (!cardEjected && REG_SCFG_MC == 0x11) {
			consoleClearTop(false);
			cardEjected = true;
		}
	}/* else { // Causes screen flicker if used here.
		if (!activeIO && cardEjected)cardEjected = CardIsPresent();
	}*/
}

void DoCardWait() {
	consoleClearTop(false);
	while(true) {
		if(CardInit() != 0xFFFF) return;
		WaitForNewCard();
		consoleClear();
		consoleClearTop(false);
		PrintToTop("Cart Chip Id: %4X \n\n", checkFlashID(), true);
		PrintToTop("Cart Type: UNKNOWN");
		printf("Not a supported cart!\n\nInsert it again...");
	}
}


void PrintMainMenuText() {
	printf("Press [A] to dump flash\n\n");
	printf("Press [X] or [Y] to write flash\n\n");
	printf("\nPress [B] to exit\n");
}

int MainMenu() {
	int Value = -1;
	memset(ReadBuffer.data(), 0xFF, SECTOR_SIZE);
	consoleClear();
	PrintMainMenuText();
	while(Value == -1) {
		if (cardEjected) {
			consoleClear();
			DoCardWait();
			PrintMainMenuText();
			cardEjected = false;
		}
		swiWaitForVBlank();
		scanKeys();
		switch (keysDown()){
			case KEY_A: 	{ Value = 0; } break;
			case KEY_X: 	{ Value = 1; } break;
			case KEY_Y: 	{ Value = 1; } break;
			case KEY_B: 	{ Value = 2; } break;
		}
	}
	return Value;
}

int main() {
	defaultExceptionHandler();
	BootSplashInit();
	sysSetCardOwner (BUS_OWNER_ARM9);
	
	isDSi = isDSiMode();
	
	fatMounted = fatInitDefault();
	
	if (!fatMounted) {
		DoFATerror(true);
		consoleClear();
		return 0;
	}
	
	if(access("/datelTool", F_OK) != 0) mkdir("/datelTool", 0777); 
		
	// Enable vblank handler
	irqSet(IRQ_VBLANK, vblankHandler);

	DoCardWait();
	
	while(1) {
		switch (MainMenu()) {
			case 0: { DoFlashDump(); } break;
			case 1: { DoFlashWrite(); } break;
			case 2: { ErrorState = true; } break;
		}
		if (ErrorState) {
			consoleClear();
			break;
		}
		swiWaitForVBlank();
    }
	return 0;
}

