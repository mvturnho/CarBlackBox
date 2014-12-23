/*
 * System.cpp
 *
 *  Created on: 23 dec. 2014
 *      Author: m.vanturnhout
 */

#include "System.h"

#define BUTTON_PIN 5

System::System() {
#if defined(DEBUG)
	Serial.begin(115200);
#endif
	pinMode(BUTTON_PIN, INPUT);
	digitalWrite(BUTTON_PIN, HIGH);
	attachInterrupt(BUTTON_PIN, button_pressed, LOW);
	// After setting up the button, setup debouncer
	debouncer.attach(BUTTON_PIN);
	debouncer.interval(5);
}

System::~System() {
	// TODO Auto-generated destructor stub
}

