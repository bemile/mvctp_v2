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

static const int FILE_COUNT = 100;
void ExperimentManager2::StartExperiment(SenderStatusProxy* sender_proxy, MVCTPSender* sender) {
	// Randomly generate FILE_COUNT files for the sample
	sender_proxy->SendMessageLocal(INFORMATIONAL, "Generating files...\n");
	system("mkdir /tmp/temp");
	system("cp /users/jieli/src/file_sizes.txt /tmp/temp");
	system("cp /users/jieli/src/inter_arrival_times.txt /tmp/temp");
	File_Sample sample = GenerateFiles();
	system("sudo sync && sudo echo 3 > /proc/sys/vm/drop_caches");
	sender_proxy->SendMessageLocal(INFORMATIONAL, "Files generated.\n");


	sender_proxy->SendMessageLocal(INFORMATIONAL, "Sending files...\n");
	struct timespec time_spec;
	time_spec.tv_sec = 0;
	time_spec.tv_nsec = 0;

	CpuCycleCounter cpu_counter;
	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);
	double last_time_mark = 0.0;
	double sent_time = 0.0;
	double curr_time = 0.0;

	char file_name[256];
	int file_id = 0;
	char str[500];
	for (int i = 0; i < FILE_COUNT; i++) {
		if (i % 100 == 0) {
			sprintf(str, "Sending file %d", i);
			sender_proxy->SendMessageLocal(INFORMATIONAL, str);
		}

		sent_time += sample.inter_arrival_times[i];
		curr_time = GetElapsedSeconds(cpu_counter);
		double time_diff = sent_time - curr_time; // - last_time_mark;
		if (time_diff > 0) {
			if (time_diff > 1.0) {
				time_spec.tv_sec = (int)time_diff;
				time_spec.tv_nsec = (time_diff - time_spec.tv_sec) * 1000000000;
			}
			else {
				time_spec.tv_sec = 0;
				time_spec.tv_nsec = time_diff * 1000000000;
			}

			cout << "Wait for " << time_diff << " seconds" << endl;
			nanosleep(&time_spec, NULL);
		}

		sprintf(file_name, "/tmp/temp/temp%d.dat", i + 1);
		file_id = sender->SendFile(file_name);
	}

	while (!sender->IsTransferFinished(file_id)) {
		usleep(2000);
	}

	double transfer_time = GetElapsedSeconds(cpu_counter);
	double pho = sample.total_file_size * 8 / sample.total_time / (sender->GetSendRate() * 1000000.0);
	double throughput = sample.total_file_size * 8 / 1000000.0 / transfer_time;
	sprintf(str, "Experiment Finished.\n\n***** Statistics *****\nTotal No. Files: %d\nTotal File Size: %d bytes\n"
			"Total Arrival Time Span: %.2f second\nPho Value: %.2f\nTotal Transfer Time: %.2f seconds\n"
			"Throughput: %.2f Mbps\n*****End of Statistics *****\n\n",
			FILE_COUNT, sample.total_file_size, sample.total_time, pho, transfer_time, throughput);
	sender_proxy->SendMessageLocal(INFORMATIONAL, str);
}


