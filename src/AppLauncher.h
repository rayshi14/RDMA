/*
 * AppLauncher.h
 *
 *  Created on: Sep 14, 2011
 *      Author: stu
 */

#ifndef APPLAUNCHER_H_
#define APPLAUNCHER_H_

#include <string>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <sstream>

using namespace std;

enum BenchmarkType {
	BENCHMARK_UNDEFINED,
	NIO_TCP_SERVER,
	NIO_TCP_CLIENT,
	NIO2_TCP_SERVER,
	NIO2_TCP_CLIENT,
	NIO2_RDMA_SERVER,
	NIO2_RDMA_CLIENT,
	RDMA_SERVER,
	RDMA_CLIENT,
	CPP_RDMA_SERVER,
	CPP_RDMA_CLIENT,
	MC_SERVER,
	MC_CLIENT
};

enum TestCase {
	TEST_UNDEFINED, TEST_WRITE, TEST_READ
};

class AppLauncher {
public:
	AppLauncher();
	virtual ~AppLauncher();

	int launch(int argc, char **argv);

	string getIPAddress();
	string getAppType();
	string getFilename();
	int getSize();
	int getFragmentSize();
	bool getWatcherEnable();
	bool getMemoryDirect();
	int getLoop();
	enum BenchmarkType getBenchmarkType();
	enum TestCase getTestCase();
	bool getEnableSG();
	short getControlPort();
	short getDataPort();
	int getWrites();

private:
	string ipAddress;
	string appType;
	string filename;
	int size;
	int fragmentsize;
	bool watcherEnabled;
	bool memoryDirect;
	int loop;
	enum BenchmarkType bechmarkType;
	enum TestCase testCase;
	bool enableSG;
	short controlPort;
	short dataPort;
	int writes;
};

#endif /* APPLAUNCHER_H_ */
