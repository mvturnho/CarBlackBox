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

#include <stdio.h>
#include <string.h>

#include <TinyGPS++.h>
#include <SPI.h>
#include <ILI9341_t3.h>
#include <Bounce2.h>
#include <Wire.h>
#include <OBD.h>
#include <SdFat.h>
#include <EEPROM.h>
#include <math.h>
#include <Time.h>  

#include "graphics.h"
#include "logging.h"
#include "obd.h"

//#define DEBUG

// For the Adafruit shield, these are the default.
#define TFT_DC     15
#define TFT_CS     6
#define TFT_MOSI   11
#define TFT_CLK    13
#define TFT_RST    14
#define TFT_MISO   12

#define BUTTON_PIN 5

#define nss Serial3
#define bts Serial2

// Use hardware SPI (on Uno, #13, #12, #11) 
ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST);
// Instantiate a Bounce object
Bounce debouncer = Bounce();

TinyGPSPlus gps;
// File system object.
SdFat sd;
// Log file.
SdFile file;

float gpsangle = -1.0;
char fileName[14];

enum {
	mainscreen, satscreen, nextscreen, get,
} commands;
char buf[20];
int vars[4];
int varcount = 0;
int command = -1;
int bc = 0;

typedef enum {
	main_scr, sat_scr
} mode;
mode screen_mode = sat_scr;

bool has_sd = false;
bool has_bt = false;
bool blink = true;
volatile bool button_state = false;

TinyGPSCustom totalGPGSVMessages(gps, "GPGSV", 1); // $GPGSV sentence, first element
TinyGPSCustom messageNumber(gps, "GPGSV", 2); // $GPGSV sentence, second element
TinyGPSCustom satsInView(gps, "GPGSV", 3);     // $GPGSV sentence, third element
TinyGPSCustom satNumber[4]; // to be initialized later
TinyGPSCustom elevation[4];
TinyGPSCustom azimuth[4];
TinyGPSCustom snr[4];

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
	// After setting up the button, setup debouncer
	debouncer.attach(BUTTON_PIN);
	debouncer.interval(5);

	//initialize TFT display
	tft.begin();
	tft.fillScreen(ILI9341_BLACK);
	bootText();
	delay(200);

	if (!digitalRead(BUTTON_PIN)) {
		resetSystem();
		delay(200);
	}

	tft.setRotation(0);	// Initialize the SD card at SPI_HALF_SPEED to avoid bus errors with

	initSystem();

#if defined(DEBUG)
	delay(2000);
#endif

	delay(2000);
//	screen_mode = main_scr;
	screen_mode = sat_scr;

	if (screen_mode == sat_scr) {
		drawSatScreen();
	} else {
		drawMetricScreen();
	}

	attachInterrupt(BUTTON_PIN, button_pressed, LOW);
}

void loop(void) {
	unsigned long start = millis();

	drawActiveScreen();

	while (((millis() - start) < 997)) {
		feedgps();
		process_serial();
		if (obd_connected && screen_mode == main_scr) {
			readRealTimeObd();
			feedgps();
			drawRealTimeData();
		}
		//else if (screen_mode == sat_scr && gps.course.isValid() && gpsangle != gps.course.deg()) {
//			drawOpenDot(gpsangle, 270, 98, ILI9341_BLACK);
//			gpsangle += 1;
//			drawOpenDot(gpsangle, 270, 98, ILI9341_WHITE);
//			drawOpenDot(gps.course.deg(), 270, 98, ILI9341_WHITE);
//			gpsangle = gps.course.deg();
		//}
		feedgps();
		switchScreen();
		int touchReading = touchRead(23);
		if (touchReading > 3000 )
			button_state = true;
	}
	readRealTimeObd();
	feedgps();
	updateObd();
	feedgps();
	logData();
	feedgps();
}

//------------------------------------------------------------------------------
// call back for file timestamps
void dateTime(uint16_t* date, uint16_t* time) {
	// return date using FAT_DATE macro to format fields
	*date = FAT_DATE(year(), month(), day());
	// return time using FAT_TIME macro to format fields
	*time = FAT_TIME(hour(), minute(), second());
}

time_t getTeensy3Time() {
	return Teensy3Clock.get();
}

