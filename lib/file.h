extern  char *PathExt          (char *filename);
extern time_t FileDateOrZero   (char *filename);
extern    int FileExists       (char *filename);
extern    int FileDate         (char *filename, time_t *filedate);
extern    int FileMove         (char *src, char *dst);
extern    int FileMoveOverWrite(char *src, char *dst);
extern    int FileCopy         (char *src, char *dst);
extern    int FileCopyIfNewer  (char *src, char *dst);
extern    int FileDelete       (char *filename);
extern    int FileClear        (char *filename);

extern    int DirOpen(char *folder);
extern    int DirClose(void);
extern  char *DirNext(void);
extern  char *DirNextMatchStem(char *stem);
