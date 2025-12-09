#include <nds.h>
#include <nds/bios.h>
#include <string.h>

volatile bool exitflag = false;

void powerButtonCB() { exitflag = true; }

void VcountHandler() { inputGetAndSend(); }

void VblankHandler(void) { }


int main() {
	readUserSettings();
	
	ledBlink(0);
	irqInit();
	
	// Start the RTC tracking IRQ
	initClockIRQ();
	touchInit();	
	fifoInit();
	
	SetYtrigger(80);	
	
	installSystemFIFO();
	
	irqSet(IRQ_VCOUNT, VcountHandler);
	irqSet(IRQ_VBLANK, VblankHandler);
	
	irqEnable( IRQ_VBLANK | IRQ_VCOUNT );
	
	setPowerButtonCB(powerButtonCB);
	
	// Keep the ARM7 mostly idle
	while (!exitflag) {
		if (0 == (REG_KEYINPUT & (KEY_SELECT | KEY_START | KEY_L | KEY_R))) { exitflag = true; }
		swiWaitForVBlank();
	}
	return 0;
}

