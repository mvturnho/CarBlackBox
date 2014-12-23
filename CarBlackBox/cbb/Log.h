/*
 * Log.h
 *
 *  Created on: 23 dec. 2014
 *      Author: m.vanturnhout
 */

#ifndef LOG_H_
#define LOG_H_

class Log {
public:
	char fileName[14];
	bool has_sd = false;
	Log();
	virtual ~Log();
};

#endif /* LOG_H_ */
