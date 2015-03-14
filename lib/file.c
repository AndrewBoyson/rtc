#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>   //strncmp, strlen
#include <time.h>     //time_t
#include <unistd.h>   //sleep
#include <dirent.h>   //opendir, readdeir
#include <sys/stat.h>
#include <fcntl.h>

#define BUF_SIZE 1024

#include "log.h"

char *PathExt(char *filename) {
	char *ext = filename;
	while (*ext != 0 && *ext != '.') ext++;
	if (*ext == '.') ext++;
	return ext;
}
time_t FileDateOrZero (char *filename) {
	struct stat filestat;
	if (stat(filename, &filestat)) return 0;
	return filestat.st_mtime;
}
int    FileExists(char *filename) {
	return FileDateOrZero(filename);
}
int    FileDate(char *filename, time_t *filedate) {
	struct stat filestat;
	if (stat(filename, &filestat))
	{
		LogErrno("Could not get last modified date");
		return -1;
	}
	*filedate = filestat.st_mtime;
	return 0;
}
int    FileMove(char *src, char *dst) {
	if (rename(src, dst))
	{
		LogErrno("ERROR renaming file");
		return -1;
	}
	return 0;
}
int    FileCopy(char *src, char *dst) {
	//Log('d', "%s -> %s", src, dst);
    char buf[BUF_SIZE];
 
    // Open input and output files
    int inputFd = open(src, O_RDONLY);
    if (inputFd == -1)
	{
		LogErrno("ERROR opening copy file source");
        return -1;
	}
    int openFlags = O_CREAT | O_WRONLY | O_TRUNC;
    mode_t filePerms = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;      /* rw-rw-rw- */
    int outputFd = open(dst, openFlags, filePerms);
    if (outputFd == -1)
	{
		Log('w', "Copy failed, trying again after mounting drives");
		system("mount -a"); //Try mounting the drive and see if that solves the problem
		sleep(2);
		outputFd = open(dst, openFlags, filePerms);
		if (outputFd == -1)
		{
			LogErrno("ERROR opening copy file destination ");
			close(inputFd);
			return -1;
		}
	}
	
    // Transfer data until we encounter end of input or an error
    while (1)
	{
		int numRead = read(inputFd, buf, BUF_SIZE);
		if (numRead == 0) break; //EOF
		if (numRead == -1)
		{
			LogErrno("ERROR reading source file");
			close( inputFd);
			close(outputFd);
			return -1;
		}
		
		int numWritten = write(outputFd, buf, numRead);
		if (numWritten != numRead)
		{
			LogErrno("ERROR writing file");
			close( inputFd);
			close(outputFd);
			return -1;
		}
	}
    if (close(inputFd))
	{
		LogErrno("ERROR closing input file");
		close(outputFd);
        return -1;
	}
    if (close(outputFd))
 	{
		LogErrno("ERROR closing output file");
		close(inputFd);
        return -1;
	}
	return 0;
}
int    FileCopyIfNewer(char *src, char *dst) {
	if (FileDateOrZero(src) > FileDateOrZero(dst)) if (FileCopy(src, dst)) return -1;
	return 0;
}
int    FileDelete(char *filename) {
	if (unlink(filename))
	{
		LogErrno("ERROR deleting file");
		return -1;
	}
	return 0;
}
int    FileClear(char *filename) {
    // Open file to clear
    int openFlags = O_CREAT | O_WRONLY | O_TRUNC;
    mode_t filePerms = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;      /* rw-rw-rw- */
    int outputFd = open(filename, openFlags, filePerms);
    if (outputFd == -1)
	{
		Log('w', "Clear failed, trying again after mounting drives");
		system("mount -a"); //Try mounting the drive and see if that solves the problem
		sleep(2);
		outputFd = open(filename, openFlags, filePerms);
		if (outputFd == -1)
		{
			LogErrno("ERROR opening clear file");
			return -1;
		}
	}
	//Close file
    if (close(outputFd))
 	{
		LogErrno("ERROR closing clear file");
        return -1;
	}
	return 0;
}
int    FileMoveOverWrite(char *src, char *dst) {
	if (FileExists(dst)) if (FileDelete(dst)) return -1;
	return FileMove(src, dst);
}


static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static DIR *d; //d is static and could be overwritten so we protect it with a mutex
int   DirOpen(char *folder) {
	//Acquire lock
	int result;
	result = pthread_mutex_lock(&mutex);
	if (result != 0)
	{
		Log('e', "problem acquiring dir lock", strerror(result));
		return -1;
	}
	
	//Open folder
	d = opendir(folder);
	if (d == NULL)
	{
		LogErrno("Could not open current directory");
		pthread_mutex_unlock(&mutex);
		return -1;
	}
	return 0;
}
int   DirClose(void) {
	if (closedir(d))
	{
		LogErrno("Could not close current directory");
		pthread_mutex_unlock(&mutex);
		return -1;
	}
	//Release the lock
	int result = pthread_mutex_unlock(&mutex);
	if (result != 0)
	{
		Log('e', "problem releasing dir lock", strerror(result));
		return -1;
	}	
	return(0);
}
char *DirNext(void) {
	struct dirent *dir; //Pointer to an entry allocated by readdir. this is not reentrant and could be overwritten.
	dir = readdir(d);
	return dir == NULL ? NULL : dir->d_name;
}
char *DirNextMatchStem(char *stem) {
	int n = strlen(stem);
	while (1)
	{
		char *filename = DirNext();
		if (filename == NULL) return NULL;
		if (strncmp(stem, filename, n) == 0) return filename;
	}
}