void initSystem() {
	// set the Time library to use Teensy 3.0's RTC to keep time
	setSyncProvider(getTeensy3Time);

	SdFile::dateTimeCallback(dateTime);

	has_sd = openFile(fileName);

#if defined(DEBUG)
	Serial.print(F("Logging to: "));
	Serial.println(fileName);
	Serial.println(F("Type any character to stop"));
#endif
	// Write data header.
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.print("init new log file    ... ");
	if (has_sd) {
		writeHeader();
		tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
		tft.println("done");
	} else {
		tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
		tft.println("failed");
	}

	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.print("init gps Serial1     ... ");
	nss.begin(9600);
	tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
	tft.println("done");

	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.print("set BT SPEED         ... ");
	bts.begin(9600);
	delay(100);
	bts.print("AT+BAUD8");
	bts.readBytesUntil('q', buf, 8);
	bts.end();
	delay(100);
	bts.begin(115200);
	delay(100);
	bts.print("AT+VERSION");
	bts.readBytesUntil('q', buf, 20);
	tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
	tft.println(buf);

	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.print("set BT PIN           ... ");
	bts.print("AT+PIN1966");
	bts.readBytesUntil('q', buf, 20);
	tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
	if (buf == "OKsetPIN")
		tft.println(buf);
	else
		tft.println(buf);
	has_bt = true;

	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.print("set BT NAME          ... ");
	bts.print("AT+NAMECBB");
	bts.readBytesUntil('q', buf, 20);
	tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
	tft.println(buf);
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);

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

void setScreen(int screen) {
	if (screen_mode != screen) {
		if (screen == sat_scr) {
			drawSatScreen();
			screen_mode = sat_scr;
			gpsangle = -1;
		} else if (screen == main_scr) {
			screen_mode = main_scr;
			drawMetricScreen();
		}
	}
}

void logData() {
	float flat, flon;
	static const float LONDON_LAT = 52.361440, LONDON_LON = 5.239020;
	if (has_sd) {
		logNewRow();
		flat = gps.location.lat();
		flon = gps.location.lng();

		logDateTime(gps);
		logFloatData(flat, 5);
		file.print("N,");
		logFloatData(flon, 5);
		file.print("E,");

		logFloatData(gps.altitude.meters(), 2);
		logFloatData(gps.course.deg(), 1);
		float dist = (unsigned long) TinyGPSPlus::distanceBetween(
				gps.location.lat(), gps.location.lng(), LONDON_LAT, LONDON_LON)
				/ 1000;
		logDistData(dist, 1);

		logIntData (rpm);
		logIntData (load);
		logIntData (throttle);
		logIntData (frp);
		logIntData (maf);
		float ratio = rpm / obdspeed;
		logFloatData(ratio, 2);
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
}

//------------------------------------------------------------------------------
// Write data header.
void writeHeader() {
	file.print(
			"INDEX,RCR,DATE,TIME,VALID,LATITUDE,N/S,LONGITUDE,E/W,HEIGHT,SPEED,HEADING,DISTANCE,RPM,ENGINELOAD,THROTTLE,FRP,MAF,RATIO,");
	file.println();
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

//	logDateTime(gps);
//	logFloatData(flat, 5);
//	file.print("N,");
//	logFloatData(flon, 5);
//	file.print("E,");

	tft.setCursor(0, 0);
	tft.setTextSize(2);
	tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
	print_date(gps);

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
//	logFloatData(gps.altitude.meters(), 2);

	tft.setTextColor(ILI9341_WHITE);
	tft.print("Angle:     ");
	gpsangle = gps.course.deg();
	tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
	print_float(gpsangle, gps.course.isValid(), 7, 2);
//	logFloatData(gpsangle, 1);

	tft.setTextColor(ILI9341_WHITE);
	tft.print("Distance:  ");
	float dist = (unsigned long) TinyGPSPlus::distanceBetween(
			gps.location.lat(), gps.location.lng(), LONDON_LAT, LONDON_LON)
			/ 1000;
	tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
	print_float(dist, gps.location.isValid(), 5, 2);
//	logDistData((float) dist, 1);
	tft.println();

	drawSDCardFileMessage();

	//gps.stats(&chars, &sentences, &failed);
//	tft.setCursor(ERROR_POS_X, ERROR_POS_Y);
//	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
//	tft.print("GPS errors:    ");
//	print_int(failed, 0xFFFFFFFF, 6);

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

	tft.drawLine(CENTER_X, TOP, CENTER_X, HEIGHT, ILI9341_WHITE);
	tft.drawLine(SIDE, CENTER_Y, WIDTH, CENTER_Y, ILI9341_WHITE);
	tft.drawCircle(CENTER_X, CENTER_Y, MAX_R + 9, ILI9341_GRAY1);
	tft.drawCircle(CENTER_X, CENTER_Y, MAX_R, ILI9341_GREEN);
	tft.drawCircle(CENTER_X, CENTER_Y, MAX_R / 2, ILI9341_GREEN);
	tft.drawCircle(CENTER_X, CENTER_Y, MAX_R * 0.75, ILI9341_GREEN);

	if (gps.course.isValid()) {
		drawOpenDot(gpsangle, 270, 98, ILI9341_BLACK, ILI9341_BLACK);
		drawOpenDot(gps.course.deg(), 270, 98, ILI9341_WHITE, ILI9341_RED);

//// DEBUG
////		drawOpenDot(gpsangle, 270, 98, ILI9341_BLACK);
////		gpsangle+=10;
////		drawOpenDot(gpsangle, 270, 98, ILI9341_WHITE);

		tft.setTextSize(4);
		tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
		tft.setCursor(20, 180);
//		tft.print(gps.course.deg()); tft.print(" - ");
		tft.print(gps.cardinal(gps.course.deg()));
//// DEBUG
////		tft.print(gpsangle);
////		tft.print(gps.cardinal(gpsangle));

		gpsangle = gps.course.deg();
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
			if (sats[i].snr > 0) {
				drawBar(i, color);
				if (gps.satellites.value() > 0)
					drawDot(i, 270, color);
			}
			sats[i].active = false;
		}
	}
}

