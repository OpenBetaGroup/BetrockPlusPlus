/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../memory.h"

#if defined(__linux__) || defined(__HAIKU__)
#include <features.h>
#include <fstream>
#include <ios>
#include <string>
#include <unistd.h>

double GetMemoryUsage(const MemoryUnit _unit) {
	std::ifstream statStream("/proc/self/stat", std::ios_base::in);
	if (!statStream)
		return 0.0;

	// dummy vars for leading entries in stat that we don't care about
	//
	std::string pid, comm, state, ppid, pgrp, session, ttyNr;
	std::string tpgid, flags, minflt, cminflt, majflt, cmajflt;
	std::string utime, stime, cutime, cstime, priority, nice;
	std::string o, itrealvalue, starttime;

	// the two fields we want
	//
	unsigned long vsize;
	long rss;

	statStream >> pid >> comm >> state >> ppid >> pgrp >> session >> ttyNr >> tpgid >> flags >> minflt >> cminflt >>
	    majflt >> cmajflt >> utime >> stime >> cutime >> cstime >> priority >> nice >> o >> itrealvalue >> starttime >>
	    vsize >> rss; // don't care about the rest

	statStream.close();

	long pageSizeBytes = sysconf(_SC_PAGE_SIZE);

	//vmUsage = static_cast<double>(vsize) / BytesPerUnit(_unit);
	return static_cast<double>(rss * pageSizeBytes) / BytesPerUnit(_unit);
}

// Get the current memory page size in Bytes (Cached after first use)
size_t GetPageSize() {
	static const size_t pageSize = []() -> size_t {
		const long size = sysconf(_SC_PAGESIZE);
		return size > 0 ? static_cast<size_t>(size) : 4096;
	}();
	return pageSize;
}

#if defined(__GLIBC__)

#include <malloc.h>
double GetActiveMemoryUsage(const MemoryUnit _unit) {
	struct mallinfo2 mi = mallinfo2();
	return static_cast<double>(mi.uordblks) / BytesPerUnit(_unit);
}

void TrimMemory() {
	malloc_trim(0);
}

#else // non-glibc

double GetActiveMemoryUsage(const MemoryUnit _unit) {
	return GetMemoryUsage(_unit);
}

void TrimMemory() {}

#endif
#endif
