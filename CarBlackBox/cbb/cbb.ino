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
#include <TinyGPS.h>
#include <SPI.h>
#include <ILI9341_t3.h>
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

#define SPEED_POS 40
#define RPM_POS 100
#define LOAD_POS 160
#define THR_POS  175
#define INFO_POS 220
#define ALIGN_X 6
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

TinyGPS gps;
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
int load = 50; //percent
int throttle = 50;
int frp = 0;
int maf = 0;
float gpsangle;
char fileName[14];

bool obd_connected = false;
bool has_sd = false;
bool blink = true;

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

	//initialize TFT display
	tft.begin();
	testText();
	delay(200);
	//tft.fillScreen(ILI9341_BLACK);
	tft.setRotation(0);	// Initialize the SD card at SPI_HALF_SPEED to avoid bus errors with

	has_sd = openFile(fileName);

#if defined(DEBUG)
	Serial.print(F("Logging to: "));
	Serial.println(fileName);
	Serial.println(F("Type any character to stop"));
#endif
	// Write data header.
	tft.print("init new log file    ... ");
	writeHeader();
	tft.println("done");
	// Start on a multiple of the sample interval.
//	logTime = micros() / (1000UL * SAMPLE_INTERVAL_MS) + 1;
//	logTime *= 1000UL * SAMPLE_INTERVAL_MS;
	tft.print("init gps Serial1     ... ");
	nss.begin(9600);
	tft.println("done");
	rowindex = 1;
	tft.print("init obd Serial3     ... ");
	obd.begin();

	// initiate OBD-II connection until success
	while (!obd.init())
		tft.print(".");
	tft.println("done");
	delay(2000);
	drawMetricScreen();
//	gpsdump(gps);
}

void loop(void) {
	// Time for next record.
	//logTime += 1000UL * SAMPLE_INTERVAL_MS;

	bool newdata = false;
	unsigned long start = millis();

	while (millis() - start < 1000) {
		if (feedgps())
			newdata = true;
		if (obd_connected == true) {
			obd.read(PID_ENGINE_LOAD, load);
//			obd.read(PID_THROTTLE, throttle);
			drawPercentBar(load, LOAD_POS, ILI9341_DGREEN, "load:");
//			drawPercentBar(throttle, THR_POS, ILI9341_YELLOW, "throttle:");
		}
	}

	if (obd.read(PID_RPM, rpm)) {
		obd.read(PID_ENGINE_LOAD, load);
		obd.read(PID_SPEED, obdspeed);
		obd.read(PID_THROTTLE, throttle);
//		obd.read(PID_FUEL_PRESSURE, frp);
//		obd.read(PID_MAF_FLOW, maf);
		obd_connected = true;
	} else {
		obd_connected = false;
		load = 0;
		obdspeed = 0;
		throttle = 0;
		rpm = 0;
	}

	tft.setCursor(10, RPM_POS + ALIGN_X);
	tft.setTextSize(5);
	tft.setTextColor(ILI9341_WHITE, ILI9341_DBLUE);
	print_fixint(rpm, -1, 6);

//	if (load > 100)
//		load = 100;
	drawPercentBar(load, LOAD_POS, ILI9341_DGREEN, "load:");

//	if (throttle > 100)
//		throttle = 100;
	drawPercentBar(throttle, THR_POS, ILI9341_YELLOW, "throttle:");

//	gpsangle = 30;
//	int x = 25 + sin(gpsangle) * 50;
//	int y = 25 + cos(gpsangle) * 50;
//	tft.drawLine(190,RPM_POS,x,y,ILI9341_YELLOW);

	//if (newdata)
	gpsdump(gps);
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

bool openFile(char *fileName) {
	//The filenumber comes from eeprom
	byte fnum = EEPROM.read(filenum_addr);
	if (fnum > 254)
		fnum = 0;
	else
		fnum++;
	sprintf(fileName, "%03d-CBB.CSV", fnum);
	tft.print("init SD card         ... ");

	// breadboards.  use SPI_FULL_SPEED for better performance.
	if (!sd.begin(SD_CS, SPI_HALF_SPEED)) {
		//sd.initErrorHalt();
		tft.println("failed");
		sprintf(fileName, "No SD card");
		return false;
	}
	tft.println("done");
	if (sd.exists(fileName)) {
		tft.print("overwrite file       ... ");
	} else {
		tft.print("open ");
		tft.print(fileName);
		tft.print("     ... ");
	}
	if (!file.open(fileName, O_CREAT | O_WRITE | O_EXCL)) {
		sprintf(fileName, "ERROR SD card");
	}
	tft.println("done");
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
}

void drawPercentBar(int value, int x, int color, const char *label) {
	int lx = 2.4 * value;
	tft.fillRect(0, x, lx, 10, color);
	tft.fillRect(lx, x, 240 - lx, 10, ILI9341_BLACK);
	tft.setCursor(2, x + 1);
	tft.setTextSize(1);
	tft.setTextColor(ILI9341_WHITE);
	tft.print(label);
	tft.setCursor(40, x + 1);
	print_fixint(value, -1, 6);
}

//------------------------------------------------------------------------------
// Write data header.
void writeHeader() {
	file.print(
			"INDEX,RCR,DATE,TIME,VALID,LATITUDE,N/S,LONGITUDE,E/W,HEIGHT,SPEED,HEADING,DISTANCE,RPM,ENGINELOAD,THROTTLE,FRP,MAF,");
	file.println();
}

unsigned long testText() {
	tft.fillScreen(ILI9341_BLACK);
	unsigned long start = micros();
	tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
	tft.setCursor(0, 0);
	tft.setTextSize(5);
	tft.println("GPS LOG");
	tft.setTextSize(2);
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.println("Initializing");
	tft.println();
	tft.setTextSize(1);
	tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);

	return micros() - start;
}

