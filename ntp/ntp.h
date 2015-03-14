#include <stdint.h>

extern  int NtpBind(void);
extern  int NtpSendTime(void);
extern  int NtpUnbind(void);

extern  int NtpConnect(char *);
extern  int NtpGetTime(int64_t *, int64_t *);
extern  int NtpClose(void);
