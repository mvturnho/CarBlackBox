/*
 * Display.h
 *
 *  Created on: 23 dec. 2014
 *      Author: m.vanturnhout
 */

#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <SPI.h>
#include <ILI9341_t3.h>

// For the Adafruit shield, these are the default.
#define TFT_DC     15
#define TFT_CS     6
#define TFT_MOSI   11
#define TFT_CLK    13
#define TFT_RST    14
#define TFT_MISO   12

class Display {
public:
	typedef enum {
		main_scr, sat_scr
	} mode;
	mode screen_mode = sat_scr;

	// Use hardware SPI (on Uno, #13, #12, #11)
	ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST);

	Display();
	virtual ~Display();
};

#endif /* DISPLAY_H_ */
