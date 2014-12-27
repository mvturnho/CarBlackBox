/*
 * GPS.h
 *
 *  Created on: 23 dec. 2014
 *      Author: m.vanturnhout
 */

#ifndef GPS_H_
#define GPS_H_

#define nss Serial3
#include <TinyGPS++.h>

class GPS {

public:
	TinyGPSPlus gps;


	GPS();
	virtual ~GPS();
};

#endif /* GPS_H_ */