static void gpsdump(TinyGPS &gps) {
	float flat, flon;
	unsigned long age, date, time, chars = 0;
	unsigned short sentences = 0, failed = 0;
//	static const float LONDON_LAT = 51.508131, LONDON_LON = -0.128002;
	static const float LONDON_LAT = 52.361440, LONDON_LON = 5.239020;

	gps.f_get_position(&flat, &flon, &age);

	logDateTime(gps);
	logFloatData(flat, 5);
	file.print("N,");
	logFloatData(flon, 5);
	file.print("E,");

	tft.setCursor(0, 0);
	tft.setTextSize(2);
	tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
	print_date(gps);

	tft.setTextSize(5);
	//float speed = gps.f_speed_kmph();
	tft.setCursor(10, SPEED_POS + ALIGN_X);
	tft.setTextColor(ILI9341_WHITE, ILI9341_DRED);
	if (gps.f_speed_kmph() != TinyGPS::GPS_INVALID_F_SPEED) {
		print_float(gps.f_speed_kmph(), TinyGPS::GPS_INVALID_F_SPEED, 6, 2);
		drawSourceIndicator(200, SPEED_POS + 2, "gps", ILI9341_YELLOW,
				ILI9341_DRED);
	} else {
		print_int(obdspeed, 255, 6);
		drawSourceIndicator(200, SPEED_POS + 2, "obd", ILI9341_YELLOW,
				ILI9341_DRED);
	}

	tft.setCursor(0, INFO_POS);
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.setTextSize(2);
	tft.print("Sats:      ");
	tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
	print_int(gps.satellites(), TinyGPS::GPS_INVALID_SATELLITES, 2);

	tft.setTextSize(2);
	tft.setTextColor(ILI9341_WHITE);
	tft.print("Altitude   ");
	tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
	print_float(gps.f_altitude(), TinyGPS::GPS_INVALID_F_ALTITUDE, 8, 2);
	logFloatData(gps.f_altitude(), 2);

	tft.setTextColor(ILI9341_WHITE);
	tft.print("Angle:     ");
	gpsangle = gps.f_course();
	tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
	print_float(gpsangle, TinyGPS::GPS_INVALID_F_ANGLE, 7, 2);
	logFloatData(gpsangle, 1);

	tft.setTextColor(ILI9341_WHITE);
	tft.print("Distance:  ");
	float dist = TinyGPS::distance_between(flat, flon, LONDON_LAT, LONDON_LON)
			/ 1000;
	tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
	print_float(dist, TinyGPS::GPS_INVALID_F_ANGLE, 3, 2);
	logDistData((float) dist, 1);
	tft.println();
	gps.stats(&chars, &sentences, &failed);
	tft.setTextSize(1);
	if (has_sd)
		tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	else {
		if (blink)
			tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
		else
			tft.setTextColor(ILI9341_GRAY, ILI9341_BLACK);
	}
	tft.setCursor(0, ERROR_POS_Y);
	tft.print(fileName);

	tft.setCursor(ERROR_POS_X, ERROR_POS_Y);
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.print("GPS errors:    ");
	print_int(failed, 0xFFFFFFFF, 6);
	
	blink = blink ^ 1;

}

