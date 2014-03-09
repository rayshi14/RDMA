/*
 * ReadyToReceive.cpp
 *
 *  Created on: Aug 24, 2011
 *      Author: stu
 */

#include "ReadyToReceive.h"
#include <arpa/inet.h>
#include <string.h>

unsigned long long htonll(unsigned long long v) {
	union {
		unsigned long lv[2];
		unsigned long long llv;
	} u;
	u.lv[0] = htonl(v >> 32);
	u.lv[1] = htonl(v & 0xFFFFFFFFULL);
	return u.llv;
}

unsigned long long ntohll(unsigned long long v) {
	union {
		unsigned long lv[2];
		unsigned long long llv;
	} u;
	u.llv = v;
	return ((unsigned long long) ntohl(u.lv[0]) << 32)
			| (unsigned long long) ntohl(u.lv[1]);
}

ReadyToReceive::ReadyToReceive(int datasize, int fragmentsize, int loop) {
	
	this->datasize = datasize;
	this->fragmentsize = fragmentsize;
	this->loop = loop;
	this->mrList = NULL;
	this->logger = Logger::getInstance();
}

ReadyToReceive::ReadyToReceive(int datasize, int fragmentsize, int loop,
		vector<struct ibv_mr *> *mrList) {
	this->datasize = datasize;
	this->fragmentsize = fragmentsize;
	this->loop = loop;
	this->mrList = mrList;
	this->logger = Logger::getInstance();
}

ReadyToReceive::~ReadyToReceive() {
	
}

int ReadyToReceive::getDatasize() {
	return this->datasize;
}

int ReadyToReceive::getFragmentsize() {
	return this->getFragmentsize();
}

int ReadyToReceive::getLoop() {
	return this->getLoop();
}

unsigned long int ReadyToReceive::size() {
	int basesize = 16;
	if (mrList != NULL) {
		basesize += mrList->size() * 4 + mrList->size() * 8;
	}
	return basesize;
}

vector<struct ibv_mr *> *ReadyToReceive::getMrList() {
	return this->mrList;
}

void ReadyToReceive::writeBack(ByteBuffer *byteBuffer) {
	logger->info("write back on buffer of size", byteBuffer->getSize());
	byteBuffer->clear();
	byteBuffer->putInt(datasize);
	byteBuffer->putInt(fragmentsize);
	byteBuffer->putInt(loop);
	if (mrList != NULL) {
		byteBuffer->putInt(mrList->size());
		for (int i = 0; i < mrList->size(); i++) {
			struct ibv_mr *mr = mrList->at(i);
			int lkey = mr->lkey;
			unsigned long long addr = (unsigned long long) mr->addr;
			byteBuffer->putLong(addr);
			byteBuffer->putInt(lkey);
		}
	} else {
		byteBuffer->putInt(0);
	}
}

void ReadyToReceive::update(ByteBuffer *byteBuffer) {
//	logger->info("update back on buffer of size", byteBuffer->getSize());
	byteBuffer->clear();
	this->datasize = byteBuffer->getInt();
//	logger->info("updating datasize ", this->datasize);
	this->fragmentsize = byteBuffer->getInt();
//	logger->info("updating fragmentsize ", this->fragmentsize);
	this->loop = byteBuffer->getInt();
//	logger->info("updating loop ", this->loop);
	int mrsize = byteBuffer->getInt();
//	logger->info("updating mrsize ", mrsize);
	if (mrsize > 0) {
		mrList = new vector<struct ibv_mr *>();
		for (int i = 0; i < mrsize; i++) {
			unsigned long long addr = byteBuffer->getLong();
//			logger->info("updating addr ", addr);

			unsigned int lkey = byteBuffer->getInt();
//			logger->info("updating lkey ", lkey);

			struct ibv_mr *mr = new struct ibv_mr;
			mr->lkey = lkey;
			mr->addr = (void *) addr;
			mrList->push_back(mr);
		}
	}
}

