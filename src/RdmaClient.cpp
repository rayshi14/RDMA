/*
 * RdmaClient.cpp
 *
 *  Created on: Sep 14, 2011
 *      Author: stu
 */

#include "RdmaClient.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>

#include "BufferUtils.h"
#include <errno.h>
#include <string.h>
#include "ReadyToReceive.h"
#include "ControlUtils.h"

#include <signal.h>
#include <errno.h>

#include "DataPlaneRdma.h"

bool RdmaClient::mbGotExitSignal = false;

RdmaClient::RdmaClient(AppLauncher *applauncher) {
	this->logger = Logger::getInstance();
	this->clientIP = applauncher->getIPAddress();
	this->filename = applauncher->getFilename();
	this->size = applauncher->getSize();
	this->loop = applauncher->getLoop();
	this->fragmentsize = applauncher->getFragmentSize();
	this->testCase = applauncher->getTestCase();

	this->stopWatch = StopWatch::getInstance();
	this->controlChannel = socket(AF_INET, SOCK_STREAM, 0);
	this->control_port = applauncher->getControlPort();
	this->data_port = applauncher->getDataPort();
	this->timeout = 2000;
	this->ncqe = 50;
	this->comp_vector = 0;
	this->access = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ;

	this->cm_channel = NULL;
	this->comp_channel = NULL;
	this->pd = NULL;
	this->id = NULL;
	this->cq = NULL;
	this->qp = NULL;

	if (signal((int) SIGINT, RdmaClient::exitSignalHandler) == SIG_ERR){
		logger->severe("could not install signal handler");
	} else {
		logger->info("signal handler installed");
	}
}

RdmaClient::~RdmaClient() {

}

