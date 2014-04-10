/* Name: net_w5100.c
 * Author: .
 * Copyright: Arduino
 * License: GPL http://www.gnu.org/licenses/gpl-2.0.html
 * Project: eboot
 * Function: Network initialization
 * Version: 0.1 tftp / flashing functional
 */

#include <avr/io.h>
#include <avr/eeprom.h>

#include "spi.h"
#include "w5100.h"
#include "neteeprom.h"
#include "serial.h"
#include "debug.h"
#include "debug_net.h"

//#define W5100_ETHERNET_SHIELD // Arduino Ethenret Shield and Compatibles ...
#define W5200_ETHERNET_SHIELD // WIZ820io, W5200 Ethernet Shield 
//#define W5500_ETHERNET_SHIELD   // WIZ550io, ioShield series of WIZnet

#if defined(W5500_ETHERNET_SHIELD)
//#define WIZ550io_WITH_MACADDRESS // Use assigned MAC address of WIZ550io
#include "w5500.c"
#endif

#if defined(W5200_ETHERNET_SHIELD)
#include "w5200.c"
#endif

#if defined(W5100_ETHERNET_SHIELD)


uint8_t registerBuffer[REGISTER_BLOCK_SIZE] = {
	0x80,         // MR Mode - reset device

	// EEPROM block starts here
	GW_ADDR,      // GWR Gateway IP Address Register
	SUB_MASK,  // SUBR Subnet Mask Register
	MAC_ADDR,     // SHAR Source Hardware Address Register
	IP_ADDR,      // SIPR Source IP Address Register
	// EEPROM block ends here

	0, 0,         // Reserved locations
	0,            // IR Interrupt Register
	0,            // IMR Interrupt Mask Register
	0x07, 0xd0,   // RTR Retry Time-value Register
	0x80,         // RCR Retry Count Register
	0x55,         // RMSR Rx Memory Size Register, 2K per socket
	0x55          // TMSR Tx Memory Size Register, 2K per socket
};


void netInit(void)
{
	uint8_t i;

	/* Pull in altered network settings, if available,
	 * from AVR EEPROM (if signature bytes are set) */
	if((eeprom_read_byte(EEPROM_SIG_1) == EEPROM_SIG_1_VALUE)
		&& (eeprom_read_byte(EEPROM_SIG_2) == EEPROM_SIG_2_VALUE)) {

		for(i = 0; i < EEPROM_SETTINGS_SIZE; i++)
			registerBuffer[i + 1] = eeprom_read_byte(EEPROM_DATA + i);

		DBG_NET(tracePGMlnNet(mDebugNet_EEPROM);)

	} DBG_NET(
		else tracePGMlnNet(mDebugNet_BUILTIN);
	)


	DBG_NET(
		tracePGMlnNet(mDebugNet_ADDR);
		for(i = 15; i < 19; i++) {
			tracenet(registerBuffer[i]);
			if(i != 18) putch(0x2E);
		}
		tracePGMlnNet(mDebugNet_SUBN);
		for(i = 5; i < 9; i++) {
			tracenet(registerBuffer[i]);
			if(i != 8) putch(0x2E);
		}
		tracePGMlnNet(mDebugNet_GW);
		for(i = 1; i < 5; i++) {
			tracenet(registerBuffer[i]);
			if(i != 4) putch(0x2E);
		}
		tracePGMlnNet(mDebugNet_MAC);
		for(i = 9; i < 15; i++) {
			tracenet(registerBuffer[i]);
			if(i != 14) putch(0x2E);
		}
	)

	/** Configure Wiznet chip. Network settings */
//	for(i = 0; i < REGISTER_BLOCK_SIZE; i++)
//		spiWriteReg(i, registerBuffer[i]);

		spiWriteReg(0, registerBuffer[0]);


                spiWriteReg(0x001B, 0x55);//TSMR  W5100
                spiWriteReg(0x001A, 0x55);//RSMR  W5100
                
                
		for(i = 9; i < 15; i++) {
			spiWriteReg(i, registerBuffer[i]);
		}
		for(i = 15; i < 19; i++) {
			spiWriteReg(i, registerBuffer[i]);
		}
		for(i = 1; i < 5; i++) {
			spiWriteReg(i, registerBuffer[i]);
		}
		for(i = 5; i < 9; i++) {
			spiWriteReg(i, registerBuffer[i]);
		}

	DBG_NET(tracePGMlnNet(mDebugNet_DONE);)
}

#endif

// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;

