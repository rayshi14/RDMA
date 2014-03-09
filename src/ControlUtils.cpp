/*
 * ControlUtils.cpp
 *
 *  Created on: Sep 28, 2011
 *      Author: stu
 */

#include "ControlUtils.h"

ControlUtils::ControlUtils(int socket) {
	
	this->socket = socket;
	this->logger = Logger::getInstance();
	logger->info("control channel for socket ", socket);
}

ControlUtils::~ControlUtils() {
	
}

void ControlUtils::waitForNextRound(ByteBuffer *readyBuffer) {
	logger->warning("wait for next round, size", readyBuffer->getSize());
	readyBuffer->clear();
	logger->warning("about to read from socket", socket);
	readyBuffer->readNet(socket);
}

void ControlUtils::startNextRound(ByteBuffer *readyBuffer) {
	readyBuffer->clear();
	readyBuffer->writeNet(socket);
}