void process_serial() {
	int incomingByte;
	if (bts.available() > 0) {
		incomingByte = bts.read();
		if (incomingByte == '\n' || incomingByte == '\r') {
#if defined(DEBUG)
			Serial.println("found newline");
#endif
			execute();
			clearbuf();
			varcount = 0;
		} else if (incomingByte == ' ') {
			getcommand();
			clearbuf();
		} else if (incomingByte == ',') {
			storevar();
			varcount++;
			clearbuf();
		} else if (incomingByte == '.') {
			execute();
			clearbuf();
			varcount = 0;
		} else {
			buf[bc] = incomingByte;
			bc++;
		}
	}
}

//store the values in variable array
void storevar() {
	//  debug("var: ",buf);
#if defined(DEBUG)
	Serial.print("var : ");
	Serial.println(buf);
#endif
	int val = atoi(buf);
	vars[varcount] = val;
}

//store command code 
void getcommand() {
#if defined(DEBUG)
	Serial.print("command : ");
	Serial.println(buf);
#endif
	if (strcmp(buf, "m") == 0) {
		command = mainscreen;
	} else if (strcmp(buf, "s") == 0) {
		command = satscreen;
	} else if (strcmp(buf, "t") == 0) {
		command = nextscreen;
	} else if (strcmp(buf, "01") == 0) {
		command = get;
	} else {
#if defined(DEBUG)
		Serial.print("invalid command: ");
		Serial.println(buf);
#endif
		command = -1;
		bts.println("?");
	}
}

//execute the command by setting motor values
void execute() {
	long val = atol(buf);

	switch (command) {
	case mainscreen:
		setScreen(main_scr);
		bts.println("switch MAIN screen");
		break;
	case satscreen:
		setScreen(sat_scr);
		bts.println("switch SAT screen");
		break;
	case nextscreen:
		button_state = true;
		bts.println("switch NEXT screen");
		break;
	case get: //get obd values
		if (val == 5) //05 = temp
			bts.println(obdspeed);
		else if (val == 12) //0c = rpm
			bts.println(rpm);
		else if (val == 13) //0d = speed
			bts.println(rpm);
		else if (val == 4) //04 = load
			bts.println(rpm);
		break;
	}

	if (buf[0] == '0' && buf[1] == '1') {
		int val = 0;
		//obd.read(buf, val);
		bts.println("m" + val);
	}

}

void clearbuf() {
	for (int i = 0; i < 11; i++) {
		buf[i] = 0;
	}
	bc = 0;
}

void resetSystem() {
	tft.setTextSize(2);
	tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
	tft.println("SYSTEM RESET!");
	tft.setTextSize(1);
	tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);

	tft.println("reset file counter to 000");
	tft.println();
	
	EEPROM.write(filenum_addr, 0);
}
