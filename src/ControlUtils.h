/*
 * ControlUtils.h
 *
 *  Created on: Sep 28, 2011
 *      Author: stu
 */

#ifndef CONTROLUTILS_H_
#define CONTROLUTILS_H_

#include "ByteBuffer.h"
#include "Logger.h"

class ControlUtils {
public:
	ControlUtils(int socket);
	virtual ~ControlUtils();

	void waitForNextRound(ByteBuffer *readyBuffer);
	void startNextRound(ByteBuffer *readyBuffer);

private:
	int socket;
	Logger *logger;
};

#endif /* CONTROLUTILS_H_ */
