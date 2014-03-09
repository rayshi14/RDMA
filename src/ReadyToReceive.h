/*
 * ReadyToReceive.h
 *
 *  Created on: Aug 24, 2011
 *      Author: stu
 */

#ifndef READYTORECEIVE_H_
#define READYTORECEIVE_H_

#include "Logger.h"
#include "ByteBuffer.h"
#include <rdma/rdma_cma.h>
#include <vector>

using namespace std;

class ReadyToReceive {
public:
	ReadyToReceive(int datasize, int fragmentsize, int loop);
	ReadyToReceive(int datasize, int fragmentsize, int loop,
			vector<struct ibv_mr *> *mrList);
	virtual ~ReadyToReceive();

	void writeBack(ByteBuffer *byteBuffer);
	void update(ByteBuffer *byteBuffer);
	unsigned long int size();

	int getDatasize();
	int getFragmentsize();
	int getLoop();
	vector<struct ibv_mr *> *getMrList();

private:
	int datasize;
	int fragmentsize;
	int loop;
	vector<struct ibv_mr *> *mrList;
	Logger *logger;
};

#endif /* READYTORECEIVE_H_ */
