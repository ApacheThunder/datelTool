#include <nds.h>
#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

#include "bootsplash.h"
#include "tonccpy.h"
#include "read_card.h"
#include "datel_flash_routines.h"

#define CONSOLE_SCREEN_WIDTH 32
#define CONSOLE_SCREEN_HEIGHT 20

#define SECTOR_SIZE (u32)0x1000

ALIGN(4) u8 CopyBuffer[SECTOR_SIZE * 0x200];

DTCM_DATA ALIGN(4) sNDSHeaderExt* cartHeader;

DTCM_DATA u32 NUM_SECTORS = 0x200;

DTCM_DATA ALIGN(4) u8 ReadBuffer[SECTOR_SIZE];

DTCM_DATA bool ErrorState = false;
DTCM_DATA bool fatMounted = false;

DTCM_DATA char gameTitle[13] = {0};

DTCM_DATA int ProgressTracker;
DTCM_DATA bool UpdateProgressText;

DTCM_DATA const char* textBuffer = "X------------------------------X\nX------------------------------X";
DTCM_DATA const char* textProgressBuffer = "X------------------------------X\nX------------------------------X";

extern uint8_t productType;


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
		// if(keysDown() & KEY_A) return;
		if(keysDown() != 0) return;
	}
}

void CardInit() {
	consoleClear();
	// Do cart init stuff to wake cart up.
	cardInit(cartHeader);
	DoWait(60);
	u16 chipID = init();
	iprintf("\nCart Chip Id: %4x \n", chipID);	
	switch(productType) {
		case GAME_N_MUSIC:
			NUM_SECTORS = 0x80;
			printf("Cart Type: Game n' Music\n\n");
			break;
		case ACTION_REPLAY_DS:
			NUM_SECTORS = 0x200;
			printf("Cart Type: Action Replay\n\n");
			break;
	}
}


void DoFlashDump() {
	consoleClear();
	DoWait(60);	
	iprintf("About to dump %d sectors.\n\n", (int)NUM_SECTORS);
	printf("Press [A] to continue\n");
	printf("Press [B] to abort\n");
	while(1) {
		swiWaitForVBlank();
		scanKeys();
		if(keysDown() & KEY_A)break;
		if(keysDown() & KEY_B)return;
	}
	if (!fatMounted) { DoFATerror(true); return; }
	FILE *dest = fopen("/datelTool/datel_rom.bin", "wb");
	
	if (dest == NULL) {
		printf("Error accessing datel_rom.bin!\n");
		printf("Press [B] to abort.\n");
		while(1) {
			swiWaitForVBlank();
			scanKeys();
			if(keysDown() & KEY_B)return;
		}
	}
	
	textBuffer = "Dumping sectors to datel_rom.bin\nPlease Wait...\n\n\n";
	textProgressBuffer = "Sectors Remaining: ";
	ProgressTracker = NUM_SECTORS;
	for (uint i = 0; i < (NUM_SECTORS * SECTOR_SIZE); i += SECTOR_SIZE) {
		readSector(i, ReadBuffer);
		fwrite(ReadBuffer, SECTOR_SIZE, 1, dest);
		if (ProgressTracker >= 0)ProgressTracker--;
		UpdateProgressText = true;
	}
	fflush(dest);
	fclose(dest);
	swiWaitForVBlank();
	while (UpdateProgressText)swiWaitForVBlank();
	consoleClear();
	printf("Flash dump finished!\n\n");
	printf("Press [A] to return to main menu\n");
	printf("Press [B] to exit\n");
	while(1) {
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
	DoWait(60);	
	iprintf("About to write %d sectors.\n\n", (int)NUM_SECTORS);
	printf("Press [A] to continue\n");
	printf("Press [B] to abort\n");
	while(1) {
		swiWaitForVBlank();
		scanKeys();
		if(keysDown() & KEY_A)break;
		if(keysDown() & KEY_B)return;
	}
	if (!fatMounted) { DoFATerror(true); return; }
	 
	FILE *src = fopen("/datelTool/datel_rom.bin", "rb");
	
	if (src == NULL) {
		printf("Error accessing datel_rom.bin!\n");
		printf("Press [B] to abort.\n");
		while(1) {
			swiWaitForVBlank();
			scanKeys();
			if(keysDown() & KEY_B)return;
		}
	}
	
	fseek(src, 0, SEEK_END);
    auto fileLength = ftell(src);
    fseek(src, 0, SEEK_SET);
	
	if ((u64)fileLength > (0x200 * SECTOR_SIZE)) {
		printf("datel_rom.bin file too large!\n");
		printf("Press [B] to abort.\n");
		while(1) {
			swiWaitForVBlank();
			scanKeys();
			if(keysDown() & KEY_B)return;
		}
	}
	
	printf("Reading datel_rom.bin ...\n\n");	
	fread(CopyBuffer, 1, fileLength, src);
	fclose(src);
	consoleClear();
	
	textBuffer = "Writing sectors to Datel Cart.\nPlease Wait...\n\n\n";
	textProgressBuffer = "Sectors Remaining: ";
	ProgressTracker = NUM_SECTORS;
	eraseChip();
	swiWaitForVBlank();
	for (uint i = 0; i < (NUM_SECTORS * SECTOR_SIZE); i += SECTOR_SIZE) {
		writeSector(i, CopyBuffer + i);
		if (ProgressTracker >= 0)ProgressTracker--;
		UpdateProgressText = true;
	}
	while (UpdateProgressText)swiWaitForVBlank();
	consoleClear();
	printf("Flash write finished!\n\n");
	printf("Press [A] to return to main menu\n");
	printf("Press [B] to exit\n");
	while(1) {
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
		iprintf("%d \n", ProgressTracker);
		UpdateProgressText = false;
	}
}

int MainMenu() {
	int Value = -1;
	toncset (CopyBuffer, 0xFF, 512);
	toncset (ReadBuffer, 0xFF, SECTOR_SIZE);
	// consoleClear();
	printf("Press [A] to dump flash\n\n");
	printf("Press [B] to write flash\n\n");
	printf("\nPress [X] or [Y] to exit\n");
	while(Value == -1) {
		swiWaitForVBlank();
		scanKeys();
		switch (keysDown()){
			case KEY_A: 	{ Value = 0; } break;
			case KEY_B: 	{ Value = 1; } break;
			case KEY_X: 	{ Value = 2; } break;
			case KEY_Y: 	{ Value = 2; } break;
		}
	}
	return Value;
}

int main() {
	defaultExceptionHandler();
	BootSplashInit();
	sysSetCartOwner (BUS_OWNER_ARM9);
	sysSetCardOwner (BUS_OWNER_ARM9);
	
	fatMounted = fatInitDefault();
	
	if (!fatMounted) {
		DoFATerror(true);
		consoleClear();
		return 0;
	}
	
	if(access("/datelTool", F_OK) != 0)mkdir("/datelTool", 0777); 
		
	// Enable vblank handler
	irqSet(IRQ_VBLANK, vblankHandler);
	
	
	if (!isDSiMode() && (REG_SCFG_EXT == 0)) {
		while(1) {
			swiWaitForVBlank();
			scanKeys();
			if(keysDown() == 0)break;;
		}
		printf("Insert target cart now.\n");
		printf("Press [A] to continue.\n");
		printf("Press [B] to aboart.\n");
		while(1) {
			swiWaitForVBlank();
			scanKeys();
			if(keysDown() & KEY_A)break;
			if(keysDown() & KEY_B)return 0;
		}
	} 
	
	consoleClear();
	CardInit();
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




