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
void ExperimentManager2::StartExperiment(SenderStatusProxy* sender_proxy, MVCTPSender* sender) {
	system("mkdir /tmp/temp");
	system("cp /users/jieli/src/file_sizes.txt /tmp/temp");
	system("cp /users/jieli/src/inter_arrival_times.txt /tmp/temp");

	// Randomly generate FILE_COUNT files for the sample
	vector<double> inter_arrival_times = GenerateFiles();

	struct timespec time_spec;
	time_spec.tv_sec = 0;
	time_spec.tv_nsec = 0;

	CpuCycleCounter cpu_counter;
	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);
	double last_time_mark = 0.0;

	system("sudo sync && sudo echo 3 > /proc/sys/vm/drop_caches");
	char file_name[256];
	for (int i = 0; i < FILE_COUNT; i++) {
		double curr_time = GetElapsedSeconds(cpu_counter);
		if (curr_time - last_time_mark < inter_arrival_times[i]) {
			time_spec.tv_nsec = (curr_time - last_time_mark) * 1000000000;
			cout << "Wait for " << time_spec.tv_nsec << " nanoseconds" << endl;
			nanosleep(&time_spec, NULL);
			last_time_mark = GetElapsedSeconds(cpu_counter);
		}

		sprintf(file_name, "/tmp/temp/temp%d.dat", i + 1);
		sender->SendFile(file_name);
	}
}


// Randomly select and generate files from a given sample
vector<double> ExperimentManager2::GenerateFiles() {
	static const int BUF_SIZE = 4096;

	cout << "Genearating files..." << endl;
	ifstream fs_file("/tmp/temp/file_sizes.txt");
	int size = 0;
	vector<int> file_sizes;
	while (fs_file >> size) {
		file_sizes.push_back(size);
	}

	// Randomly generate FILE_COUNT files from the sample
	srand(time(NULL));
	int file_index = 1;
	char file_name[50];
	char buf[BUF_SIZE];
	int total_size = 0;
	for (int i = 0; i < FILE_COUNT; i++) {
		int index = rand() % file_sizes.size();
		total_size += file_sizes[index];

		// Generate the file
		int remained_size = file_sizes[index]; //* 100;
		sprintf(file_name, "/tmp/temp/temp%d.dat", file_index++);
		ofstream outfile (file_name, ofstream::binary | ofstream::trunc);
		while (remained_size > 0) {
			int data_len = (remained_size > BUF_SIZE) ? BUF_SIZE : remained_size;
			outfile.write(buf, data_len);
			remained_size -= data_len;
		}
		outfile.close();
	}
	cout << "Average file size: " << (total_size / FILE_COUNT) << " bytes" << endl;


	// Randomly generate FILE_COUNT inter-arrival times
	ifstream irt_file("/tmp/temp/inter_arrival_times.txt");
	double time;
	vector<double> inter_arrival_times;
	while (irt_file >> time) {
		inter_arrival_times.push_back(time);
	}
	irt_file.close();

	vector<double> res;
	double total_time = 0.0;
	for (int i = 0; i < FILE_COUNT; i++) {
		int index = rand() % inter_arrival_times.size();
		res.push_back(inter_arrival_times[index]);
		total_time += inter_arrival_times[index];
	}
	cout << "Average inter-arrival time: " << (total_time / FILE_COUNT) << " second" << endl;
	return res;
}


void ExperimentManager2::HandleExpResults(string msg) {
	if (result_file.is_open() && finished_node_count < num_test_nodes) {
		finished_node_count++;
		if (finished_node_count == num_test_nodes)
			result_file.flush();
	}
}

