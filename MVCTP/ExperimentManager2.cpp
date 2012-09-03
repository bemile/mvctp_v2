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

static const int FILE_COUNT = 1000;
void ExperimentManager2::StartExperiment(SenderStatusProxy* sender_proxy, MVCTPSender* sender) {
	system("mkdir /tmp/temp");
	system("cp /users/jieli/src/file_sizes.txt /tmp/temp");
	system("cp /users/jieli/src/inter_arrival_times.txt /tmp/temp");

	cout << "Genearating files..." << endl;
	GenerateFiles();

	vector<double> inter_arrival_times;
	ifstream infile;
	double time;
	infile.open("/tmp/temp/inter_arrival_times.txt");
	for (int i = 0; i < FILE_COUNT; i++) {
		infile >> time;
		inter_arrival_times.push_back(time);
	}
	infile.close();


	struct timespec time_spec;
	time_spec.tv_sec = 0;
	time_spec.tv_nsec = 0;

	CpuCycleCounter cpu_counter;
	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);
	double last_time_mark = 0.0;

	char file_name[256];
	for (int i = 0; i < FILE_COUNT; i++) {
		double curr_time = GetElapsedSeconds(cpu_counter);
		if (curr_time - last_time_mark < inter_arrival_times[i]) {
			time_spec.tv_nsec = (curr_time - last_time_mark) * 1000000000;
			nanosleep(&time_spec, NULL);
			last_time_mark = GetElapsedSeconds(cpu_counter);
		}

		sprintf(file_name, "/tmp/temp/temp%d.dat", i + 1);
		sender->SendFile(file_name);
	}
}

void ExperimentManager2::GenerateFiles() {
	static const int BUF_SIZE = 4096;

	ifstream infile ("/tmp/temp/file_sizes.txt");
	int size = 0;
	char file_name[50];
	int file_index = 1;
	char buf[BUF_SIZE];
	int count = 0;
	while (infile >> size) {
		int remained_size = size;
		sprintf(file_name, "/tmp/temp/temp%d.dat", file_index++);
		ofstream outfile (file_name, ofstream::binary | ofstream::trunc);
		while (remained_size > 0) {
			int data_len = (remained_size > BUF_SIZE) ? BUF_SIZE : remained_size;
			outfile.write(buf, data_len);
			remained_size -= data_len;
		}
		outfile.close();

		if (file_index > FILE_COUNT)
			break;
	}
}


void ExperimentManager2::HandleExpResults(string msg) {
	if (result_file.is_open() && finished_node_count < num_test_nodes) {
		finished_node_count++;
		if (finished_node_count == num_test_nodes)
			result_file.flush();
	}
}

