struct Thread {
	char *		Name;
	pthread_t	Pthread;
	int         NormalPriority;
	int         CriticalPriority;
	void *(*Worker)(void *arg);
};
extern void ThreadCancel             (struct Thread *);
extern int  ThreadStart              (struct Thread *);
extern int  ThreadWorkerInit         (struct Thread *);
extern int  ThreadSetNormalPriority  (struct Thread *);
extern int  ThreadSetCriticalPriority(struct Thread *);
extern int  ThreadCancelDisable      (struct Thread *);
extern int  ThreadCancelEnable       (struct Thread *);
extern int  ThreadJoin               (struct Thread *);