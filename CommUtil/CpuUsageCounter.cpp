/*
 * CpuUsageCounter.cpp
 *
 *  Created on: Jan 7, 2012
 *      Author: jie
 */

#include "CpuUsageCounter.h"

CpuUsageCounter::CpuUsageCounter(int interval) : interval(interval) {
	keep_running = false;
}

CpuUsageCounter::~CpuUsageCounter() {
	// TODO Auto-generated destructor stub
}


void CpuUsageCounter::Start() {
	keep_running = true;
	pthread_create(&count_thread, NULL, &CpuUsageCounter::StartCountThread, this);
}


void CpuUsageCounter::Stop() {
	keep_running = false;
}


void* CpuUsageCounter::StartCountThread(void* ptr) {
	((CpuUsageCounter*)ptr)->RunCountThread();
	return NULL;
}


void CpuUsageCounter::RunCountThread() {
	ofstream output("cpu_usage.csv", ofstream::out | ofstream::trunc);
	output << "Measure Time (sec),System Time (sec),User Time (sec),CPU Usage (%)" << endl;

	CpuCycleCounter cpu_counter;
	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);

	struct rusage old_usage, new_usage;
	getrusage(RUSAGE_SELF, &old_usage);

	double interval_sec = interval / 1000.0;
	double measure_time = 0.0;
	double elapsed_sec;
	double cpu_time, user_time, sys_time;
	double usage_ratio;
	int usage_percent;
	while (keep_running) {
		while ((elapsed_sec = GetElapsedSeconds(cpu_counter)) < interval_sec)
			;
		measure_time += elapsed_sec;

		getrusage(RUSAGE_SELF, &new_usage);
		user_time = (new_usage.ru_utime.tv_sec - old_usage.ru_utime.tv_sec)
					+ (new_usage.ru_utime.tv_usec - old_usage.ru_utime.tv_usec) * 0.000001;
		sys_time = (new_usage.ru_stime.tv_sec - old_usage.ru_stime.tv_sec)
					+ (new_usage.ru_stime.tv_usec - old_usage.ru_stime.tv_usec) * 0.000001;
		cpu_time = user_time + sys_time;

		usage_ratio = cpu_time / elapsed_sec;
		usage_percent = usage_ratio * 100;
		if (usage_percent > 100)
			usage_percent = 100;

		output << measure_time << "," << sys_time << "," << user_time << "," << usage_percent << endl;

		old_usage = new_usage;
		AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);
	}

	output.close();
}
