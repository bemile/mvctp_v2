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

class ExperimentManager {
public:
	ExperimentManager();
	~ExperimentManager();

	void StartExperiment(SenderStatusProxy* sender_proxy);
	void HandleExpResults(string msg);

	ulong 	GetFileSize() {return file_size;}
	int 	GetSendRate() {return send_rate;}

private:
	ulong file_size;
	int send_rate;
};


#endif /* EXPERIMENTMANAGER_H_ */