void RdmaClient::run() {
	/*
	 * Initialize RDMA 
	 * data structuctures
	 * (QPs, CQ, connection, etc.)
	 * and control data structure
	 * (socket, connection, etc.)
	 */

	int ret;
	in_addr_t server_addr = inet_addr(clientIP.c_str());
	in_port_t server_port = htons(data_port);
	struct rdma_conn_param conn_param;
	struct rdma_cm_event *cm_event;
	struct sockaddr_in s_addr;
	struct ibv_qp_init_attr qp_init_attr;
	struct ibv_cq *dst_cq;
	void *dst_cq_ctx;
	struct ibv_wc wc;
	struct ibv_sge local_triplet_sge;
	struct sockaddr_in controlAddress;
	memset(&controlAddress, 0, sizeof controlAddress);
	controlAddress.sin_family = AF_INET;
	controlAddress.sin_addr.s_addr = server_addr;
	controlAddress.sin_port = htons(control_port);

	cm_channel = rdma_create_event_channel();
	if (!cm_channel) {
		logger->severe("creating cm event channel failed");
		return;
	} else {
		logger->info("cm event channel created");
	}

	ret = rdma_create_id(cm_channel, &id, NULL, RDMA_PS_TCP);
	if (ret) {
		logger->severe("creating listen cm id failed");
		return;
	} else {
		logger->info("listen cm id created, port_num %d", id->port_num);
	}

	memset(&s_addr, 0, sizeof s_addr);
	s_addr.sin_family = AF_INET;
	s_addr.sin_addr.s_addr = server_addr;
	s_addr.sin_port = server_port;
	ret = rdma_resolve_addr(id, NULL, (struct sockaddr*) &s_addr, 2000);
	if (ret) {
		logger->severe("failed to resolve address");
		return;
	}

	logger->info("resolve_addr, port_num ", id->port_num);

	ret = rdma_get_cm_event(cm_channel, &cm_event);
	if (ret) {
		logger->severe("failed to get cm event");
		return;
	}

	logger->info("get_cm_event, port_num ", id->port_num);

	if (cm_event->event != RDMA_CM_EVENT_ADDR_RESOLVED) {
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
	} else {
		logger->info("address resolved");
	}

	/* route */
	ret = rdma_resolve_route(id, 2000);
	if (ret) {
		logger->severe("failed to resolve route (%m)");
		return;
	}
	ret = rdma_get_cm_event(cm_channel, &cm_event);
	if (ret) {
		logger->severe("failed to get cm event (%m)");
		return;
	}
	if (cm_event->event != RDMA_CM_EVENT_ROUTE_RESOLVED) {
		logger->severe("wrong event received: %s");
		return;
	} else if (cm_event->status != 0) {
		logger->severe("event has error status:");
		return;
	}
	ret = rdma_ack_cm_event(cm_event);
	if (ret) {
		logger->severe("failed to acknowledge cm event");
		return;
	} else {
		logger->info("route resolved");
	}

	pd = ibv_alloc_pd(id->verbs);
	if (!pd) {
		logger->severe("failed to alloc pd");
		return;
	} else {
		logger->info("pd allocated");
	}

	comp_channel = ibv_create_comp_channel(id->verbs);
	if (!comp_channel) {
		logger->severe("failed to create completion event channel");
		return;
	} else {
		logger->info("comp channel created");
	}

	cq = ibv_create_cq(id->verbs, N_CQE, NULL, comp_channel, 0);
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
	qp_init_attr.cap.max_recv_wr = 2;
	qp_init_attr.cap.max_send_sge = 1;
	qp_init_attr.cap.max_send_wr = 2;
	qp_init_attr.qp_type = IBV_QPT_RC;
	qp_init_attr.recv_cq = cq;
	qp_init_attr.send_cq = cq;
	ret = rdma_create_qp(id, pd, &qp_init_attr);
	if (ret) {
		logger->severe("failed to create qp");
		return;
	} else {
		logger->info("qp created, max_recv_sge", (int) qp_init_attr.cap.max_recv_sge);
	}
	qp = id->qp;

	vector<ByteBuffer *> *fragments = BufferUtils::getBufferFragments(size,
			fragmentsize);

	if (connect(controlChannel, (struct sockaddr *) &controlAddress,
			sizeof(controlAddress)) < 0) {
		logger->severe("ERROR connecting");
	} else {
		logger->info("control connection established");
	}
	ControlUtils *controlUtils = new ControlUtils(controlChannel);
	
	memset(&conn_param, 0, sizeof(conn_param));
	conn_param.initiator_depth = 5;
	conn_param.responder_resources = 5;
	conn_param.retry_count = 2;

	ret = rdma_connect(id, &conn_param);
	if (ret) {
		logger->severe("failed to connect to remote host");
		return;
	}
	ret = rdma_get_cm_event(cm_channel, &cm_event);
	if (ret) {
		logger->severe("failed to get cm event");
		return;
	}
	if (cm_event->event != RDMA_CM_EVENT_ESTABLISHED) {
		logger->severe("wrong event received:", cm_event->event);
		return;
	} else if (cm_event->status != 0) {
		logger->severe("event has error status:");
		return;
	}
	ret = rdma_ack_cm_event(cm_event);
	if (ret) {
		logger->severe("failed to acknowledge cm event");
		return;
	} else {
		logger->info("rdma connected");
	}
	
	//------------------ end of rdma init ----------------

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
	int access_send = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ;
	int i = 0;


	/* ##IMPLEMENT## */
	for (i = 0; i < fragments->size(); i++) {
		ByteBuffer *current = fragments->at(i);
		struct ibv_mr *mr = ibv_reg_mr(pd, current->getBuffer(),
				current->getSize(), access);
		printf("address is %d\n", current->getBuffer());
		if (mr != NULL){
			sg_list_send = (struct ibv_sge *) malloc(sizeof(struct ibv_sge));
			sg_list_send->addr = (uint64_t) (unsigned long) current->getBuffer();
			sg_list_send->length = current->getSize();
			sg_list_send->lkey = mr->lkey;

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
	int access_recv = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ;
	i = 0;



	/* ##IMPLEMENT## */
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
			currentWR_recv->wr_id = i+1;
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
    /* ##IMPLEMENT## */



	//------------------ end of benchmark init ----------------

	/* 
	 * Exchange
	 * RDMA/stags
	 * with server
	 */ 
	ReadyToReceive *client2Server = new ReadyToReceive(size, fragmentsize, loop, mrList_recv);
	ReadyToReceive *server2Client = new ReadyToReceive(size, fragmentsize, loop, mrList_send);
	ByteBuffer *client2ServerBuffer = ByteBuffer::allocate(client2Server->size());
	ByteBuffer *server2ClientBuffer = ByteBuffer::allocate(server2Client->size());

	client2Server->writeBack(client2ServerBuffer);
	controlUtils->startNextRound(client2ServerBuffer); /* inform server to exchange info */

	for (int i = 0; i < client2Server->getMrList()->size(); i++) {
		struct ibv_mr *mr = client2Server->getMrList()->at(i);
		int lkey = mr->lkey;
		unsigned long long addr = (unsigned long long) mr->addr;
		logger->info("used addr", addr);
		logger->info("used stag", lkey);
	}

	controlUtils->waitForNextRound(server2ClientBuffer); /* receive the information of server */
	server2Client->update(server2ClientBuffer);
	
	i = 0;
	for (currentWR_send = wrlist_send; currentWR_send; currentWR_send = currentWR_send->next) {
		struct ibv_mr *mr = server2Client->getMrList()->at(i++);
		int lkey = mr->lkey;
		unsigned long long addr = (long) mr->addr;
		currentWR_send->wr.rdma.remote_addr = addr;
		currentWR_send->wr.rdma.rkey = lkey;
		currentWR_send->opcode = IBV_WR_RDMA_READ;
		currentWR_send->send_flags |= IBV_SEND_SIGNALED;
		logger->info("received addr", addr);
		logger->info("received stag", lkey);
	}

	logger->warning("init handshake done");


	/* 
	 * Main loop
	 * in write-mode:
	 * send a TCP request
	 * to the server
	 * and receive
	 * a buffer through RDMA recv
	 * in read-mode:
	 * read a buffer from
	 * the server through
	 * RDMA read
	 */
	DataPlaneRdma *dataPlane = new DataPlaneRdma(comp_channel, cq, qp);

	double sumbytes = 0;
	stopWatch->start();
	logger->info("starting loop", loop);
	double ops = 0;
	for (int i = 0; !RdmaClient::mbGotExitSignal && i < loop; i++) {
		if (testCase == TEST_WRITE) {
			int wrElements = dataPlane->initSGRead(wrlist_recv, bad_tx_wr_recv);
			logger->severe("let's start round", i);
			controlUtils->startNextRound(client2ServerBuffer);
			bool res = dataPlane->completeSGRead(wrElements);
			if (res) {
				logger->severe("properly tagged data received");
				BufferUtils::checkTags(fragments);
				this->writeOps++;
			} else {
				logger->severe("incorrect data received");
				this->errorOps += 1.0;
			}
		} else if (testCase == TEST_READ) {
			logger->severe("about to rdma, round", i);
			dataPlane->readSG(wrlist_send, bad_tx_wr_send, true);
			BufferUtils::checkTags(fragments);
			logger->severe("read completed");
			this->readOps++;
		}
		
		sumbytes += ((double) size);
		ops += 1.0;
	}
	
    /* ##IMPLEMENT## */
    // Calculate the throughput and update this->throughput
	double executionTime = 1000 * stopWatch->getExecutionTime();
	this->throughput = 8 * sumbytes / executionTime;

	controlUtils->startNextRound(client2ServerBuffer);
	controlUtils->waitForNextRound(server2ClientBuffer);
	logger->warning("final handshake done");
	logger->severe("throughput computed [Mbit/s]:", (int) this->throughput);

	ret = rdma_disconnect(id);
	if (ret) {
		logger->severe("disconnect failed");
		return;
	}
	ret = rdma_get_cm_event(cm_channel, &cm_event);
	if (ret) {
		logger->severe("failed to get cm event");
		return;
	}
	if (cm_event->event != RDMA_CM_EVENT_DISCONNECTED) {
		logger->severe("wrong event received:");
		return;
	} else if (cm_event->status != 0) {
		logger->severe("event has error status");
		return;
	}
	ret = rdma_ack_cm_event(cm_event);
	if (ret) {
		logger->severe("failed to acknowledge cm event");
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

	return;
}

void RdmaClient::_close() {
	close(this->controlChannel);
}

double RdmaClient::getThroughput() {
	return this->throughput;
}

double RdmaClient::getReadOps() {
	return this->readOps;
}

double RdmaClient::getWriteOps() {
	return this->writeOps;
}

double RdmaClient::getErrorOps() {
	return this->errorOps;
}

double RdmaClient::getLatency() {
	return this->latency;
}

void RdmaClient::exitSignalHandler(int sig){
	cout << "exit signal handler" << endl;
	RdmaClient::mbGotExitSignal = true;
}

