/*
 * AppLauncher.cpp
 *
 *  Created on: Aug 24, 2011
 *      Author: stu
 */

#include <string>

#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <sstream>

#include "AppLauncher.h"
#include "IBenchmarkTask.h"
#include "RdmaServer.h"
#include "RdmaClient.h"
#include "Logger.h"   

using namespace std;

class RdmaServer;
class RdmaClient;

AppLauncher::AppLauncher() {
	this->ipAddress = "127.0.0.1"; // of
	this->appType = "";
	this->filename = "";
	this->size = 1024;
	this->fragmentsize = size;
	this->watcherEnabled = true;
	this->memoryDirect = true;
	this->loop = 1;
	this->bechmarkType = BENCHMARK_UNDEFINED;
	this->testCase = TEST_WRITE;
	this->enableSG = false;
	this->controlPort = 2020;
	this->dataPort = 1919;
	this->writes = loop;
}

AppLauncher::~AppLauncher() {
}

string AppLauncher::getIPAddress() {
	return this->ipAddress;
}

string AppLauncher::getAppType() {
	return this->appType;
}

string AppLauncher::getFilename() {
	return this->filename;
}

int AppLauncher::getSize() {
	return this->size;
}

int AppLauncher::getFragmentSize() {
	return this->fragmentsize;
}

bool AppLauncher::getWatcherEnable() {
	return this->watcherEnabled;
}

bool AppLauncher::getMemoryDirect() {
	return this->memoryDirect;
}

int AppLauncher::getLoop() {
	return this->loop;
}

enum BenchmarkType AppLauncher::getBenchmarkType() {
	return this->bechmarkType;
}

enum TestCase AppLauncher::getTestCase() {
	return this->testCase;
}

bool AppLauncher::getEnableSG() {
	return this->enableSG;
}

short AppLauncher::getControlPort(){
	return this->controlPort;
}

short AppLauncher::getDataPort(){
	return this->dataPort;
}

int AppLauncher::getWrites(){
	return this->writes;
}


/*
 * Parse command line
 * arguments and 
 * launch program
 */
int AppLauncher::launch(int argc, char **argv) {
	IBenchmarkTask *benchmarkTask = NULL;
	stringstream dataFilename;
	ofstream dataStream;
	int option_index = 0;
	int c;
	enum LogLevel loglevel = LOG_OFF;
	string _loglevel;
	Logger *logger = Logger::getInstance();
	string _testCase;

	while ((c = getopt(argc, argv, "t:a:f:s:k:b:l:o:p:r:w:")) != -1) {
		switch (c) {
		case 't':
			appType.clear();
			appType.assign(optarg);
			break;
		case 'a':
			ipAddress.clear();
			ipAddress.assign(optarg);
			break;
		case 'f':
			filename.clear();
			filename.assign(optarg);
			break;
		case 's':
			size = atoi(optarg);
			fragmentsize = size;
			break;
		case 'k':
			loop = atoi(optarg);
			writes = loop;
			break;
		case 'b':
			fragmentsize = atoi(optarg);
			break;
		case 'l':
			_loglevel.clear();
			_loglevel.assign(optarg);
			break;
		case 'o':
			_testCase.clear();
			_testCase.assign(optarg);
			break;
		case 'p':
			controlPort = (short) atoi(optarg);
			break;
		case 'r':
			dataPort = (short) atoi(optarg);
			break;
		case 'w':
			writes = atoi(optarg);
			break;
		default:
			cout << "wrong arguments" << endl;
			exit(0);
		}
	}

	if (_loglevel.compare("info") == 0) {
		loglevel = LOG_INFO;
	} else if (_loglevel.compare("warning") == 0) {
		loglevel = LOG_WARNING;
	}
	if (_loglevel.compare("severe") == 0) {
		loglevel = LOG_SEVERE;
	}
	Logger::loglevel = loglevel;

	logger->info("apptype", appType.c_str());
	logger->info("ipaddress", ipAddress.c_str());
	logger->info("filename", filename.c_str());
	logger->info("size", size);
	logger->info("loop", loop);
	logger->info("fragmentsize", fragmentsize);
	logger->info("_testCase", _testCase.c_str());

	if (_testCase.compare("write") == 0) {
		this->testCase = TEST_WRITE;
	} else if (_testCase.compare("read") == 0) {
		this->testCase = TEST_READ;
	} else {
		logger->severe("no valid testCase", _testCase.c_str());
	}

	if (appType.compare("cpp-rdma-server") == 0) {
		logger->info("starting rdma server");
		benchmarkTask = new RdmaServer(this);
		bechmarkType = CPP_RDMA_SERVER;
	} else if (appType.compare("cpp-rdma-client") == 0) {
		logger->info("starting rdma client");
		benchmarkTask = new RdmaClient(this);
		bechmarkType = CPP_RDMA_CLIENT;
	}
    else {
		logger->severe("no valid apptype", appType.c_str());
	}

	benchmarkTask->run();
	benchmarkTask->_close();

	dataFilename << "datalog";
	if (appType.find("client") != string::npos) {
		dataFilename << "-client.dat";
	} else {
		dataFilename << "-server.dat";
	}
	dataStream.open(dataFilename.str().c_str(), ofstream::app);
	if (benchmarkTask != NULL) {
		dataStream << appType << "\t\t" << bechmarkType << "\t" << loop << "\t"
				<< size << "\t\t" << benchmarkTask->getReadOps() << "\t\t"
				<< benchmarkTask->getWriteOps() << "\t\t"
				<< benchmarkTask->getThroughput() << "\t\t"
				<< benchmarkTask->getErrorOps() << "\t\t" << fragmentsize << "\t\t"
				<< benchmarkTask->getLatency()
				<< "\n";
	}
	dataStream.close();

	return 0;
}

int main(int argc, char **argv) {
	AppLauncher *applauncher = new AppLauncher();
	applauncher->launch(argc, argv);
}

