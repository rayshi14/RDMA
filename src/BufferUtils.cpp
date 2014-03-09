/*
 * BufferUtils.cpp
 *
 *  Created on: Sep 14, 2011
 *      Author: stu
 */

#include "BufferUtils.h"

#include <iostream>
#include "Logger.h"

using namespace std;

BufferUtils::BufferUtils() {
}

BufferUtils::~BufferUtils() {
}

vector<ByteBuffer *> *BufferUtils::getBufferFragments(int size,
		int fragmentsize) {
	Logger *logger = Logger::getInstance();

	int remainingsize = size;
	vector<int> *_fragments = new vector<int>();
	while (remainingsize > 0) {
		int currentbuffersize = fragmentsize;
		if (currentbuffersize > remainingsize) {
			currentbuffersize = remainingsize;
		}
		_fragments->push_back(currentbuffersize);
		remainingsize -= currentbuffersize;
	}

	logger->info("getting buffer, nbr of fragments ", (int) _fragments->size());
	vector<ByteBuffer *> *fragments = new vector<ByteBuffer *>();
	int i = 0;
	int offset = 0;
	for (i = 0; i < _fragments->size(); i++) {
		int _size = _fragments->at(i);
		logger->info("allocating buffer, size", size);
		ByteBuffer *buffer = ByteBuffer::allocate(_size);
		fragments->push_back(buffer);
		offset += _size;
	}

	return fragments;
}

void BufferUtils::dumpFragments(vector<ByteBuffer *> *fragments){
	Logger *logger = Logger::getInstance();
	for (int i = 0; i < fragments->size(); i++) {
		ByteBuffer *buffer = fragments->at(i);
		logger->info("dumping server buffer, size", (int) buffer->getSize());
		buffer->dump();
	}
}

void BufferUtils::addTags(vector<ByteBuffer *> *fragments, int head, int tail){
	Logger *logger = Logger::getInstance();
	ByteBuffer *first = fragments->front();
	ByteBuffer *last = fragments->back();

	first->clear();
	first->putInt(head);
	last->clear();
	last->putInt(tail);

//	logger->info("data buffer before sending");
//	first->dump();
}

void BufferUtils::checkTags(vector<ByteBuffer *> *fragments){
	Logger *logger = Logger::getInstance();
	ByteBuffer *first = fragments->front();
	ByteBuffer *last = fragments->back();

	first->clear();
	int startTag = first->getInt();
	last->clear();
	int endTag = last->getInt();

	logger->info("startTag", startTag);
	logger->info("endTag", endTag);
}

