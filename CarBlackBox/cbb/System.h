/*
 * System.h
 *
 *  Created on: 23 dec. 2014
 *      Author: m.vanturnhout
 */

#ifndef SYSTEM_H_
#define SYSTEM_H_

#include <Bounce2.h>
#include "Display.h"
#include "GPS.h"
//#include "OBD.h"
#include "Log.h"

class System {
public:
	volatile bool button_state = false;
	Display display;
	Bounce debouncer = Bounce();

	System();
	virtual ~System();
	void button_pressed();
};

#endif /* SYSTEM_H_ */
