/*
 * RdmaServer.cpp
 *
 *  Created on: Sep 14, 2011
 *      Author: stu
 */

#include "RdmaServer.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>

#include "BufferUtils.h"
#include <errno.h>
#include <string.h>
#include "ReadyToReceive.h"
#include <sstream>
#include <iostream>
#include <string>
#include "ControlUtils.h"

#include "DataPlaneRdma.h"

RdmaServer::RdmaServer(AppLauncher *applauncher) {
	this->logger = Logger::getInstance();
	this->serverIP = applauncher->getIPAddress();
	this->filename = applauncher->getFilename();
	this->size = applauncher->getSize();
	this->fragmentsize = applauncher->getFragmentSize();
	this->loop = applauncher->getLoop();
	this->control_port = applauncher->getControlPort();
	this->data_port = applauncher->getDataPort();
	this->testCase = applauncher->getTestCase();

	this->backlog = 1;
	this->ncqe = 50;
	this->comp_vector = 0;
	this->access = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ;
	this->controlServerChannel = socket(AF_INET, SOCK_STREAM, 0); /* a socket for transmitting control message */
	int on = 1;
	setsockopt(controlServerChannel, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	this->stopWatch = StopWatch::getInstance();

	this->cm_channel = NULL;
	this->comp_channel = NULL;
	this->pd = NULL;
	this->cm_listen_id = NULL;
	this->cq = NULL;
	this->qp = NULL;
}

RdmaServer::~RdmaServer() {

}

void RdmaServer::run() {
        /*
         * Initialize RDMA
         * data structuctures
         * (QPs, CQ, connection, etc.)
         * and control data structure
         * (socket, connection, etc.)
         */

	int ret;
	stringstream ss;
	struct sockaddr_in controlAddress;
	memset(&controlAddress, 0, sizeof controlAddress);
	controlAddress.sin_family = AF_INET;
	controlAddress.sin_addr.s_addr = INADDR_ANY;
	controlAddress.sin_port = htons(control_port);
	struct sockaddr_in s_addr;
	memset(&s_addr, 0, sizeof s_addr);
	s_addr.sin_family = AF_INET;
	s_addr.sin_addr.s_addr = inet_addr(serverIP.c_str());
	s_addr.sin_port = htons(data_port);
	struct rdma_conn_param conn_param;
	struct rdma_cm_event *cm_event;
	struct rdma_cm_id *cm_conn_id;
	struct ibv_qp_init_attr qp_init_attr;
	struct ibv_cq *dst_cq;
	void *dst_cq_ctx;
	struct ibv_wc wc;
	struct ibv_sge file_recv_sge;
	static struct ibv_recv_wr file_recv_rx_wr, *bad_rx_wr;
	struct app1_rdma_buf *rdma_file_recv_buff;
	vector<ByteBuffer *> *fragments = BufferUtils::getBufferFragments(size,
			fragmentsize); 
	/*
		allocate buffer with each 'fragmentsize' large and in total 'size' large
		size is the remaining size, fragmentsize is the size of each fragment	
	*/

	cm_channel = rdma_create_event_channel(); 
	/* create an event-driven connection manager channel to connect queue pairs, manage state transitions, and handle errors*/
	if (!cm_channel) {
		logger->severe("creating cm event channel failed");
		return;
	} else {
		logger->info("cm event channel created");
	}

	ret = rdma_create_id(cm_channel, &cm_listen_id, NULL, RDMA_PS_TCP); 
	if (ret) {
		logger->severe("creating listen cm id failed");
		return;
	} else {
		logger->info("listen cm id created");
	}

	ret = rdma_bind_addr(cm_listen_id, (struct sockaddr*) &s_addr);
	if (ret) {
		logger->severe("failed to bind server address");
		return;
	} else {
		logger->info("binding done");
	}

	ret = rdma_listen(cm_listen_id, 1); /* listen only 1 connection at one time*/
	if (ret < 0) {
		logger->severe("ERROR on listening");
	} else {
		logger->info("control channel listen successful");
	}

	ret = bind(controlServerChannel, (struct sockaddr *) &controlAddress, /* bind socket controlServerChannel to controlAddress */
			sizeof(controlAddress));
	if (ret < 0) {
		logger->severe("ERROR on binding");
	} else {
		logger->info("control channel bind successful");
	}

	ret = listen(controlServerChannel, 5); /* listen on socket controlServerChannel */
	if (ret < 0) {
		logger->severe("ERROR on listening");
	} else {
		logger->info("control channel listen successful");
	}

	controlChannel = accept(controlServerChannel, NULL, NULL);/* accept request from socket controlServerChannel*/
	if (controlChannel < 0) {
		logger->severe("ERROR on accept");
	} else {
		logger->info("control channel set up ", controlChannel);
	}
	ControlUtils *controlUtils = new ControlUtils(controlChannel);

	ret = rdma_get_cm_event(cm_channel, &cm_event);
	if (ret) {
		logger->severe("failed to get cm event");
		return;
	}
	if (cm_event->event != RDMA_CM_EVENT_CONNECT_REQUEST) {
		logger->severe("wrong event received", cm_event->event);
		return;
	} else if (cm_event->status != 0) {
		logger->severe("event has error status:");
		return;
	}
	cm_conn_id = cm_event->id;
	ret = rdma_ack_cm_event(cm_event);
	if (ret) {
		logger->severe("failed to acknowledge cm event");
		return;
	} else {
		logger->info("connect request received");
	}
	
	pd = ibv_alloc_pd(cm_conn_id->verbs);
	if (!pd) {
		logger->severe("failed to alloc pd");
		return;
	} else {
		logger->info("pd allocated");
	}

	comp_channel = ibv_create_comp_channel(cm_conn_id->verbs);
	if (!comp_channel) {
		logger->severe("failed to create completion event channel");
		return;
	} else {
		logger->info("completion event channel created");
	}

	cq = ibv_create_cq(cm_conn_id->verbs, N_CQE, NULL, comp_channel, 0);
	if (!cq) {
		logger->severe("failed to create cq");
		return;
	}
	ret = ibv_req_notify_cq(cq, 0);
	if (ret) {
		logger->severe("failed to request notifications");
		return;
	} else {
		logger->info("cq created");
	}

	memset(&qp_init_attr, 0, sizeof qp_init_attr);
	qp_init_attr.cap.max_recv_sge = 1;
	qp_init_attr.cap.max_recv_wr = 2; // So 10 entries with 10 sge (=100)
	qp_init_attr.cap.max_send_sge = 1;
	qp_init_attr.cap.max_send_wr = 2;
	qp_init_attr.qp_type = IBV_QPT_RC;
	qp_init_attr.recv_cq = cq;
	qp_init_attr.send_cq = cq;

	ret = rdma_create_qp(cm_conn_id, pd, &qp_init_attr);
	if (ret) {
		logger->severe("failed to create qp");
		return;
	} else {
		logger->info("qp created, max_recv_sge", (int) qp_init_attr.cap.max_recv_sge);
	}
	qp = cm_conn_id->qp; /*bind connection manager to queue pair*/

	memset(&conn_param, 0, sizeof(conn_param));
	conn_param.initiator_depth = 5;
	conn_param.responder_resources = 5;
	/* private data for connection response are at conn_param... not used */
	ret = rdma_accept(cm_conn_id, &conn_param);

	logger->info("back from accept");

	if (ret) {
		logger->severe("failed to accept connid");
		return;
	}
	ret = rdma_get_cm_event(cm_channel, &cm_event);
	if (ret) {
		logger->severe("failed to get cm event");
		return;
	}

	logger->info("back from get_cm_event");

	if (cm_event->event != RDMA_CM_EVENT_ESTABLISHED) {
		logger->severe("wrong event received");
		return;
	} else if (cm_event->status != 0) {
		logger->severe("event has error status:");
		return;
	}
	ret = rdma_ack_cm_event(cm_event);
	if (ret) {
		logger->severe("failed to acknowledge cm event");
		return;
	}

	logger->info("back from ack cm_event");

	//------------------ end of rdma init ----------------
    
    
    /* 
     * Register
     * memory used
     * for receiving
     * with RDMA
     */
	struct ibv_sge *sg_list_recv;
	vector<struct ibv_mr *> *mrList_recv = new vector<struct ibv_mr *>();
	struct ibv_recv_wr *wrlist_recv = NULL, *currentWR_recv = NULL, *previousWR_recv = NULL;
	struct ibv_recv_wr *bad_tx_wr_recv;
	int i = 0;

	for (i = 0; i < fragments->size(); i++) {
		ByteBuffer *current = fragments->at(i);
		struct ibv_mr *mr = ibv_reg_mr(pd, current->getBuffer(),
				current->getSize(), access);

		if (mr != NULL){
			sg_list_recv = (struct ibv_sge *) malloc(sizeof(struct ibv_sge));
			sg_list_recv->addr = (uint64_t) (unsigned long) current->getBuffer();
			sg_list_recv->length = current->getSize();
			sg_list_recv->lkey = mr->lkey;

			previousWR_recv = currentWR_recv;
			currentWR_recv = new struct ibv_recv_wr;
			currentWR_recv->sg_list = sg_list_recv;
			currentWR_recv->num_sge = 1;
			currentWR_recv->next = NULL;

			if (wrlist_recv == NULL) {
				wrlist_recv = currentWR_recv;
			}
			if (previousWR_recv != NULL) {
				previousWR_recv->next = currentWR_recv;
			}

			mrList_recv->push_back(mr);
		} else {
			logger->warning("mr is NULL");
			return;
		}
	}

    /* 
     * Register
     * memory used
     * for sending
     * with RDMA
     */
	struct ibv_sge *sg_list_send;
	vector<struct ibv_mr *> *mrList_send = new vector<struct ibv_mr *>();
	struct ibv_send_wr *wrlist_send = NULL, *currentWR_send = NULL, *previousWR_send = NULL;
	struct ibv_send_wr *bad_tx_wr_send;
	i = 0;


    /* ##IMPLEMENT## */
	for (i = 0; i < fragments->size(); i++) {
		ByteBuffer *current = fragments->at(i);
		struct ibv_mr *mr = ibv_reg_mr(pd, current->getBuffer(),
				current->getSize(), access);
		
		if (mr != NULL){
			sg_list_send = (struct ibv_sge *) malloc(sizeof(struct ibv_sge));
			sg_list_send->addr = (uint64_t) (unsigned long) current->getBuffer();
			printf("address is %d\n", current->getBuffer());
			sg_list_send->length = current->getSize();
			sg_list_send->lkey = mr->lkey;
			printf("address is %d\n", mr->lkey);
			previousWR_send = currentWR_send;
			currentWR_send = new struct ibv_send_wr;
			currentWR_send->sg_list = sg_list_send;
			currentWR_send->num_sge = 1;
			currentWR_send->next = NULL;
			if (wrlist_send == NULL) {
				wrlist_send = currentWR_send;
			}
			if (previousWR_send != NULL) {
				previousWR_send->next = currentWR_send;
			}

			mrList_send->push_back(mr);
		} else {
			logger->warning("mr is NULL");
			return;
		}
	}
	/* ##IMPLEMENT## */
	


	if (testCase == TEST_READ){
		logger->info("preparing read case");
		BufferUtils::addTags(fragments, 66, 66);
		BufferUtils::checkTags(fragments);
	}
	
	//------------------ end of benchmark init ----------------

    /* 
     * Exchange
     * RDMA/stags
     */
	ReadyToReceive *client2Server = new ReadyToReceive(size, fragmentsize, loop, mrList_recv);
	ReadyToReceive *server2Client = new ReadyToReceive(size, fragmentsize, loop, mrList_send);
	ByteBuffer *client2ServerBuffer = ByteBuffer::allocate(client2Server->size());
	ByteBuffer *server2ClientBuffer = ByteBuffer::allocate(server2Client->size());


	/* ##IMPLEMENT## */
	server2Client->writeBack(server2ClientBuffer);
	controlUtils->waitForNextRound(client2ServerBuffer);
	client2Server->update(client2ServerBuffer);

	i = 0;
	for (currentWR_send = wrlist_send; currentWR_send; currentWR_send = currentWR_send->next) {
		struct ibv_mr *mr = client2Server->getMrList()->at(i++);
		int lkey = mr->lkey;
		unsigned long long addr = (long) mr->addr;
		currentWR_send->wr_id = i;
		currentWR_send->wr.rdma.remote_addr = addr;
		currentWR_send->wr.rdma.rkey = lkey;
		currentWR_send->opcode = IBV_WR_SEND;
		currentWR_send->send_flags |= IBV_SEND_SIGNALED;
		logger->info("received addr", addr);
		logger->info("received stag", lkey);
	}
	/* ##IMPLEMENT## */

	controlUtils->startNextRound(server2ClientBuffer);/* inform the client of server info */

	for (int i = 0; i < server2Client->getMrList()->size(); i++) {
		struct ibv_mr *mr = server2Client->getMrList()->at(i);
		int lkey = mr->lkey;
		unsigned long long addr = (unsigned long long) mr->addr;
		logger->info("used addr", addr);
		logger->info("used stag", lkey);
	}	

	logger->warning("init handshake done");
	
    /* 
     * Main loop
     * in write-mode:
     * receive a TCP request
     * from the client
     * and send
     * a buffer through RDMA send
     * in read-mode:
     * nothing to be done
     * at server, client
	 * reads buffer through
	 * one sided
     * RDMA read
     */

	DataPlaneRdma *dataPlane = new DataPlaneRdma(comp_channel, cq, qp);

	double sumbytes = 0;
	stopWatch->start();
	logger->info("starting loop", loop);
	for (int i = 0; i < loop; i++) {
		if (testCase == TEST_WRITE) {
			logger->severe("let's start round", i);
            BufferUtils::addTags(fragments, i+1, i+1);
            controlUtils->waitForNextRound(client2ServerBuffer);
			logger->severe("next round trigger received, round", i);
            dataPlane->writeSG(wrlist_send, bad_tx_wr_send, true);
            this->writeOps++;
            logger->severe("transmitted nbr of fragments:", (int) fragments->size());
		} else if (testCase == TEST_READ) {
		}
		sumbytes += ((double) size);
	}

    /* ##IMPLEMENT## */
    // Calculate the throughput and update this->throughput
	double executionTime = 1000 * stopWatch->getExecutionTime();
	this->throughput = 8 * sumbytes / executionTime;
	
	controlUtils->waitForNextRound(client2ServerBuffer);
	controlUtils->startNextRound(server2ClientBuffer);
	logger->warning("final handshake done");
	logger->severe("throughput computed [Mbit/s]:", (int) this->throughput);

	ret = rdma_get_cm_event(cm_channel, &cm_event);
	if (ret) {
		perror("failed to get cm event (%m)");
	}
	if (cm_event->event != RDMA_CM_EVENT_DISCONNECTED) {
		perror("wrong event received:");
		return;
	} else if (cm_event->status != 0) {
		perror("event has error status:");
		return;
	}
	ret = rdma_ack_cm_event(cm_event);
	if (ret) {
		perror("failed to acknowledge cm event");
		return;
	}

	if (testCase == TEST_READ){
		for (int i = 0; i < mrList_send->size(); i++) {
			struct ibv_mr *mr = mrList_send->at(i);
			ibv_dereg_mr(mr);
		}
	}
	if (testCase == TEST_WRITE) {
		for (int i = 0; i < mrList_recv->size(); i++) {
			struct ibv_mr *mr = mrList_recv->at(i);
			ibv_dereg_mr(mr);
		}
	}
}

void RdmaServer::_close() {
	close(this->controlServerChannel);
	close(this->controlChannel);
}

double RdmaServer::getThroughput() {
	return this->throughput;
}

double RdmaServer::getReadOps() {
	return this->readOps;
}

double RdmaServer::getWriteOps() {
	return this->writeOps;
}

double RdmaServer::getErrorOps() {
	return errorOps;
}

double RdmaServer::getLatency() {
	return 0;
}

