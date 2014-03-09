/*
 * IBenchmarkTask.h
 *
 *  Created on: Sep 14, 2011
 *      Author: stu
 */

#ifndef IBENCHMARKTASK_H_
#define IBENCHMARKTASK_H_

class IBenchmarkTask {
public:
	IBenchmarkTask();
	virtual ~IBenchmarkTask();

	virtual void run() = 0;
	virtual void _close() = 0;

	virtual double getThroughput() = 0;
	virtual double getReadOps() = 0;
	virtual double getWriteOps() = 0;
	virtual double getErrorOps() = 0;
	virtual double getLatency() = 0;
};

#endif /* IBENCHMARKTASK_H_ */
