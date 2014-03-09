/*
 * DataPlaneRdma.h
 *
 *  Created on: Feb 9, 2012
 *      Author: stu
 */

#ifndef DATAPLANERDMA_H_
#define DATAPLANERDMA_H_

#include <rdma/rdma_cma.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "Logger.h"

class DataPlaneRdma {
public:
	DataPlaneRdma(struct ibv_comp_channel *comp_channel, struct ibv_cq *cq, struct ibv_qp *qp);
	virtual ~DataPlaneRdma();

	void writeSG(struct ibv_send_wr *local_triplet_tx_wr,
			struct ibv_send_wr *bad_tx_wr, bool signaled);
	void readSG(struct ibv_send_wr *local_triplet_tx_wr,
			struct ibv_send_wr *bad_tx_wr, bool signaled);
	int initSGRead(struct ibv_recv_wr *local_triplet_tx_wr,
			struct ibv_recv_wr *bad_tx_wr);
	bool completeSGRead(int expectedElements);

private:
	int writeOps;
	int readOps;
	int errorOps;

	struct ibv_comp_channel *comp_channel;
	struct ibv_cq *cq;
	struct ibv_qp *qp;

	Logger *logger;
};

#endif /* DATAPLANERDMA_H_ */
