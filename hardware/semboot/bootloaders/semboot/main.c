/* Name: main.c
 * Author: .
 * Copyright: Arduino
 * License: GPL http://www.gnu.org/licenses/gpl-2.0.html
 * Project: tftpboot
 * Function: Bootloader core
 * Version: 0.2 support for USB flashing
 */
#include <avr/eeprom.h>
#include <avr/boot.h>
#include <avr/wdt.h>
#include <util/delay.h>

#include "util.h"
#include "spi.h"
#include "w5100.h"
#include "tftp.h"
#include "serial.h"
#include "neteeprom.h"
#include "watchdog.h"
#include "debug.h"
#include "debug_main.h"
#if defined(ANNOUNCE)
	#include "announce.h"
#endif


int main(void) __attribute__((naked)) __attribute__((section(".init9")));
//void (*appStart)(void) __attribute__((naked)) = 0x0000;
//void (*appStart)(void) = 0x0000;

int main(void)
{
	/* Disable the watchdog timer to prevent
	 * eternal reset loop of doom and despair */
	uint8_t ch = MCUSR;
	MCUSR = 0;
	wdt_disable();

	// Wait to ensure startup of W5100
	_delay_ms(300);
    
    LED_DDR |= _BV(LED);
    LED_DDR |= _BV(PIND4);
    
    if (!(ch & _BV(WDRF))) {  //If not watchdogreset, reset wiznet chip
        
        LED_PORT ^= _BV(LED);
        //(PINC & (1<<3)
        DDRC = _BV(3);
        PORTC = _BV(3); //Ensure PINC5 is HIGH
        _delay_ms(100);
        PORTC = 0b00000000; //Pull PINC5 LOW
        _delay_ms(100);
        PORTC = _BV(3); //PINC5 HIGH
        _delay_ms(300);
        //WDTCSR = _BV(WDCE) | _BV(WDE);
        //WDTCSR = _BV(WDP2) | _BV(WDE); //Enable watchdog timeout 125ms
        //LED_PORT &= ~_BV(LED);
        //while(1);
    }
	/* This code makes the following assumptions:
	 * No interrupts will execute
	 * SP points to RAMEND
	 * r1 contains zero */
	//cli();
	asm volatile("clr __zero_reg__");
#if defined(__AVR_ATmega8__)
	SP = RAMEND;  // This is done by hardware reset
#endif

	// Prescaler=0, ClkIO Period = 62,5ns
	// TCCR1B values:
	// 0x01 -> ClkIO/1 -> 62,5ns period, 4ms max
	// 0x02 -> ClkIO/8 -> 500ns period, 32ms max
	// 0X03 -> ClkIO/64 -> 4us period, 256ms max
	// 0x04 -> ClkIO/256 -> 16us period, 1024ms max
	// 0x05 -> ClkIO/1024 -> 64us period, 4096ms max
	// Set up Timer 1 as timekeeper for LED flashing
	TCCR1B = _BV(CS12) | _BV(CS10); // Same thing as TCCR1B = 0x05;

	/* Write version information in the EEPROM */
	if(eeprom_read_byte(EEPROM_MAJVER) != ARIADNE_MAJVER)
		eeprom_write_byte(EEPROM_MAJVER, ARIADNE_MAJVER);
	if(eeprom_read_byte(EEPROM_MINVER) != ARIADNE_MINVER)
		eeprom_write_byte(EEPROM_MINVER, ARIADNE_MINVER);
	
    uint8_t updateFlag = 0;
    
    
    if (!((PIND & (1<<5)) == (1<<5)) ) { //update button pressed and reset to default IP
        updateFlag = 1;
#if (W5200 > 0)
#if defined(__AVR_ATmega1284P__)
        static uint16_t last_timer_2;
        static uint16_t tick2 = 0;
        
        while (!((PIND & (1<<5)) == (1<<5)) ) {
            uint16_t next_timer_2 = TCNT1;
            if(next_timer_2 & 0x400) {
                
                if ( (PIND & _BV(PIND4)) == _BV(PIND4) ) {//if blue led is on
                    LED_PORT &= ~_BV(PIND4); // Led pin low
                }
                else {
                    LED_PORT ^= _BV(PIND4);// Led pin high
                }
                
            }

            if(next_timer_2 < last_timer_2) {
                tick2++;
            }
            
            last_timer_2 = next_timer_2;
            
            
            if (tick2 > 2) {
                LED_PORT ^= _BV(LEDBLUE);
                uint8_t netBuffer[20] = {
                    0xFF,         // default SIG1
                    0xFF,         // default SIG2
                    GW_ADDR,      // GWR Gateway IP Address Register
                    SUB_MASK,      // SUBR Subnet Mask Register
                    MAC_ADDR,     // SHAR Source Hardware Address Register
                    IP_ADDR,      // SIPR Source IP Address Register
                };
                for (uint8_t a = 3;a < 23;a++) {
                    eeprom_write_byte(&a, netBuffer[a-3]);
                }
                
                _delay_ms(1000);
                LED_PORT &= ~_BV(LEDBLUE);
                break;
            }
            
        }
        LED_PORT &= ~_BV(LEDBLUE);
#endif
#endif

    }
    else if( (eeprom_read_byte(EEPROM_UPDATE_FLAG) == 1) || eeprom_read_byte(EEPROM_IMG_STAT) != EEPROM_IMG_OK_VALUE) {
        //If the update flag was set from userspace || the button 'default' is being pressed || no valid image has been written => no timeout will occur
 		updateFlag = 1;
 		eeprom_write_byte(EEPROM_UPDATE_FLAG, 0);//Reset update flag so that next reboot normal boot continues
 	}
    else {
		appStart();
 	}

	//Initialize UART communication
	serialInit();
	DBG_MAIN(tracePGMlnMain(mDebugMain_TITLE);)

	DBG_BTN(
		DBG_MAIN_EX(tracePGMlnMain(mDebugMain_BTN);)
		buttonInit();
	)

	/** Initalize SPI communication */
	DBG_MAIN_EX(tracePGMlnMain(mDebugMain_SPI);)
	spiInit();
	/** Initialize networking */
	DBG_MAIN_EX(tracePGMlnMain(mDebugMain_NET);)
	netInit();

	// Initialize the UDP socket for tftp
	DBG_MAIN_EX(tracePGMlnMain(mDebugMain_TFTP);)
	tftpInit();

	/* This code is to be used with the java-client inherited from the
	 * Arduino project. We don't support it and it adds about
	 * 600 bytes to the binary. So off it goes */
#if defined(ANNOUNCE)
	DBG_MAIN_EX(tracePGMlnMain(mDebugMain_ANN);)
	announceInit();
#endif

	serialFlashing = FALSE;
	tftpFlashing = FALSE;

	for(;;) {
		// If there is no serial flashing under way, poll tftp
		if(!serialFlashing)
			// If tftp recieved a FINAL_ACK, break
			if(tftpPoll() == 0)
				break;

		// If there is no tftp flashing, poll serial
		//if(!tftpFlashing)
			// If flashing is done exit
		//	if(serialPoll() == 0)
		//		break;

		/* As explained above this goes out */
#if defined(ANNOUNCE)
		announcePoll();
#endif

		if((timedOut()) && !(updateFlag == 1)) {
			if(eeprom_read_byte(EEPROM_IMG_STAT) == EEPROM_IMG_OK_VALUE) break;

			//TODO: determine the conditions for reseting server OR reseting socket
			if(tftpFlashing == TRUE) {
				// Delete first page of flash memory
				boot_page_erase(0);
				// Reinitialize TFTP
				tftpInit();
				// Reset the timeout counter
				resetTick();
				// Unset tftp flag
				tftpFlashing = FALSE;
			}
		}
		/* Blink the notification led */
		updateLed();
	}

	/* Exit to user application */
	WDTCSR = _BV(WDCE) | _BV(WDE);
 	WDTCSR = _BV(WDP2) | _BV(WDE); //Enable watchdog timeout 125ms
 	while(1); 	//endless loop, this triggers a reset needed to properly reset the Wiznet chip
 			//The bootloader recognizes the watchdog reset and skips to the user application
	//return(0); /* never reached */
}
