/*
 * DataPlaneRdma.cpp
 *
 *  Created on: Feb 9, 2012
 *      Author: stu
 */

#include "DataPlaneRdma.h"

DataPlaneRdma::DataPlaneRdma(struct ibv_comp_channel *comp_channel, struct ibv_cq *cq, struct ibv_qp *qp){
	this->comp_channel = comp_channel;
	this->cq = cq;
	this->qp = qp;

	this->writeOps = 0;
	this->errorOps = 0;
	this->readOps = 0;
}

DataPlaneRdma::~DataPlaneRdma() {
	
}

/*
 * Used in write mode
 * at the server
 * to RDMA send a buffer
 * to a client after
 * having received
 * a request from 
 * the client through
 * TCP
 */
void DataPlaneRdma::writeSG(struct ibv_send_wr *wrlist,
		struct ibv_send_wr *bad_tx_wr, bool signaled) {
	struct ibv_cq *dst_cq;
	void *dst_cq_ctx;
	struct ibv_wc wc;
	struct ibv_send_wr *i;
	int wr_count = 0;
	int sge_count = 0;
	int elementsRead = 0;
	int ret = 0;

	for (i = wrlist; i; i = i->next) {
		wr_count++;
		sge_count += i->num_sge;
	}

	ibv_post_send(qp, wrlist, &bad_tx_wr);
	if (ret < 0) {
		logger->info("failed to ibv_post_send");
	}
	writeOps++;

	if (signaled) {
		while (true) {
			ret = ibv_req_notify_cq(cq, 0);
			ret = ibv_poll_cq(cq, wr_count, &wc);
			if (ret < 0) {
				logger->info("failed to poll");
				break;
			}
			elementsRead += ret;
			logger->info("elements read:", elementsRead);
			if (elementsRead < wr_count) {
				if (ret < 0) {
					logger->info("failed to ibv_req_notify_cq");
					break;
				}
				ibv_get_cq_event(comp_channel, &dst_cq, &dst_cq_ctx);
				if (ret < 0) {
					logger->info("failed to ibv_get_cq_event");
					break;
				}
			} else {
				break;
			}
		}
	}
}


/* ##IMPLEMENT##
 * Used in read mode 
 * at the client
 * to receive a buffer
 * through a one-sided
 * RDMA read operation
 * from a server
 * without invoking
 * the server
 */
void DataPlaneRdma::readSG(struct ibv_send_wr *wrlist,
		struct ibv_send_wr *bad_tx_wr, bool signaled) {
	
	struct ibv_cq *dst_cq;
	void *dst_cq_ctx;
	struct ibv_wc wc;
	struct ibv_send_wr *i;
	int wr_count = 0;
	int sge_count = 0;
	int elementsRead = 0;
	int ret = 0;

	for (i = wrlist; i; i = i->next) {
		wr_count++;
		sge_count += i->num_sge;
	}

	ibv_post_send(qp, wrlist, &bad_tx_wr);
	if (ret < 0) {
		logger->info("failed to ibv_post_send");
	}
	writeOps++;

	if (signaled) {
		while (true) {
			ret = ibv_req_notify_cq(cq, 0);
			ret = ibv_poll_cq(cq, wr_count, &wc);
			if (ret < 0) {
				logger->info("failed to poll");
				break;
			}
			elementsRead += ret;
			logger->info("elements read:", elementsRead);
			if (elementsRead < wr_count) {
				if (ret < 0) {
					logger->info("failed to ibv_req_notify_cq");
					break;
				}
				ibv_get_cq_event(comp_channel, &dst_cq, &dst_cq_ctx);
				if (ret < 0) {
					logger->info("failed to ibv_get_cq_event");
					break;
				}
			} else {
				break;
			}
		}
	}


}




/* 
 * Used in write mode
 * at the client
 * to initiate a
 * RDMA read operation
 * which will be used
 * to receive a buffer
 * sent from the server
 * 
 */
int DataPlaneRdma::initSGRead(struct ibv_recv_wr *wrlist,
		struct ibv_recv_wr *bad_tx_wr) {
	struct ibv_recv_wr *i;
	int wr_count = 0;
	int sge_count = 0;
	for (i = wrlist; i; i = i->next) {
		wr_count++;
		sge_count += i->num_sge;
	}

	int ret = ibv_post_recv(qp, wrlist, &bad_tx_wr);
	if (ret < 0) {
		logger->info("failed to post_recv");
		return ret;
	} else {
		return wr_count;
	}
}



/* ##IMPLEMENT##
 * Used in write mode
 * at the client to wait
 * until buffer has
 * completely been 
 * received.
 * Hint: you can use ibv_req_notify_cq, ibv_poll_cq, ibv_get_cq_event
 */
bool DataPlaneRdma::completeSGRead(int expectedElements) {
	struct ibv_cq *dst_cq;
	void *dst_cq_ctx;
	struct ibv_wc wc;
	int elementsRead = 0;
	int ret = 0;

	while (true) {
		ret = ibv_req_notify_cq(cq, 0);
		ret = ibv_poll_cq(cq, expectedElements, &wc);
		if (ret < 0) {
			logger->info("failed to poll");
			break;
		}
		elementsRead += ret;
		logger->info("elements read:", elementsRead);
		if (elementsRead < expectedElements){
			if (ret < 0) {
				logger->info("failed to ibv_req_notify_cq");
				break;
			}
			ibv_get_cq_event(comp_channel, &dst_cq, &dst_cq_ctx);
			if (ret < 0) {
				logger->info("failed to ibv_get_cq_event");
				break;
			}
		} else {
			break;
		}
	}
	return true;
}

