#include <pthread.h>
#include <semaphore.h>

#include "thread.h"
#include "log.h"

static int setPriority(pthread_t thread, int priority) {
	struct sched_param sp;
	sp.sched_priority = priority;
	int type = priority ? SCHED_FIFO : SCHED_OTHER;
	int r = pthread_setschedparam(thread, type, &sp);
	if (r)
	{
		LogNumber("Error setting priority", r);
		return -1;
	}
	return 0;
}


void ThreadCancel       (struct Thread *t) {
	Log('i', "%s %s %s","Stopping", t->Name, "worker thread");
	int r = pthread_cancel(t->Pthread);
	if (r)
	{
		LogNumber("Error killing worker thread", r);
		return;
	}

}
int  ThreadStart        (struct Thread *t) {	
	Log('i', "Creating worker thread %s", t->Name);
    int r = pthread_create(&t->Pthread, NULL, t->Worker, NULL);
	if (r)
    {
        LogNumber("ThreadStart - error creating thread", r);
        return -1;
    }
	return 0;
}
int  ThreadWorkerInit   (struct Thread *t) {
	int r = pthread_setname_np(t->Pthread, t->Name);
	if (r)
	{
		LogNumber("Error setting thread name", r);
		return -1;
	}
	
	if (ThreadSetNormalPriority(t)) return -1;
	
	Log('i', "Starting thread");
	r = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	if (r)
	{
		LogNumber("Error setting cancel type to asynchronous", r);
		return -1;
	}
	
	return 0;
}
int  ThreadSetNormalPriority  (struct Thread *t) {
	if (setPriority(t->Pthread, t->NormalPriority)) return -1;
	return 0;
}
int  ThreadSetCriticalPriority(struct Thread *t) {
	if (setPriority(t->Pthread, t->CriticalPriority)) return -1;
	return 0;
}
int  ThreadCancelDisable(struct Thread *t) {
	int r = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	if (r != 0)
	{
		LogNumber("Error enabling cancel", r);
		return -1;
	}
	return 0;
}
int  ThreadCancelEnable (struct Thread *t) {
	int r = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	if (r)
	{
		LogNumber("Error disabling cancel", r);
		return -1;
	}
	return 0;
}
int  ThreadJoin         (struct Thread *t) {
	int r = pthread_join(t->Pthread, NULL);
	if (r)
	{
		LogNumber("Error joining thread", r);
		return -1;
	}
	return 0;
}