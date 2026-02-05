///////////////////////////////////////////////////////////////////////
// CW Decoder made by Hjalmar Skovholm Hansen OZ1JHM  VER 1.01       //
// Feel free to change, copy or what ever you like but respect       //
// that license is <a href="http://www.gnu.org/copyleft/gpl.html" title="http://www.gnu.org/copyleft/gpl.html" rel="nofollow">http://www.gnu.org/copyleft/gpl.html</a>              //
// Discuss and give great ideas on                                   //
// <a href="https://groups.yahoo.com/neo/groups/oz1jhm/conversations/messages" title="https://groups.yahoo.com/neo/groups/oz1jhm/conversations/messages" rel="nofollow">https://groups.yahoo.com/neo/groups/oz1jhm/conversations/messages</a> //
//                                                                   //
// Modifications by KC2UEZ. Bumped to VER 1.2:                       //
// Changed to work with the Arduino NANO.                            //
// Added selection of "Target Frequency" and "Bandwith" at power up. //
///////////////////////////////////////////////////////////////////////
 
///////////////////////////////////////////////////////////////////////////
// Read more here <a href="http://en.wikipedia.org/wiki/Goertzel_algorithm" title="http://en.wikipedia.org/wiki/Goertzel_algorithm" rel="nofollow">http://en.wikipedia.org/wiki/Goertzel_algorithm</a>        //
// if you want to know about FFT the <a href="http://www.dspguide.com/pdfbook.htm" title="http://www.dspguide.com/pdfbook.htm" rel="nofollow">http://www.dspguide.com/pdfbook.htm</a> //
///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
// Modified version by Kimio Ohe
// Modifications:
//  - port to UIAPduino Pro Micro(CH32V003)
//  - add freq detector function
//  - Refactoring of all source files
// Date: 2025-11-07
// このソフトウェアは GNU General Public License (GPL) に基づき配布されています。
// 改変版も同じ GPL ライセンスで再配布してください。
///////////////////////////////////////////////////////////////////////////
//
//
//	CW Decoder Ver for UIAPduino Pro Micro
//
//  2024.08.12 New Create
//  2024.08.21 integer version
//  2025.09.30 port to UIAPduino Pro Micro(CH32V003)
//  2025.10.04 add freq detector
//  2025.11.07 first release Version 1.0
//
//  Hardware Connections
//
//	UIAP	CH32V003  	SSD1306(SPI)	SW		MIC    	 ETC.
//	10	    PD0            DC
//	 8      PC6            MOSI
//   9      PC7            RES
//   7      PC5            SCK
//   5      PC3            CS
//	A1	    PA1                         SW1
//	A2      PC4                         SW2
//  A3      PD2							SW3
//	A0		PA2									OUT
//  A6		PD6											TEST
//	3.3V                  3.3V           		VCC(Via 78L33 3.3V)
//  GND                    GND          		GND
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "ch32v003fun.h"
#include "cw_decoder.h"
#include "frequencyDetector.h"

uint16_t sampling_period_us;
alignas(2) uint8_t  shared_buf[BUFSIZE];

static void tft_test_pattern(void)
{
	tft_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, BLACK);
	Delay_Ms(300);
	tft_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, WHITE);
	Delay_Ms(300);
	tft_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, RED);
	Delay_Ms(300);
	tft_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, GREEN);
	Delay_Ms(300);
	tft_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, BLUE);
	Delay_Ms(300);

	// 1px border
	tft_fill_rect(0, 0, TFT_WIDTH, 1, YELLOW);
	tft_fill_rect(0, TFT_HEIGHT - 1, TFT_WIDTH, 1, YELLOW);
	tft_fill_rect(0, 0, 1, TFT_HEIGHT, YELLOW);
	tft_fill_rect(TFT_WIDTH - 1, 0, 1, TFT_HEIGHT, YELLOW);

	// crosshair
	tft_fill_rect(TFT_WIDTH / 2, 0, 1, TFT_HEIGHT, CYAN);
	tft_fill_rect(0, TFT_HEIGHT / 2, TFT_WIDTH, 1, CYAN);
	Delay_Ms(5000);

	// text
	tft_set_background_color(BLACK);
	tft_set_color(WHITE);
	tft_set_cursor(0, 0);
	tft_print("0123456789", FONT_SCALE_8X8);

	tft_set_cursor(TFT_WIDTH - (4 * 8), TFT_HEIGHT - 8);
	tft_print("WXYZ", FONT_SCALE_8X8);
}

//==================================================================
//	main
//==================================================================
int main()
{
	int8_t *vReal;
	int8_t *vImag;

	SystemInit();				// ch32v003 Setup
	GPIO_setup();				// gpio Setup;
    tft_init();					// LCD init
	tft_test_pattern();

	vReal = (int8_t *)&shared_buf[0];
	vImag = (int8_t *)&shared_buf[128];
	while (1) {
		// cw decoder
		cwd_setup();			// freq detector Setup
		cwDecoder();			// run cw decoder
		// frequency detector
		fd_setup();				// freq detector Setup
		freqDetector(vReal, vImag);			// run freq counter
	}
	return 0;
}


