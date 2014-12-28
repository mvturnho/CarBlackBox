/*
 * logging.h
 *
 *  Created on: 22 dec. 2014
 *      Author: mvturnho
 */

#ifndef CBB_LOGGING_H_
#define CBB_LOGGING_H_

//EEPROM adres to store filenum counter
#define filenum_addr 0x10
//#define FILE_BASE_NAME "DATA"
#define SD_CS      20

extern SdFat sd;
extern SdFile file;
int rowindex;

bool openFile(char *fileName) {
//	const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1;

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
	tft.println(fileName);
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



#endif /* CBB_LOGGING_H_ */
