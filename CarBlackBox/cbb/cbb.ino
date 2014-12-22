/***************************************************
 This is our GFX example for the Adafruit ILI9341 Breakout and Shield
 ----> http://www.adafruit.com/products/1651

 Check out the links above for our tutorials and wiring diagrams
 These displays use SPI to communicate, 4 or 5 pins are required to
 interface (RST is optional)
 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 Written by Limor Fried/Ladyada for Adafruit Industries.
 MIT license, all text above must be included in any redistribution
 ****************************************************/

//#include <HardwareSerial.h>
//#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <SPI.h>
#include <ILI9341_t3.h>
#include <Bounce2.h>
#include <Wire.h>
#include <OBD.h>
#include <SdFat.h>
#include <EEPROM.h>
#include <math.h>

#include "graphics.h"
#include "logging.h"
#include "obd.h"

#define DEBUG

// For the Adafruit shield, these are the default.
#define TFT_DC     15
#define TFT_CS     6
#define TFT_MOSI   11
#define TFT_CLK    13
#define TFT_RST    14
#define TFT_MISO   12

#define BUTTON_PIN 5

//Pin Definitions
#define nss Serial3
//SoftwareSerial nss(GPS_RxPin, GPS_TxPin);

// Use hardware SPI (on Uno, #13, #12, #11) 
ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST);
// Instantiate a Bounce object
Bounce debouncer = Bounce();

TinyGPSPlus gps;
// File system object.
SdFat sd;
// Log file.
SdFile file;
// Time in micros for next data record.
//uint32_t logTime;

float gpsangle = -1.0;
char fileName[14];

typedef enum {
	main_scr, sat_scr
} mode;
mode screen_mode = sat_scr;

bool has_sd = false;
bool blink = true;
volatile bool button_state = false;

TinyGPSCustom totalGPGSVMessages(gps, "GPGSV", 1); // $GPGSV sentence, first element
TinyGPSCustom messageNumber(gps, "GPGSV", 2); // $GPGSV sentence, second element
TinyGPSCustom satsInView(gps, "GPGSV", 3);     // $GPGSV sentence, third element
TinyGPSCustom satNumber[4]; // to be initialized later
TinyGPSCustom elevation[4];
TinyGPSCustom azimuth[4];
TinyGPSCustom snr[4];

//struct satelite {
//	bool active;
//	int elevation;
//	int azimuth;
//	int snr;
//	int run;
//	int old_x;
//	int old_y;
//} sats[MAX_SATELLITES];

// Error messages stored in flash.
#define error(msg) error_P(PSTR(msg))
//------------------------------------------------------------------------------
void error_P(const char* msg) {
	//sd.errorHalt_P(msg);
	has_sd = false;
	sprintf(fileName, msg);
}

void button_pressed() {
	debouncer.update();
	if (debouncer.read() == LOW)
		button_state = true;
}

void setup() {

#if defined(DEBUG)
	Serial.begin(115200);
#endif
	pinMode(BUTTON_PIN, INPUT);
	digitalWrite(BUTTON_PIN, HIGH);
	attachInterrupt(BUTTON_PIN, button_pressed, LOW);
	// After setting up the button, setup debouncer
	debouncer.attach(BUTTON_PIN);
	debouncer.interval(5);

	//initialize TFT display
	tft.begin();
	tft.fillScreen(ILI9341_BLACK);
	bootText();
	delay(200);

	tft.setRotation(0);	// Initialize the SD card at SPI_HALF_SPEED to avoid bus errors with

	initSystem();

	delay(2000);
//	screen_mode = main_scr;
	screen_mode = sat_scr;

	if (screen_mode == sat_scr) {
		drawSatScreen();
	} else {
		drawMetricScreen();
	}
}

