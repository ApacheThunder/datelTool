#include <nds.h>

#include "bootsplash.h"

#include "topLogo.h"
#include "botConsole.h"

#include "font.h"

static PrintConsole tpConsole;
static PrintConsole btConsole;

static int bg;
static int bgSub;

bool TopSelected = false;

void SelectTopConsole() { consoleSelect(&tpConsole); }
void SelectBotConsole() { consoleSelect(&btConsole); }

void consoleClearTop(bool KeepTopSelected) {
	SelectTopConsole();
	consoleClear();
	if (!KeepTopSelected)SelectBotConsole();
}

void PrintToTop(const char* Message) {
	if (!TopSelected) { SelectTopConsole(); TopSelected = true; }
	printf(Message);
	SelectBotConsole();
	TopSelected = false;
}

void PrintToTop(const char* Message, int data, bool clearScreen) {
	if (!TopSelected) { SelectTopConsole(); TopSelected = true; }
	if (clearScreen)consoleClear();
	if (data != -1) { printf(Message, data); } else { printf(Message); }
	SelectBotConsole();
	TopSelected = false;
}

void PrintToTop(const char* Message, const char* SecondMessage, bool clearScreen) {
	if (!TopSelected) { SelectTopConsole(); TopSelected = true; }
	if (clearScreen)consoleClear();
	printf(Message, SecondMessage);
	SelectBotConsole();
	TopSelected = false;
}

void vramcpy_ui (void* dest, const void* src, int size) {
	u16* destination = (u16*)dest;
	u16* source = (u16*)src;
	while (size > 0) { *destination++ = *source++; size -= 2; }
}

void BootSplashInit() {
	videoSetMode(MODE_4_2D);
	videoSetModeSub(MODE_4_2D);
	vramSetBankA (VRAM_A_MAIN_BG);
	vramSetBankC (VRAM_C_SUB_BG);
	
	bg = bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 1, 0);
	bgSub = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 1, 0);
	
	consoleInit(&btConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 4, 6, false, false);
	consoleInit(&tpConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 4, 6, true, false);
		
	ConsoleFont font;
	font.gfx = (u16*)fontTiles;
	font.pal = (u16*)fontPal;
	font.numChars = 95;
	font.numColors =  fontPalLen / 2;
	font.bpp = 4;
	font.asciiOffset = 32;
	// font.convertSingleColor = true;
	consoleSetFont(&btConsole, &font);
	consoleSetFont(&tpConsole, &font);
	
	
	consoleSetWindow(&btConsole, 0, 6, 32, 20);
	consoleSetWindow(&tpConsole, 4, 20, 28, 8);
	
	// Load graphics after font or else you get palette conflicts. :P
	dmaCopy(topLogoBitmap, bgGetGfxPtr(bg), 256*256);
	dmaCopy(topLogoPal, BG_PALETTE, 256*2);
	dmaCopy(botConsoleBitmap, bgGetGfxPtr(bgSub), 256*256);
	dmaCopy(botConsolePal, BG_PALETTE_SUB, 256*2);
		
	// BG_PALETTE[1] = RGB15(31,31,31);
	BG_PALETTE_SUB[255] = RGB15(31,31,31);
	
	consoleSelect(&btConsole);
}

