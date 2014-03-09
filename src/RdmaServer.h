/*
 * RdmaServer.h
 *
 *  Created on: Sep 14, 2011
 *      Author: stu
 */

#ifndef RDMASERVER_H_
#define RDMASERVER_H_

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

class RdmaServer: public IBenchmarkTask {
public:
	RdmaServer(AppLauncher *applauncher);
	virtual ~RdmaServer();

	void run();
	void _close();

	double getThroughput();
	double getReadOps();
	double getWriteOps();
	double getErrorOps();
	double getLatency();

private:
	Logger *logger;
	StopWatch *stopWatch;
	string serverIP;
	short data_port;
	short control_port;
	int backlog;
	int ncqe;
	int comp_vector;
	int access;
	int controlServerChannel;
	int controlChannel;
	string filename;
	int size;
	int fragmentsize;
	int loop;
	double throughput;
	double readOps;
	double writeOps;
	double errorOps;
	TestCase testCase;

	struct rdma_event_channel *cm_channel;
	struct ibv_comp_channel *comp_channel;
	struct ibv_pd *pd;
	struct rdma_cm_id *cm_listen_id;
	struct ibv_cq *cq;
	struct ibv_qp *qp;
};

#endif /* RDMASERVER_H_ */
