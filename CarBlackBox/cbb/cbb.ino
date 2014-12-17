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
//#include <Bounce2.h>
#include <Wire.h>
#include <OBD.h>
#include <SdFat.h>
#include <EEPROM.h>

#define DEBUG

#define FILE_BASE_NAME "DATA"
// For the Adafruit shield, these are the default.
#define TFT_DC     15
#define TFT_CS     6
#define TFT_MOSI   11
#define TFT_CLK    13
#define TFT_RST    14
#define TFT_MISO   12

#define SD_CS      20

#define BUTTON_PIN 5

#define SPEED_POS 40
#define RPM_POS 100
#define LOAD_POS 160
#define THR_POS  175
#define INFO_POS 220
#define ALIGN_Y 6
#define ERROR_POS_X 120
#define ERROR_POS_Y 310

//Pin Definitions
#define nss Serial3
//SoftwareSerial nss(GPS_RxPin, GPS_TxPin);

#define filenum_addr 0x10

const uint32_t SAMPLE_INTERVAL_MS = 100;

COBD obd;

// Use hardware SPI (on Uno, #13, #12, #11) 
ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST);
// Instantiate a Bounce object
//Bounce debouncer = Bounce(); 

TinyGPSPlus gps;
//HardwareSerial nss(GPS_RxPin, GPS_TxPin);
// File system object.
SdFat sd;
// Log file.
SdFile file;
// Time in micros for next data record.
//uint32_t logTime;
int rowindex;

int rpm = 0;
int obdspeed = 10;
float speed = 0;
int load = 50; //percent
int throttle = 50;
int frp = 0;
int maf = 0;
float gpsangle;
char fileName[14];

typedef enum {
	main_scr, sat_scr
} mode;
mode screen_mode = sat_scr;

bool obd_connected = false;
bool has_sd = false;
bool blink = true;

//For Satelites screen
static const int MAX_SATELLITES = 40;

TinyGPSCustom totalGPGSVMessages(gps, "GPGSV", 1); // $GPGSV sentence, first element
TinyGPSCustom messageNumber(gps, "GPGSV", 2); // $GPGSV sentence, second element
TinyGPSCustom satsInView(gps, "GPGSV", 3);     // $GPGSV sentence, third element
TinyGPSCustom satNumber[4]; // to be initialized later
TinyGPSCustom elevation[4];
TinyGPSCustom azimuth[4];
TinyGPSCustom snr[4];

struct {
	bool active;
	int elevation;
	int azimuth;
	int snr;
} sats[MAX_SATELLITES];

// Error messages stored in flash.
#define error(msg) error_P(PSTR(msg))
//------------------------------------------------------------------------------
void error_P(const char* msg) {
	//sd.errorHalt_P(msg);
	has_sd = false;
	sprintf(fileName, msg);
}

void setup() {

	const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1;

#if defined(DEBUG)
	Serial.begin(115200);
#endif
	pinMode(BUTTON_PIN, INPUT);
	digitalWrite(BUTTON_PIN, HIGH);
	// After setting up the button, setup debouncer
//	debouncer.attach(BUTTON_PIN);
//	debouncer.interval(5);
	//initialize TFT display
	tft.begin();
	tft.fillScreen(ILI9341_BLACK);
	testText();
	delay(200);

	tft.setRotation(0);	// Initialize the SD card at SPI_HALF_SPEED to avoid bus errors with

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
	// Start on a multiple of the sample interval.
//	logTime = micros() / (1000UL * SAMPLE_INTERVAL_MS) + 1;
//	logTime *= 1000UL * SAMPLE_INTERVAL_MS;
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.print("init gps Serial1     ... ");
	nss.begin(9600);
	tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
	tft.println("done");
	rowindex = 1;
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.print("init obd Serial3     ");
	startOBD();

	for (int i = 0; i < 320; i++) {
		int s = 20 * sin((long double) i / 10);
		tft.drawPixel(i, 300 + (int) s, ILI9341_GREEN);
		delay(5);
	}

	// Initialize all the uninitialized TinyGPSCustom objects
	for (int i = 0; i < 4; ++i) {
		satNumber[i].begin(gps, "GPGSV", 4 + 4 * i); // offsets 4, 8, 12, 16
		elevation[i].begin(gps, "GPGSV", 5 + 4 * i); // offsets 5, 9, 13, 17
		azimuth[i].begin(gps, "GPGSV", 6 + 4 * i); // offsets 6, 10, 14, 18
		snr[i].begin(gps, "GPGSV", 7 + 4 * i); // offsets 7, 11, 15, 19
	}

	delay(2000);
	screen_mode = main_scr;
//	screen_mode = sat_scr;

	if (screen_mode == sat_scr) {
		tft.fillScreen(ILI9341_BLACK);
	} else {
		drawMetricScreen();
	}
}