// Randomly select and generate files from a given sample
File_Sample ExperimentManager2::GenerateFiles() {
	static const int BUF_SIZE = 4096;
	File_Sample sample;

	cout << "Genearating files..." << endl;
	ifstream fs_file("/tmp/temp/file_sizes.txt");
	int size = 0;
	vector<int> file_sizes;
	while (fs_file >> size) {
		file_sizes.push_back(size);
	}
	fs_file.close();

	// Randomly generate FILE_COUNT files from the sample
	srand(time(NULL));
	int file_index = 1;
	char file_name[50];
	char buf[BUF_SIZE];
	int total_size = 0;
	for (int i = 0; i < FILE_COUNT; i++) {
		int index = rand() % file_sizes.size();
		sample.file_sizes.push_back(file_sizes[index]);
		sample.total_file_size += file_sizes[index];
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

	double total_time = 0.0;
	int count = 0;
	while (count < FILE_COUNT) {
		int index = rand() % inter_arrival_times.size();
		if (inter_arrival_times[index] > 1.0)
			continue;

		sample.inter_arrival_times.push_back(inter_arrival_times[index]);
		sample.total_time += inter_arrival_times[index];
		total_time += inter_arrival_times[index];
		count++;
	}
	cout << "Average inter-arrival time: " << (total_time / FILE_COUNT) << " second" << endl;
	return sample;
}



static const int NUM_EXPERIMENTS = 2;
void ExperimentManager2::StartExperiment2(SenderStatusProxy* sender_proxy, MVCTPSender* sender) {
	static const int BUF_SIZE = 4096;

	//sender->SetSendRate(600);
	system("mkdir /tmp/temp");
	system("cp /users/jieli/src/file_sizes.txt /tmp/temp");
	system("cp /users/jieli/src/inter_arrival_times.txt /tmp/temp");

	// read in file sizes and inter-arrival-times
	ifstream fs_file("/tmp/temp/file_sizes.txt");
	int size = 0;
	vector<int> file_sizes;
	while (fs_file >> size) {
		if (size > 20000000)
			size = 20000000;

		file_sizes.push_back(size);
	}
	fs_file.close();

	ifstream irt_file("/tmp/temp/inter_arrival_times.txt");
	double time;
	vector<double> inter_arrival_times;
	while (irt_file >> time) {
		inter_arrival_times.push_back(time);
	}
	irt_file.close();


	// Run the experiments for NUM_EXPERIMENTS times
	result_file.open("exp_results.csv");
	result_file << "#Node ID, Throughput (Mbps), Robustness (%), Avg. CPU Usage (%), Slow Node (True or False)" << endl;
	for (int n = 0; n < NUM_EXPERIMENTS; n++) {
		sender_proxy->SendMessageLocal(INFORMATIONAL, "Generating files...\n");
		// Generate files
		int file_index = 1;
		char file_name[256];
		char buf[BUF_SIZE];
		File_Sample sample;
		for (int i = 0; i < FILE_COUNT; i++) {
			int index = n * FILE_COUNT + i;
			sample.file_sizes.push_back(file_sizes[index]);
			sample.total_file_size += file_sizes[index];
			sample.inter_arrival_times.push_back(inter_arrival_times[index]);
			sample.total_time += inter_arrival_times[index];

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


		// Start sending files
		sender_proxy->SendMessageLocal(INFORMATIONAL, "Sending files...\n");
		system("sudo sync && sudo echo 3 > /proc/sys/vm/drop_caches");
		sender->ResetAllReceiverStats();

		struct timespec time_spec;
		time_spec.tv_sec = 0;
		time_spec.tv_nsec = 0;

		CpuCycleCounter cpu_counter;
		AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);
		double last_time_mark = 0.0;
		double sent_time = 0.0;
		double curr_time = 0.0;

		int file_id = 0;
		char str[500];
		for (int i = 0; i < FILE_COUNT; i++) {
			if (i % 50 == 0) {
				sprintf(str, "Sending file %d", i);
				sender_proxy->SendMessageLocal(INFORMATIONAL, str);
			}

			sent_time += sample.inter_arrival_times[i];
			curr_time = GetElapsedSeconds(cpu_counter);
			double time_diff = sent_time - curr_time; // - last_time_mark;
			if (time_diff > 0) {
				if (time_diff > 1.0) {
					time_spec.tv_sec = (int)time_diff;
					time_spec.tv_nsec = (time_diff - time_spec.tv_sec) * 1000000000;
				}
				else {
					time_spec.tv_sec = 0;
					time_spec.tv_nsec = time_diff * 1000000000;
				}

				cout << "Wait for " << time_diff << " seconds" << endl;
				nanosleep(&time_spec, NULL);
			}

			sprintf(file_name, "/tmp/temp/temp%d.dat", i + 1);
			file_id = sender->SendFile(file_name, 10000);
		}

		while (!sender->IsTransferFinished(file_id)) {
			usleep(2000);
		}

		sender->CollectExpResults();

		double transfer_time = GetElapsedSeconds(cpu_counter);
		double pho = sample.total_file_size / sample.total_time / (sender->GetSendRate() * 1000000.0) * 8;
		double throughput = sample.total_file_size / 1000000.0 / transfer_time * 8;
		sprintf(str, "Experiment Finished.\n\n***** Statistics *****\nTotal No. Files: %d\nTotal File Size: %d bytes\n"
				"Total Arrival Time Span: %.2f second\nPho Value: %.2f\nTotal Transfer Time: %.2f seconds\n"
				"Throughput: %.2f Mbps\n*****End of Statistics *****\n\n",
				FILE_COUNT, sample.total_file_size, sample.total_time, pho, transfer_time, throughput);
		sender_proxy->SendMessageLocal(INFORMATIONAL, str);
	}

	result_file.close();
}



void ExperimentManager2::HandleExpResults(string msg) {
	if (result_file.is_open()) {
		result_file << msg << endl;
		result_file.flush();
	}
}

