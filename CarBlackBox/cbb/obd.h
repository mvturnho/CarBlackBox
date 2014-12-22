/*
 * obd.h
 *
 *  Created on: 22 dec. 2014
 *      Author: mvturnho
 */

#ifndef CBB_OBD_H_
#define CBB_OBD_H_

int rpm = 0;
int obdspeed = 10;
float speed = 0;
int load = 50; //percent
int throttle = 50;
int frp = 0;
int maf = 0;
int temp = 0;

COBD obd;

bool obd_connected = false;

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

void updateObd() {
	if (obd_connected == false)
		obd_connected = obd.init();
	if (obd_connected && obd.read(PID_RPM, rpm)) {
		obd.read(PID_ENGINE_LOAD, load);
		obd.read(PID_SPEED, obdspeed);
		obd.read(PID_THROTTLE, throttle);
		obd.read(PID_COOLANT_TEMP, temp);
//		obd.read(PID_FUEL_PRESSURE, frp);
//		obd.read(PID_MAF_FLOW, maf);
	} else {
		obd_connected = false;
		load = 0;
		obdspeed = 0;
		throttle = 0;
		rpm = 0;
	}
}

void readRealTimeObd() {
	obd.read(PID_ENGINE_LOAD, load);
	obd.read(PID_THROTTLE, throttle);
}

#endif /* CBB_OBD_H_ */
