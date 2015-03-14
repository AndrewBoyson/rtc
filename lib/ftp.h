#define FTP_NOT_CONNECTED 0
#define FTP_CONNECTED     1
#define FTP_LOGGED_IN     2

int    FtpLogin      (char *host, int *pSck);
void   FtpLogout     (int sck, int state);
int    FtpCwd        (int sck, char *path);
time_t FtpGetFileTime(int sck, char *file);
int    FtpDownload   (int sck, char *file, char *cache);