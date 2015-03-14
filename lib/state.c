#include <stdio.h> //fopen
#include <stdarg.h> //va_list, va_start, va_end, vfprintf
#include <errno.h> //errno and ENOENT
#include "../lib/log.h"

int StateWrite(char *filename, const char *format, ...) { //Returns 0 on success or -1 on error
	FILE *fp = fopen(filename, "w");
	if (fp == 0)
	{
		LogErrno("StateWrite - fopen");
		return -1;
	}
	
	va_list args;
	va_start (args, format);
	int length = vfprintf(fp, format, args);
	va_end (args);
	fclose(fp);
	
	if (length < 0)
	{
		LogErrno("StateWrite - vfprintf");
		return -1;
	}
	return 0;
}
int StateRead(char *filename, const char *format, ...) { //Returns the number of conversions made or 0 if no such file or -1 on error
	FILE *fp = fopen(filename, "r");
	if (fp == 0)
	{
		if (errno == ENOENT) return 0; //No such file or directory so return 0
		LogErrno("StateRead - fopen");
		return -1;
	}
	
	va_list args;
	va_start (args, format);
	int count = vfscanf(fp, format, args);
	va_end (args);
	fclose(fp);
	
	if (count == EOF)
	{
		LogErrno("StateRead - vfscanf");
		return -1;
	}
	return count;
}