void loop(void) {
	unsigned long start = millis();

	drawActiveScreen();

	while (((millis() - start) < 997)) {
		feedgps();
		if (obd_connected == true && screen_mode == main_scr) {
			readRealTimeObd();
			drawRealTimeData();
		} else if (screen_mode == sat_scr && gps.course.isValid() && gpsangle != gps.course.deg()) {
			drawOpenDot(gpsangle, 270, 98, ILI9341_BLACK);
//			gpsangle += 1;
//			drawOpenDot(gpsangle, 270, 98, ILI9341_WHITE);
			drawOpenDot(gps.course.deg(), 270, 98, ILI9341_WHITE);
			gpsangle = gps.course.deg();
		}
		switchScreen();
	}

	updateObd();

	logData();
}

void initSystem() {
	has_sd = openFile(fileName);

#if defined(DEBUG)
	Serial.print(F("Logging to: "));
	Serial.println(fileName);
	Serial.println(F("Type any character to stop"));
#endif
	// Write data header.
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.print("init new log file    ... ");
	writeHeader();
	tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
	tft.println("done");

	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.print("init gps Serial1     ... ");
	nss.begin(9600);
	tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
	tft.println("done");
	rowindex = 1;
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.print("init obd Serial3     ");
	startOBD();

	// Initialize all the uninitialized TinyGPSCustom objects
	for (int i = 0; i < 4; ++i) {
		satNumber[i].begin(gps, "GPGSV", 4 + 4 * i); // offsets 4, 8, 12, 16
		elevation[i].begin(gps, "GPGSV", 5 + 4 * i); // offsets 5, 9, 13, 17
		azimuth[i].begin(gps, "GPGSV", 6 + 4 * i); // offsets 6, 10, 14, 18
		snr[i].begin(gps, "GPGSV", 7 + 4 * i); // offsets 7, 11, 15, 19
	}

	for (int i = 0; i < MAX_SATELLITES; ++i) {
		sats[i].run = 0;
		sats[i].snr = 0;
		sats[i].active = false;
	}

}

void drawActiveScreen() {
	if (screen_mode == main_scr)
		drawMetricData(gps);
	else if (screen_mode == sat_scr) {
		drawSatData();
	}
}

void switchScreen() {
	if (button_state) {
		button_state = false;
		if (screen_mode == main_scr) {
			drawSatScreen();
			screen_mode = sat_scr;
			gpsangle = -1;
		} else {
			screen_mode = main_scr;
			drawMetricScreen();
		}
	}
}

void logData() {
	if (has_sd) {
		logNewRow();
		logIntData (rpm);
		logIntData (load);
		logIntData (throttle);
		logIntData (frp);
		logIntData (maf);
		logEndRow();
		// Force data to SD and update the directory entry to avoid data loss.
		if (!file.sync() || file.getWriteError())
			error("SD write error");
	}
}

static bool feedgps() {
	while (nss.available()) {
		if (gps.encode(nss.read()))
			return true;
	}
	return false;
}

void drawMetricScreen() {
	tft.fillScreen(ILI9341_BLACK);
	tft.setCursor(0, 145);

	tft.fillRoundRect(0, SPEED_POS, 240, 50, 10, ILI9341_DRED);
	tft.fillRoundRect(0, RPM_POS, 240, 50, 10, ILI9341_DBLUE);
	tft.drawFastHLine(0, 25, 240, ILI9341_RED);
	tft.drawFastHLine(0, 26, 240, ILI9341_RED);
//	feedgps();
}

//------------------------------------------------------------------------------
// Write data header.
void writeHeader() {
	file.print(
			"INDEX,RCR,DATE,TIME,VALID,LATITUDE,N/S,LONGITUDE,E/W,HEIGHT,SPEED,HEADING,DISTANCE,RPM,ENGINELOAD,THROTTLE,FRP,MAF,");
	file.println();
//	feedgps();
}

unsigned long bootText() {
	unsigned long start = micros();
	tft.setTextColor(ILI9341_BLUE, ILI9341_BLACK);
	tft.setTextSize(5);
	tft.setCursor(80, 30);
	tft.println("CBB");
	tft.setTextSize(2);
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.println("   Car Black Box");
	tft.drawFastHLine(0, 110, 240, ILI9341_RED);
	tft.drawFastHLine(0, 111, 240, ILI9341_RED);
	tft.setCursor(0, 120);
	tft.setTextSize(2);
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.println("Initializing");
	tft.println();
	tft.setTextSize(1);
	tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);

	return micros() - start;
}

