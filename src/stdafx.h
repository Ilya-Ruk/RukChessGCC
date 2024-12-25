// stdafx.h

#pragma once

#ifndef STDAFX_H
#define STDAFX_H

#include <stdint.h>
#include <unistd.h>             // usleep()
#include <sys/time.h>           // struct timeval, gettimeofday()
#include <time.h>               // CLOCK_REALTIME, struct timespec, clock_gettime()
#include <pthread.h>            // pthread_t, pthread_create(), pthread_join(), pthread_exit()

//#include <windows.h>            // Master include file for Windows applications (includes additional header files; see below)

//#include <winnt.h>              // SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, GROUP_AFFINITY
//#include <sysinfoapi.h>         // GetLogicalProcessorInformationEx()
//#include <systemtopologyapi.h>  // GetNumaNodeProcessorMaskEx()
//#include <processtopologyapi.h> // SetThreadGroupAffinity()
//#include <winbase.h>            // SetThreadAffinityMask()
//#include <processthreadsapi.h>  // GetCurrentThread()
//#include <synchapi.h>           // Sleep()
//#include <profileapi.h>         // QueryPerformanceCounter(), QueryPerformanceFrequency()

#include <stdio.h>              // _IONBF, printf(), scanf(), fopen(), fseek(), ftell(), fclose(), fprintf(), fgets(), sprintf()
#include <stdlib.h>             // atoi(), strtoull(), malloc(), realloc(), calloc(), free(), qsort()
#include <string.h>             // strcmp(), strncmp(), strchr(), strstr(), strcpy()
//#include <sys/timeb.h>          // timeb, ftime()
//#include <process.h>            // _beginthread(), _endthread()
#include <omp.h>                // Open MP
//#include <intrin.h>             // __popcnt64(), _BitScanForward64(), _BitScanReverse64(), _mm_prefetch()
#include <immintrin.h>          // _pdep_u64(), _pext_u64()
#include <limits.h>             // INT_MAX
#include <float.h>              // FLT_MAX
#include <math.h>               // round(), pow(), log()
#include <assert.h>             // assert()

#endif // !STDAFX_H