void loop(void) {
	unsigned long start = millis();

	while (((millis() - start) < 997)) {
		feedgps();

		if (obd_connected == true && screen_mode == main_scr) {
			obd.read(PID_ENGINE_LOAD, load);
			feedgps();
			obd.read(PID_THROTTLE, throttle);
			feedgps();
			drawPercentBar(load, LOAD_POS, ILI9341_DGREEN, "load:");
			feedgps();
			drawPercentBar(throttle, THR_POS, ILI9341_YELLOW, "throttle:");
		}
	}

	int button_state = digitalRead(BUTTON_PIN);
	if (button_state == LOW)
		if (screen_mode == main_scr) {
			tft.fillScreen(ILI9341_BLACK);
			screen_mode = sat_scr;
		} else {
			screen_mode = main_scr;
			drawMetricScreen();
		}

	if (obd_connected == false)
		obd_connected = obd.init();

	if (obd_connected && obd.read(PID_RPM, rpm)) {
		obd.read(PID_ENGINE_LOAD, load);
		obd.read(PID_SPEED, obdspeed);
		obd.read(PID_THROTTLE, throttle);
//		obd.read(PID_FUEL_PRESSURE, frp);
//		obd.read(PID_MAF_FLOW, maf);
	} else {
		obd_connected = false;
		load = 0;
		obdspeed = 0;
		throttle = 0;
		rpm = 0;
	}

	if (obd_connected == true && screen_mode == main_scr) {
		drawPercentBar(load, LOAD_POS, ILI9341_DGREEN, "load:");
		drawPercentBar(throttle, THR_POS, ILI9341_YELLOW, "throttle:");
	}

	if (screen_mode == main_scr)
		gpsdump(gps);
	else if (screen_mode == sat_scr) {
		drawSatScreen();
	}
//	gpsangle = 30;
//	int x = 25 + sin(gpsangle) * 50;
//	int y = 25 + cos(gpsangle) * 50;
//	tft.drawLine(190,RPM_POS,x,y,ILI9341_YELLOW);

	//if (newdata)

	if (has_sd) {
		logNewRow();
		logIntData(rpm);
		logIntData(load);
		logIntData(throttle);
		logIntData(frp);
		logIntData(maf);
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

void startOBD() {
	obd.begin();

	// initiate OBD-II connection until success
	int retry = 0;
	while (retry < 3) {
		if (!obd_connected)
			obd_connected = obd.init();
		tft.print(".");
		retry++;
	}
	if (obd_connected) {
		tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
		tft.println(" done");
	} else {
		tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
		tft.println(" failed");
	}

}

bool openFile(char *fileName) {
	//The filenumber comes from eeprom
	byte fnum = EEPROM.read(filenum_addr);
	if (fnum > 254)
		fnum = 0;
	else
		fnum++;
	sprintf(fileName, "%03d-CBB.CSV", fnum);
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.print("init SD card         ... ");

	// breadboards.  use SPI_FULL_SPEED for better performance.
	if (!sd.begin(SD_CS, SPI_HALF_SPEED)) {
		//sd.initErrorHalt();
		tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
		tft.println("failed");
		sprintf(fileName, "No SD card");
		return false;
	}
	tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
	tft.println("done");
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	if (sd.exists(fileName)) {
		tft.print("overwrite file       ... ");
	} else {
		tft.print("open ");
		tft.print(fileName);
		tft.print("     ... ");
	}
	if (!file.open(fileName, O_CREAT | O_WRITE | O_EXCL)) {
		sprintf(fileName, "ERROR SD card");
		tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
		tft.println("failed");
	} else {
		tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
		tft.println("done");
	}
	EEPROM.write(filenum_addr, fnum);
	return true;
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

void drawPercentBar(int value, int y, int color, const char *label) {
	int lx = 2.4 * value;
	tft.fillRect(0, y, lx, 10, color);
	tft.fillRect(lx, y, 240 - lx, 10, ILI9341_BLACK);
	tft.setCursor(2, y + 1);
	tft.setTextSize(1);
	tft.setTextColor(ILI9341_WHITE);
	tft.print(label);
	tft.setCursor(40, y + 1);
	print_fixint(value, -1, 6);
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

unsigned long testText() {
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

static void gpsdump(TinyGPSPlus &gps) {
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
	print_fixint(rpm, true, 6);

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

void logNewRow() {
	file.print(rowindex++);
	file.print(",TDS,");
//	feedgps();
}

void logFloatData(float data, int prec) {
	file.print(data, prec);
	file.print(",");
//	feedgps();
}
void logDistData(float data, int prec) {
	file.print(data, prec);
	file.print("M,");
//	feedgps();
}
void logIntData(int data) {
	file.print(data);
	file.print(",");
//	feedgps();
}

void logDateTime(TinyGPSPlus &gps) {
	TinyGPSDate d = gps.date;
	TinyGPSTime t = gps.time;
	if (!d.isValid()) {
		file.print("*******,");
	} else {
		char sz[32];
		sprintf(sz, "%02d/%02d/%02d,", d.month(), d.day(), d.year());
		file.print(sz);
	}
	if (!t.isValid()) {
		file.print("********,");
	} else {
		char sz[32];
		sprintf(sz, "%02d:%02d:%02d,", t.hour(), t.minute(), t.second());
		file.print(sz);
	}
	file.print("SPS,");
//	feedgps();
}

void logEndRow() {
	file.println();
//	feedgps();
}

static void print_int(unsigned int val, bool valid, int len) {
//	clearLine(0);
	char sz[32];
	if (!valid)
		strcpy(sz, "*******");
	else
		sprintf(sz, "%d", val);
	sz[len] = 0;
	for (int i = strlen(sz); i < len; ++i)
		sz[i] = ' ';
	if (len > 0)
		sz[len - 1] = ' ';
	tft.println(sz);
//	feedgps();
}

static void print_fixint(unsigned long val, bool valid, int len) {
//	clearLine(0);
	char sz[32];
	if (!valid)
		strcpy(sz, "*******");
	else
		sprintf(sz, "%5d", val);
	sz[len] = 0;
	for (int i = strlen(sz); i < len; ++i)
		sz[i] = ' ';
	if (len > 0)
		sz[len - 1] = ' ';
	tft.println(sz);
//	feedgps();
}

static void print_float(float val, bool valid, int len, int prec) {
//	clearLine(0);
	char sz[32];
	if (!valid) {
		strcpy(sz, "............");
		sz[len] = 0;
		if (len > 0)
			sz[len - 1] = ' ';
		for (int i = 7; i < len; ++i)
			sz[i] = ' ';
		tft.print(sz);
	} else {
//		file.print(",");
//		file.print(val, prec);
		tft.print(val, prec);
		int vi = abs((int) val);
		int flen = prec + (val < 0.0 ? 2 : 1);
		flen += vi >= 1000 ? 4 : vi >= 100 ? 3 : vi >= 10 ? 2 : 1;
		for (int i = flen; i < len; ++i)
			tft.print(" ");
	}

	tft.println();
//	feedgps();
}

static void print_date(TinyGPSPlus &gps) {
	TinyGPSDate d = gps.date;
	TinyGPSTime t = gps.time;
	if (!d.isValid()) {
		tft.print("../../....");
	} else {
		char sz[32];
		sprintf(sz, "%02d/%02d/%02d", d.month(), d.day(), d.year());
		tft.print(sz);
	}
	tft.print("  ");
	if (!t.isValid()) {
		tft.print("..:..:..");
	} else {
		char sz[32];
		sprintf(sz, "%02d:%02d:%02d", t.hour(), t.minute(), t.second());
		tft.print(sz);
	}

//	feedgps();
}

static void print_str(const char *str, int len) {
//	clearLine(len * 5);

	int slen = strlen(str);
	for (int i = 0; i < len; ++i)
		tft.println(i < slen ? str[i] : ' ');
//	feedgps();
}

void drawSourceIndicator(int xpos, int ypos, const char *str, int fg_color,
		int bg_color) {
	tft.setCursor(xpos, ypos);
	tft.setTextColor(fg_color, bg_color);
	tft.setTextSize(2);
	tft.print(str);
}

void drawSDCardFileMessage() {
	tft.setTextSize(1);
	if (has_sd)
		tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	else {
		if (blink)
			tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
		else
			tft.setTextColor(ILI9341_BLACK, ILI9341_BLACK);
	}
	tft.setCursor(0, ERROR_POS_Y);
	tft.print(fileName);
}

void drawSatScreen() {
	char sz[32];

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

	int totalMessages = atoi(totalGPGSVMessages.value());
	int currentMessage = atoi(messageNumber.value());
	if (totalMessages == currentMessage) {
		tft.setTextSize(2);
		tft.setCursor(155, 30);
		tft.print(F("Sats="));
		tft.println(gps.satellites.value());
		tft.setCursor(0, 0);
		tft.setTextSize(1);
		for (int i = 0; i < MAX_SATELLITES; ++i) {
			if (sats[i].active)
				tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
			else
				tft.setTextColor(ILI9341_BLUE, ILI9341_BLACK);
			sprintf(sz, "%02d - El=%02d Az=%03d snr=%02d", i + 1,
					sats[i].elevation, sats[i].azimuth, sats[i].snr);
			tft.println(sz);
			sats[i].active = false;
			//}
		}
	}

}
