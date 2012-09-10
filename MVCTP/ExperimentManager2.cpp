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

static const int FILE_COUNT = 50;
static const int BASE_NUM = 1000;
void ExperimentManager2::StartExperiment(SenderStatusProxy* sender_proxy, MVCTPSender* sender) {
	system("mkdir /tmp/temp");
	system("cp /users/jieli/src/file_sizes.txt /tmp/temp");
	system("cp /users/jieli/src/inter_arrival_times.txt /tmp/temp");

	cout << "Genearating files..." << endl;
	GenerateFiles();

	vector<double> inter_arrival_times;
	ifstream infile;
	double time;
	double total_time = 0.0;
	infile.open("/tmp/temp/inter_arrival_times.txt");
	for (int i = 0; i < FILE_COUNT; i++) {
		infile >> time;
		inter_arrival_times.push_back(time);
		total_time += time;
		cout << "Interarrival time: " << time << endl;
	}
	infile.close();
	cout << "Average inter-arrival time: " << (total_time / FILE_COUNT) << " second" << endl;

	struct timespec time_spec;
	time_spec.tv_sec = 0;
	time_spec.tv_nsec = 0;

	CpuCycleCounter cpu_counter;
	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);
	double last_time_mark = 0.0;

	system("sudo sync && sudo echo 3 > /proc/sys/vm/drop_caches");
	char file_name[256];
	for (int i = BASE_NUM; i < BASE_NUM + FILE_COUNT; i++) {
		double curr_time = GetElapsedSeconds(cpu_counter);
		if (curr_time - last_time_mark < inter_arrival_times[i]) {
			time_spec.tv_nsec = (curr_time - last_time_mark) * 1000000000;
			cout << "Wait for " << time_spec.tv_nsec << " nanoseconds" << endl;
			nanosleep(&time_spec, NULL);
			last_time_mark = GetElapsedSeconds(cpu_counter);
		}

		sprintf(file_name, "/tmp/temp/temp%d.dat", i - BASE_NUM + 1);
		sender->SendFile(file_name);
	}
}

void ExperimentManager2::GenerateFiles() {
	static const int BUF_SIZE = 4096;

	ifstream infile ("/tmp/temp/file_sizes.txt");
	int total_size = 0;
	int num_files = 0;
	int size = 0;
	vector<int> file_sizes;
	while (infile >> size) {
		file_sizes.push_back(size);
		total_size += size;
		num_files++;
	}
	cout << "Average file size: " << (total_size / num_files) << " bytes" << endl;


	// Generate the files
	int file_index = 1;
	char file_name[50];
	char buf[BUF_SIZE];
	for (int i = BASE_NUM; i < BASE_NUM + FILE_COUNT; i++) {
		int remained_size = file_sizes[i]; //* 100;

		sprintf(file_name, "/tmp/temp/temp%d.dat", file_index++);
		ofstream outfile (file_name, ofstream::binary | ofstream::trunc);
		while (remained_size > 0) {
			int data_len = (remained_size > BUF_SIZE) ? BUF_SIZE : remained_size;
			outfile.write(buf, data_len);
			remained_size -= data_len;
		}
		outfile.close();
	}
}


void ExperimentManager2::HandleExpResults(string msg) {
	if (result_file.is_open() && finished_node_count < num_test_nodes) {
		finished_node_count++;
		if (finished_node_count == num_test_nodes)
			result_file.flush();
	}
}

