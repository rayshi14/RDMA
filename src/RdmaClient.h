/*
 * RdmaClient.h
 *
 *  Created on: Sep 14, 2011
 *      Author: stu
 */

#ifndef RDMACLIENT_H_
#define RDMACLIENT_H_

#include "IBenchmarkTask.h"
#include <string>
#include <rdma/rdma_cma.h>
#include "AppLauncher.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "StopWatch.h"
#include "ByteBuffer.h"

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <stdlib.h>
#include "Logger.h"
#include <vector>

#define N_CQE 50

using namespace std;

class RdmaClient: public IBenchmarkTask {
public:
	RdmaClient(AppLauncher *applauncher);
	virtual ~RdmaClient();

	void run();
	void _close();

	double getThroughput();
	double getReadOps();
	double getWriteOps();
	double getErrorOps();
	double getLatency();

	static void exitSignalHandler(int sig);
	static bool mbGotExitSignal;

private:
	Logger *logger;
	string clientIP;
	string filename;
	int size;
	StopWatch *stopWatch;
	int controlChannel;
	int loop;
	TestCase testCase;

	short data_port;
	short control_port;
	int timeout;
	int ncqe;
	int comp_vector;
	struct rdma_event_channel *cm_channel;
	struct rdma_cm_id *id;
	struct ibv_pd *pd;
	struct ibv_comp_channel *comp_channel;
	struct ibv_cq *cq;
	struct ibv_qp *qp;
	int access;
	int fragmentsize;

	double throughput;
	double readOps;
	double writeOps;
	double errorOps;
	double latency;
};

#endif /* RDMACLIENT_H_ */
