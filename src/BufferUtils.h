/*
 * BufferUtils.h
 *
 *  Created on: Sep 14, 2011
 *      Author: stu
 */

#ifndef BUFFERUTILS_H_
#define BUFFERUTILS_H_

#include "ByteBuffer.h"
#include <vector>

using namespace std;

class BufferUtils {
public:
	BufferUtils();
	virtual ~BufferUtils();

	static vector<ByteBuffer *> *getBufferFragments(int size, int fragmentsize);
	static void dumpFragments(vector<ByteBuffer *> *fragments);
	static void addTags(vector<ByteBuffer *> *fragments, int head, int tail);
	static void checkTags(vector<ByteBuffer *> *fragments);
};

#endif /* BUFFERUTILS_H_ */
