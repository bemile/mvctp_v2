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

private:
	ofstream result_file;
};


#endif /* EXPERIMENTMANAGER_H_ */
