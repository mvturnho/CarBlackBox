/*
 * graphics.h
 *
 *  Created on: 22 dec. 2014
 *      Author: mvturnho
 */

#ifndef CBB_GRAPHICS_H_
#define CBB_GRAPHICS_H_

#define SPEED_POS 40
#define RPM_POS 100
#define LOAD_POS 160
#define THR_POS  175
#define INFO_POS 220
#define ALIGN_Y 6
#define ERROR_POS_X 120
#define ERROR_POS_Y 310

#define COLD  	10
#define FROOZEN  20
#define CENTER_X 75
#define CENTER_Y 90
#define MAX_R	65
#define TOP  (CENTER_Y - MAX_R)
#define SIDE (CENTER_X - MAX_R)
#define WIDTH ((2*MAX_R)+SIDE)
#define HEIGHT ((2*MAX_R)+TOP)

#define BARHEIGHT  	7
#define STARTX 		155
#define MAXBAR		240
#define MAXSIGNAL   50.0
#define BARLENGTH   (MAXBAR - STARTX)
#define FACTOR 		(BARLENGTH / MAXSIGNAL)

//For Satelites screen
#define MAX_SATELLITES 40

extern ILI9341_t3 tft;
typedef struct satelite {
	bool active;
	int elevation;
	int azimuth;
	int snr;
	int run;
	int old_x;
	int old_y;
} Sat;

satelite sats[MAX_SATELLITES];

extern TinyGPSPlus gps;
extern float gpsangle;
extern bool has_sd;
extern bool blink;
extern char fileName[];

extern int rpm;
extern int obdspeed;
extern int load;
extern int throttle;
extern int frp;
extern int maf;
extern int temp;

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
		sprintf(sz, "%4d", val);
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

void drawDot(int i, int offset, int color) {
	//first delete the old
	tft.fillCircle(sats[i].old_x, sats[i].old_y, 3, ILI9341_BLACK);
	tft.setTextColor(ILI9341_BLACK);
	tft.setTextSize(1);
	tft.setCursor(sats[i].old_x, sats[i].old_y + 4);
	tft.print(i + 1);
	//draw the new
	int fact = ((float) sats[i].elevation / 90) * MAX_R;
	int y = fact * sin(radians(sats[i].azimuth + offset));
	int x = fact * cos(radians(sats[i].azimuth + offset));
	tft.fillCircle(CENTER_X + x, CENTER_Y + y, 2, color);
	tft.setTextColor(color);
	tft.setTextSize(1);
	tft.setCursor(CENTER_X + x, CENTER_Y + y + 4);
	tft.print(i + 1);
	sats[i].old_x = CENTER_X + x;
	sats[i].old_y = CENTER_Y + y;
}

void drawOpenDot(float angle, int offset, float elevation, int color,
		int c_color = -1) {
	int fact = (elevation / 90) * MAX_R;
	int y = fact * sin(radians(angle + offset));
	int x = fact * cos(radians(angle + offset));
	tft.drawCircle(CENTER_X + x, CENTER_Y + y, 3, color);
	if (c_color > -1)
		tft.fillCircle(CENTER_X + x, CENTER_Y + y, 2, c_color);
	else
		tft.drawCircle(CENTER_X + x, CENTER_Y + y, 2, color);

}

void drawBar(int i, int color) {
	if (sats[i].snr > 0) {
		int y = i * 8;
		int w = sats[i].snr * FACTOR;
		if (color == ILI9341_BLACK) {
			tft.fillRect(STARTX, y, MAXBAR, BARHEIGHT, ILI9341_BLACK);
		} else {
			tft.fillRect(STARTX, y, w, BARHEIGHT, color);
			tft.fillRect(STARTX + w, y, BARLENGTH - w, BARHEIGHT,
					ILI9341_BLACK);

			tft.setTextColor(ILI9341_WHITE);
			tft.setCursor(MAXBAR - 14, y);
			tft.setTextSize(1);
			tft.print(i + 1);
		}
	}
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

void drawRealTimeData() {
	drawPercentBar(load, LOAD_POS, ILI9341_DGREEN, "load:");
	drawPercentBar(throttle, THR_POS, ILI9341_YELLOW, "throttle:");
}

void rotatePoint(float a, int *x, int *y, int xm, int ym) {
	// Subtract midpoints, so that midpoint is translated to origin
	// and add it in the end again
	int xr = (*x - xm) * cos(a) - (*y - ym) * sin(a) + xm;
	int yr = (*x - xm) * sin(a) + (*y - ym) * cos(a) + ym;
	//result values
	*x = xr;
	*y = yr;
}

void drawSatScreen() {
	tft.fillScreen(ILI9341_BLACK);
	tft.setCursor(0, 260);
	tft.fillRoundRect(0, 280, STARTX - 10, 40, 10, ILI9341_DRED);
}

#endif /* CBB_GRAPHICS_H_ */
