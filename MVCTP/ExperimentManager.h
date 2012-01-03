/*
 * ExperimentManager.h
 *
 *  Created on: Jan 2, 2012
 *      Author: jie
 */

#ifndef EXPERIMENTMANAGER_H_
#define EXPERIMENTMANAGER_H_

//#include "SenderStatusProxy.h"
#include <iostream>
#include <string>
#include <vector>

class SenderStatusProxy;
class MVCTPSender;

class ExperimentManager {
public:
	ExperimentManager();
	~ExperimentManager();

	void StartExperiment(SenderStatusProxy* sender_proxy, MVCTPSender* sender);
	void HandleExpResults(string msg);

	ulong 	GetFileSize() {return file_size;}
	int 	GetSendRate() {return send_rate;}

private:
	ulong file_size;
	int send_rate;
	int num_test_nodes;
	int finished_node_count;
	ofstream 			result_file;

	void DoSpeedTest(SenderStatusProxy* sender_proxy, MVCTPSender* sender);
};


#endif /* EXPERIMENTMANAGER_H_ */