static void drawMetricData(TinyGPSPlus &gps) {
	float flat, flon;
	unsigned long age, date, time, chars = 0;
	unsigned short sentences = 0, failed = 0;
//	static const float LONDON_LAT = 51.508131, LONDON_LON = -0.128002;
	static const float LONDON_LAT = 52.361440, LONDON_LON = 5.239020;

	flat = gps.location.lat();
	flon = gps.location.lng();
	age = gps.location.age();

	logDateTime(gps);
	logFloatData(flat, 5);
	file.print("N,");
	logFloatData(flon, 5);
	file.print("E,");

	tft.setCursor(0, 0);
	tft.setTextSize(2);
	tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
	print_date(gps);

	//float speed = gps.f_speed_kmph();
	if (gps.speed.isValid()) {
		speed = gps.speed.kmph();
		drawSourceIndicator(200, SPEED_POS + 2, "gps", ILI9341_YELLOW,
				ILI9341_DRED);
	} else {
		if (obd_connected) {
			drawSourceIndicator(200, SPEED_POS + 2, "obd", ILI9341_YELLOW,
					ILI9341_DRED);
			speed = obdspeed;
		} else {
			if (blink)
				drawSourceIndicator(200, SPEED_POS + 2, "NC ", ILI9341_YELLOW,
						ILI9341_DRED);
			else
				tft.fillRect(200, SPEED_POS, 30, 20, ILI9341_DRED);
		}
	}
	tft.setTextColor(ILI9341_WHITE, ILI9341_DRED);
	tft.setCursor(10, SPEED_POS + ALIGN_Y);
	tft.setTextSize(5);
	print_float(speed * 1.0, true, 6, 1);

	tft.setCursor(10, RPM_POS + ALIGN_Y);
	tft.setTextSize(5);
	tft.setTextColor(ILI9341_WHITE, ILI9341_DBLUE);
	print_fixint(rpm, true, 5);
	tft.setTextSize(3);
	tft.setCursor(140, RPM_POS + ALIGN_Y + 10);
	print_fixint(temp, true, 5);

	tft.setCursor(0, INFO_POS);
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.setTextSize(2);
	tft.print("Sats:      ");
	tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
	print_int(gps.satellites.value(), gps.satellites.isValid(), 3);

	tft.setTextSize(2);
	tft.setTextColor(ILI9341_WHITE);
	tft.print("Altitude   ");
	tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
	print_float(gps.altitude.meters(), gps.altitude.isValid(), 8, 2);
	logFloatData(gps.altitude.meters(), 2);

	tft.setTextColor(ILI9341_WHITE);
	tft.print("Angle:     ");
	gpsangle = gps.course.deg();
	tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
	print_float(gpsangle, gps.course.isValid(), 7, 2);
	logFloatData(gpsangle, 1);

	tft.setTextColor(ILI9341_WHITE);
	tft.print("Distance:  ");
	float dist = (unsigned long) TinyGPSPlus::distanceBetween(
			gps.location.lat(), gps.location.lng(), LONDON_LAT, LONDON_LON)
			/ 1000;
	tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
	print_float(dist, gps.location.isValid(), 5, 2);
	logDistData((float) dist, 1);
	tft.println();

	drawSDCardFileMessage();

	//gps.stats(&chars, &sentences, &failed);
	tft.setCursor(ERROR_POS_X, ERROR_POS_Y);
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.print("GPS errors:    ");
	print_int(failed, 0xFFFFFFFF, 6);

	blink = blink ^ 1;

}

