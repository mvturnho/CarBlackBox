/*
 * GPS.h
 *
 *  Created on: 23 dec. 2014
 *      Author: m.vanturnhout
 */

#ifndef GPS_H_
#define GPS_H_

#define nss Serial3

class GPS {
public:
	TinyGPSCustom totalGPGSVMessages(gps, "GPGSV", 1); // $GPGSV sentence, first element
	TinyGPSCustom messageNumber(gps, "GPGSV", 2); // $GPGSV sentence, second element
	TinyGPSCustom satsInView(gps, "GPGSV", 3);     // $GPGSV sentence, third element
	TinyGPSCustom satNumber[4]; // to be initialized later
	TinyGPSCustom elevation[4];
	TinyGPSCustom azimuth[4];
	TinyGPSCustom snr[4];

	TinyGPSPlus gps;

	GPS();
	virtual ~GPS();
};

#endif /* GPS_H_ */
