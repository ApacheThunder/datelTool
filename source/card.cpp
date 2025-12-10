#include <nds.h>

#include "card.h"

std::string_view gamename{"????"};
std::string_view gameid{"????"};

struct PaddedHeader {
    sNDSHeader header;
    char padding[0x200 - sizeof(sNDSHeader)];
} paddedHeader;

bool UpdateCardInfo(bool updateGlobalHeader = true) {
	if(isDSiMode()) {
		disableSlot1();
		for(int i = 0; i < 25; i++) { swiWaitForVBlank(); }
		enableSlot1();
		for(int i = 0; i < 15; i++) { swiWaitForVBlank(); }
	}
    cardReadHeader((u8*)&paddedHeader);
    if(*(uint32_t*)&paddedHeader == 0xffffffff)
        return false;
    if(updateGlobalHeader){
        gameid = {paddedHeader.header.gameCode, 4};
        gamename = {paddedHeader.header.gameTitle, 12};
    }
    return true;
}

bool CardIsPresent() {
    if(isDSiMode())
        return (REG_SCFG_MC & BIT(0)) == 0;
    static unsigned char counter = 0;

    //poll roughly every 30 frames
    return ((counter++) % 30) != 0 || UpdateCardInfo(true);
}

void WaitCardStabilize(){
    disableSlot1();
    for(size_t i = 0; i < 25; ++i) { swiWaitForVBlank(); }
    enableSlot1();
}

void WaitCard() {
    if(isDSiMode()) {
        if((REG_SCFG_MC & BIT(0)) == 1) {
			consoleClear();
            printf("No cartridge detected!\n\nPlease insert a cartridge to\ncontinue!\n");
            while((REG_SCFG_MC & BIT(0)) == 1)
                swiWaitForVBlank();
        }
        WaitCardStabilize();
    } else {
        (void)0;
    }
}

void WaitForNewCard(){
    do {
        WaitCard();
        if(UpdateCardInfo())
            break;
		consoleClear();
        printf("Cartridge not read properly!\n\nPlease reinsert it\n");
        do {
            swiWaitForVBlank();
        } while(isDSiMode() && !CardIsPresent());
    } while(true);
}

