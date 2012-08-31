/*
 * ExperimentManager2.cpp
 *
 *  Created on: Aug 31, 2012
 *      Author: jie
 */

#include "SenderStatusProxy.h"
#include "ExperimentManager2.h"


ExperimentManager2::ExperimentManager2() {
}


ExperimentManager2::~ExperimentManager2() {

}


void ExperimentManager2::StartExperiment(SenderStatusProxy* sender_proxy, MVCTPSender* sender) {
	const int NUM_FILES = 10;

	char file_name[256];
	for (int i = 0; i < NUM_FILES; i++) {
		sprintf(file_name, "/tmp/temp/temp%d.dat", i + 1);
		sender->SendFile(file_name);
	}
}



void ExperimentManager2::HandleExpResults(string msg) {
	if (result_file.is_open() && finished_node_count < num_test_nodes) {
		finished_node_count++;
		if (finished_node_count == num_test_nodes)
			result_file.flush();
	}
}