void drawSatData() {
	char sz[32];
	int color = 0;
	if (!gps.sentencesWithFix() > 1)
		return;
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	if (totalGPGSVMessages.isUpdated()) {
		for (int i = 0; i < 4; ++i) {
			int no = atoi(satNumber[i].value());
			if (no >= 1 && no <= MAX_SATELLITES) {
				sats[no - 1].elevation = atoi(elevation[i].value());
				sats[no - 1].azimuth = atoi(azimuth[i].value());
				sats[no - 1].snr = atoi(snr[i].value());
				sats[no - 1].active = true;
			}
		}
	}

//	tft.drawCircle(40, 200, 30, ILI9341_GREEN);
//	rotatePoint(gps.course.deg(), &sx, &sy, CENTER_X, CENTER_Y);
//	tft.drawLine(sx, sy, CENTER_X, CENTER_Y, ILI9341_WHITE);
	tft.drawLine(CENTER_X, TOP, CENTER_X, HEIGHT, ILI9341_WHITE);
	tft.drawLine(SIDE, CENTER_Y, WIDTH, CENTER_Y, ILI9341_WHITE);
	tft.drawCircle(CENTER_X, CENTER_Y, MAX_R + 9, ILI9341_GRAY1);
	tft.drawCircle(CENTER_X, CENTER_Y, MAX_R, ILI9341_GREEN);
	tft.drawCircle(CENTER_X, CENTER_Y, MAX_R / 2, ILI9341_GREEN);
	tft.drawCircle(CENTER_X, CENTER_Y, MAX_R * 0.75, ILI9341_GREEN);

	if (gps.course.isValid()) {
//		drawOpenDot(gpsangle, 270, 98, ILI9341_BLACK,ILI9341_BLACK);
//		drawOpenDot(gps.course.deg(), 270, 98, ILI9341_WHITE, ILI9341_RED);
//
//// DEBUG
////		drawOpenDot(gpsangle, 270, 98, ILI9341_BLACK);
////		gpsangle+=10;
////		drawOpenDot(gpsangle, 270, 98, ILI9341_WHITE);
//
		tft.setTextSize(2);
		tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
		tft.setCursor(0, 200);
		tft.print(gps.course.deg());
		tft.print(gps.cardinal(gps.course.deg()));
//// DEBUG
////		tft.print(gpsangle);
////		tft.print(gps.cardinal(gpsangle));
//
//		gpsangle = gps.course.deg();
	}

	tft.setTextSize(2);
	tft.setTextColor(ILI9341_WHITE);
	tft.setCursor(0, 0);
	tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
	print_int(gps.satellites.value(), gps.satellites.isValid(), 3);

	tft.setTextColor(ILI9341_WHITE);
	tft.setCursor(0, 260);
	tft.print("Alt   ");
	tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
	print_float(gps.altitude.meters(), gps.altitude.isValid(), 5, 2);

	tft.setTextColor(ILI9341_WHITE, ILI9341_DRED);
	tft.setCursor(10, 280 + ALIGN_Y);
	tft.setTextSize(4);
	print_float(gps.speed.kmph(), gps.speed.isValid(), 5, 1);

	int totalMessages = atoi(totalGPGSVMessages.value());
	int currentMessage = atoi(messageNumber.value());
	if (totalMessages == currentMessage) {

		tft.setCursor(0, 0);
		tft.setTextSize(1);
		for (int i = 0; i < MAX_SATELLITES; ++i) {
			if (sats[i].active) {
				color = ILI9341_YELLOW;
				sats[i].run = 0;
			} else {
				color = ILI9341_RED;
				sats[i].run++;
			}

			if (sats[i].run > COLD && sats[i].run < FROOZEN)
				color = ILI9341_BLUE;
			else if (sats[i].run == FROOZEN)
				color = ILI9341_BLACK;
			else if (sats[i].run > FROOZEN)
				continue;
//			drawBar(i, 35, color);
			if (sats[i].snr > 0) {
				drawBar(i, color);
				if (gps.satellites.value() > 0)
					drawDot(i, 270, color);
			}
//			drawDot(i,130,12,color);
//			drawDot(i,20,25,color);
//			drawDot(i,270,50,color);
//			tft.setTextColor(color, ILI9341_BLACK);
//			sprintf(sz, "%02d - El=%02d Az=%03d snr=%02d", i + 1,
//					sats[i].elevation, sats[i].azimuth, sats[i].snr);
//			tft.println(sz);
			sats[i].active = false;
		}
	}
}

