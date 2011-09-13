/*
 * mvctp.cpp
 *
 *  Created on: Jun 30, 2011
 *      Author: jie
 */
#include "mvctp.h"

double MVCTP::CPU_MHZ = 0;
FILE*  MVCTP::log_file = NULL;
bool MVCTP::is_log_enabled;
struct CpuCycleCounter MVCTP::start_time_counter;

// Must be called before starting MVCTP activities
void MVCTPInit() {
	MVCTP::CPU_MHZ = GetCPUMhz();
	AccessCPUCounter(&MVCTP::start_time_counter.hi, &MVCTP::start_time_counter.lo);
	MVCTP::log_file = NULL;
	MVCTP::is_log_enabled = false;
}

void Log(char* format, ...) {
	if (!MVCTP::is_log_enabled)
		return;

	if (MVCTP::log_file == NULL) {
		MVCTP::log_file = fopen("mvctp_run.log", "w");
	}

	va_list args;
	va_start (args, format);
	vfprintf (MVCTP::log_file, format, args);
	//fflush(log_file);
	va_end (args);
}

void CreateNewLogFile(const char* file_name) {
	if (MVCTP::log_file != NULL) {
		fclose(MVCTP::log_file);
	}

	MVCTP::log_file = fopen(file_name, "w");
}


void SysError(string s) {
	perror(s.c_str());
	exit(-1);
}

void AccessCPUCounter(unsigned *hi, unsigned *lo) {
	asm("rdtsc; movl %%edx, %0; movl %%eax, %1"
			: "=r" (*hi), "=r" (*lo)
			:
			: "%edx", "%eax");
}


double GetElapsedCycles(unsigned cycle_hi, unsigned cycle_lo) {
	unsigned ncycle_hi, ncycle_lo;
	unsigned hi, lo, borrow;
	double result;

	AccessCPUCounter(&ncycle_hi, &ncycle_lo);

	lo = ncycle_lo - cycle_lo;
	borrow = lo > ncycle_lo;
	hi = ncycle_hi - cycle_hi - borrow;
	result = (double) hi * (1 << 30) * 4 + lo;
	if (result < 0) {
		SysError("GetElapsedCycles(): counter returns negative value");
	}
	return result;
}


double GetCPUMhz() {
	double rate;
	unsigned hi, lo;
	AccessCPUCounter(&hi, &lo);
	sleep(1);
	rate = GetElapsedCycles(hi, lo) / 1 / 1000000;
	return rate;
}

double GetCurrentTime() {
	return GetElapsedCycles(MVCTP::start_time_counter.hi, MVCTP::start_time_counter.lo) / 1000000.0 / MVCTP::CPU_MHZ;
}

double GetElapsedSeconds(CpuCycleCounter lastCount) {
	return GetElapsedCycles(lastCount.hi, lastCount.lo) / 1000000.0 / MVCTP::CPU_MHZ;
}


