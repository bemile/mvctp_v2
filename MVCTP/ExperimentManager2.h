/*
 * ExperimentManager2.h
 *
 *  Created on: Aug 31, 2012
 *      Author: jie
 */

#ifndef EXPERIMENTMANAGER2_H_
#define EXPERIMENTMANAGER2_H_

#include <iostream>
#include <string>
#include <vector>
#include <stdio.h>

class SenderStatusProxy;
class MVCTPSender;


struct File_Sample {
	int total_file_size;
	double total_time;
	vector<int> file_sizes;
	vector<double> inter_arrival_times;

	File_Sample() {
		total_file_size = 0;
		total_time = 0.0;
	}
};


class ExperimentManager2 {
public:
public:
	ExperimentManager2();
	~ExperimentManager2();

	void StartExperiment(SenderStatusProxy* sender_proxy, MVCTPSender* sender);
	void StartExperiment2(SenderStatusProxy* sender_proxy, MVCTPSender* sender);
	void ReadFileSizes(vector<int>& file_sizes);
	void GenerateFile(string file_name, int size);
	void ReadInterArrivals(vector<double>& file_sizes);

	void HandleExpResults(string msg);

private:
	int num_test_nodes;
	int finished_node_count;
	ofstream result_file;
	int retrans_scheme;
	int num_retrans_thread;

	pthread_mutex_t write_mutex;

	File_Sample GenerateFiles();
	//void DoSpeedTest(SenderStatusProxy* sender_proxy, MVCTPSender* sender);
	//void DoLowSpeedExperiment(SenderStatusProxy* sender_proxy, MVCTPSender* sender);
};

#endif /* EXPERIMENTMANAGER2_H_ */