void logNewRow() {
	file.print(rowindex++);
	file.print(",TDS,");
	feedgps();
}

void logFloatData(float data, int prec) {
	file.print(data, prec);
	file.print(",");
	feedgps();
}
void logDistData(float data, int prec) {
	file.print(data, prec);
	file.print("M,");
	feedgps();
}
void logIntData(int data) {
	file.print(data);
	file.print(",");
}

void logDateTime(TinyGPS &gps) {
	int year;
	byte month, day, hour, minute, second, hundredths;
	unsigned long age;
	gps.crack_datetime(&year, &month, &day, &hour, &minute, &second,
			&hundredths, &age);
	if (age == TinyGPS::GPS_INVALID_AGE) {
		day = month = year = hour = minute = second = 0;
	}
	char sz[32];
	sprintf(sz, "%04d/%02d/%02d,%02d:%02d:%02d,", year, month, day, hour,
			minute, second);
	file.print(sz);
	file.print("SPS,");
	feedgps();
}

void logEndRow() {
	file.println();
	feedgps();
}

static bool feedgps() {
	while (nss.available()) {
		if (gps.encode(nss.read()))
			return true;
	}
	return false;
}

static void print_int(unsigned int val, unsigned int invalid, int len) {
//	clearLine(0);
	char sz[32];
	if (val == invalid)
		strcpy(sz, "*******");
	else
		sprintf(sz, "%d", val);
	sz[len] = 0;
	for (int i = strlen(sz); i < len; ++i)
		sz[i] = ' ';
	if (len > 0)
		sz[len - 1] = ' ';
	tft.println(sz);
	feedgps();
}

static void print_fixint(unsigned long val, unsigned long invalid, int len) {
//	clearLine(0);
	char sz[32];
	if (val == invalid)
		strcpy(sz, "*******");
	else
		sprintf(sz, "%5d", val);
	sz[len] = 0;
	for (int i = strlen(sz); i < len; ++i)
		sz[i] = ' ';
	if (len > 0)
		sz[len - 1] = ' ';
	tft.println(sz);
	feedgps();
}

static void print_float(float val, float invalid, int len, int prec) {
//	clearLine(0);
	char sz[32];
	if (val == invalid) {
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
	feedgps();
}

static void print_date(TinyGPS &gps) {
//	clearLine(0);
	int year;
	byte month, day, hour, minute, second, hundredths;
	unsigned long age;
	gps.crack_datetime(&year, &month, &day, &hour, &minute, &second,
			&hundredths, &age);
	if (age == TinyGPS::GPS_INVALID_AGE) {
		tft.println("*******      *******");
//		file.print(",");
//		file.print("*******    *******");
	} else {
		char sz[32];
		sprintf(sz, "%02d/%02d/%02d  %02d:%02d:%02d ", month, day, year, hour,
				minute, second);
		tft.print(sz);
	}
	feedgps();
}

static void print_str(const char *str, int len) {
//	clearLine(len * 5);

	int slen = strlen(str);
	for (int i = 0; i < len; ++i)
		tft.println(i < slen ? str[i] : ' ');
	feedgps();
}

void drawSourceIndicator(int xpos, int ypos, const char *str, int fg_color,
		int bg_color) {
	tft.setCursor(xpos, ypos);
	tft.setTextColor(fg_color, bg_color);
	tft.setTextSize(2);
	tft.print(str);

}
