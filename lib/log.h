char LogIsAtLevel(char type);
void LogP(char type, const char *t1, const char *t2);
void LogPN(char type, char *t1, int n1, char *t2, int n2);
void Log(char type, const char *fmt, ...);
void LogErrno(char *);
void LogNumber(char *, int);