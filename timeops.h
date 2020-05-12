#ifndef _TIMEOPS_H_
#define _TIMEOPS_H_
unsigned long get_current_time(void) {
	struct timeval te; 
	gettimeofday(&te, NULL); // get current time
	long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds
	return milliseconds;
}
#endif
