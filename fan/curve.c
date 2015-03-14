#include <pthread.h> //rwlock
#include <stdio.h>   //fopen printf
#include <errno.h>   //errno
#include <string.h>  //strerror
#include <stdlib.h>  //free
#include <stdint.h> 
#include "../lib/log.h"
#include "../lib/cfg.h"

#define MAX_LINE_LENGTH 200
#define MAX_DENSITIES 20
#define MAX_AMBIENTS  20

static uint16_t Densities[MAX_DENSITIES];               //X
static float     Ambients[MAX_AMBIENTS];                //Ys
static float     Heatings[MAX_DENSITIES][MAX_AMBIENTS]; //X Y
static int  AmbientsCount;
static int DensitiesCount;

static uint16_t convertFloatToDensity(float density) { //Convert float to uint16 0 --> 1023

	if      (density < 0)    return 0;
	else if (density > 1023) return 1023;
	else                     return (uint16_t)density;
}

static int   getAmbientIndexA     (                  float ambient) {
	int ambientIndex = 0;
	while (ambientIndex + 1 < AmbientsCount && Ambients[ambientIndex + 1] < ambient) ambientIndex++; //Go through the list until the next entry is the last or has a larger value.
	return ambientIndex;
}
static float getAmbientRatioBtoA  (int ambientIndex, float ambient) {

	float ambientA = Ambients[ambientIndex + 0];
	float ambientB = Ambients[ambientIndex + 1];
	
	return (ambient - ambientA) / (ambientB - ambientA);
}

static int   getDensityIndexA     (                  uint16_t density) {
	int densityIndex = 0;
	while(densityIndex + 1 < DensitiesCount && Densities[densityIndex + 1] > density) densityIndex++; //Go through the list until the next entry is the last or has a smaller value.
	return densityIndex;
}
static float getDensityRatioBtoA  (int densityIndex, uint16_t density) {
	float densityA = (float)Densities[densityIndex + 0];
	float densityB = (float)Densities[densityIndex + 1];
	
	return ((float)density - densityA) / (densityB - densityA);
}

static float getHeatingFromDensity(int ambientIndex, uint16_t density) {

	if (DensitiesCount == 1)
	{
		return Heatings[0][ambientIndex];
	}
	else
	{
		int densityIndex = getDensityIndexA(density);
		
		float heatingA = Heatings[densityIndex + 0][ambientIndex];
		float heatingB = Heatings[densityIndex + 1][ambientIndex];
			
		float ratioBtoA = getDensityRatioBtoA(densityIndex, density);
		
		return heatingA + ratioBtoA * (heatingB - heatingA);
	}
}

static float getHeatingAcrossAmbients      (int densityIndex, float ambient) {
	if ( AmbientsCount == 1)
	{
		return Heatings[densityIndex][0];
	}
	else
	{
		int ambientIndex = getAmbientIndexA(ambient);

		float heatingA = Heatings[densityIndex][ambientIndex + 0];
		float heatingB = Heatings[densityIndex][ambientIndex + 1];
		
		float ratioBtoA = getAmbientRatioBtoA(ambientIndex, ambient);
		return heatingA + ratioBtoA * (heatingB - heatingA);
	}
}
static int   getDensityIndexAFromHeating   (                  float ambient, float heating) {
	int densityIndex = 0;
	while (densityIndex + 1 < DensitiesCount)
	{
		float nextHeating = getHeatingAcrossAmbients(densityIndex + 1, ambient);
		if (nextHeating > heating) break;
		densityIndex++ ;
	}
	return densityIndex;
}
static float getDensityRatioBtoAFromHeating(int densityIndex, float ambient, float heating) {
	float heatingA = getHeatingAcrossAmbients(densityIndex + 0, ambient);
	float heatingB = getHeatingAcrossAmbients(densityIndex + 1, ambient);
	
	if (heatingB - heatingA < 0.1) return 0.0;
	else                           return (heating - heatingA) / (heatingB - heatingA);
}

static void  addHeatingAtPoint(int ambientIndex, int densityIndex, float heatingToAdd) {
	Heatings[densityIndex][ambientIndex] += heatingToAdd;
	densityIndex++;
	while(densityIndex < DensitiesCount && Heatings[densityIndex][ambientIndex] - Heatings[densityIndex - 1][ambientIndex] < 0.1)
	{
		Heatings[densityIndex][ambientIndex] += heatingToAdd;
		densityIndex++;
	}
}
static void  subHeatingAtPoint(int ambientIndex, int densityIndex, float heatingToAdd) {
	Heatings[densityIndex][ambientIndex] += heatingToAdd;
	densityIndex--;
	while(densityIndex >= 0 && Heatings[densityIndex + 1][ambientIndex] - Heatings[densityIndex][ambientIndex] < 0.1)
	{
		Heatings[densityIndex][ambientIndex] += heatingToAdd;
		densityIndex--;
	}
}
static void  addHeatingToCurve(int ambientIndex, uint16_t density, float heatingToAdd) {
	
	if (DensitiesCount == 0) return;
	if (DensitiesCount == 1)
	{
		if (heatingToAdd > 0) addHeatingAtPoint(ambientIndex, 0, heatingToAdd);
		else                  subHeatingAtPoint(ambientIndex, 0, heatingToAdd);
	}
	else
	{
		int densityIndex = getDensityIndexA(density);
		float  ratioBtoA = getDensityRatioBtoA(densityIndex, density);
		
		float heatingB = ratioBtoA * heatingToAdd;
		float heatingA = heatingToAdd - heatingB;	
		
		if (heatingToAdd > 0)
		{
			addHeatingAtPoint(ambientIndex, densityIndex + 1, heatingB); //Add to the higher density first to give room for the lower density next
			addHeatingAtPoint(ambientIndex, densityIndex + 0, heatingA);
		}
		else
		{
			subHeatingAtPoint(ambientIndex, densityIndex + 0, heatingA); //Subtract from the lower density first to give room for the higher density next
			subHeatingAtPoint(ambientIndex, densityIndex + 1, heatingB);
		}
	}
}

static char* splitFieldReturnNextOrNull(char* p) {
	while (*p)
	{
		if (*p == '\t')
		{
			*p = 0;
			return ++p;
		}
		p++;
	}
	return NULL;
}
static int   loadHeader(char* pThis) {
	DensitiesCount = 0;
	 AmbientsCount = 0;
	int col = 0;
	while (pThis)
	{
		char * pNext = splitFieldReturnNextOrNull(pThis);
		if (col > 0)//ignore the first column of x values
		{
			if (AmbientsCount < MAX_AMBIENTS)
			{
				AmbientsCount++;
				Ambients[AmbientsCount - 1] = (float)strtod(pThis, NULL);
			}
		}
		pThis = pNext;
		col++;
	}
	return 0;
}
static int   loadRow   (char* pThis) {
	
	DensitiesCount++;
	int col = 0;
	while (pThis)
	{
		char * pNext = splitFieldReturnNextOrNull(pThis);
		if (col == 0) //First column of x values
		{
			Densities[DensitiesCount - 1] = (float)strtod(pThis, NULL);
		}
		else
		{
			Heatings[DensitiesCount - 1][col - 1] = (float)strtod(pThis, NULL);
		}
		pThis = pNext;
		col++;
	}
	return 0;
}
static int   loadLine  (char isLine, char* line) {
	if (isLine) return loadRow(line);
	else        return loadHeader(line); 
}
static int   loadLines (FILE* fp) {
	char isLine = 0;
	int i = 0;
	char  line[MAX_LINE_LENGTH];
	while (1)
	{
		int c = fgetc(fp);
		if (c == EOF)
		{
			if (feof(fp))
			{
				if (loadLine(isLine, line)) return -1;
				isLine = 1;
				break;
			}
			else
			{
				LogErrno("Error reading curve file");
				return -1;
			}
		}
		if (c == '\n')
		{
			if (loadLine(isLine, line)) return -1;
			isLine = 1;
			i = 0;
			continue;
		}
		if (c == '\r') { continue;}
		if (c ==  ' ') { continue;}
		if (i < MAX_LINE_LENGTH) line[i++] = (char)c; //i can never exceed MAX_LINE_LENGTH - 1 leaving room for a null
	}
	return 0;
}

static pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
static int acquireWriteLock() {
	int result = pthread_rwlock_wrlock(&rwlock);
	if (result != 0)
	{
		LogNumber("Error acquiring write lock on cfg", result);
		return -1;
	}
	return 0;
}
static int  acquireReadLock() {
	int result = pthread_rwlock_rdlock(&rwlock);
	if (result != 0)
	{
		LogNumber("Error acquiring read lock on cfg", result);
		return -1;
	}
	return 0;
}
static int      releaseLock() {
	int result = pthread_rwlock_unlock(&rwlock);
	if (result != 0)
	{
		LogNumber("Error releasing cfg lock", result);
		return -1;
	}
	return 0;
}

int CurveGetOutputFromHeating(float ambient, float    heating, uint16_t *pDensity) {
	if (acquireReadLock()) return -1;
		
	if ( AmbientsCount == 0) { releaseLock(); Log('e', "No ambient entries in fan items list"); return -1; }
	if (DensitiesCount == 0) { releaseLock(); Log('e', "No density entries in fan items list"); return -1; }
	if (DensitiesCount == 1)
	{
		*pDensity = Densities[0]; //If there is only one density then that is the answer irrespective of anything else
	}
	else
	{
		int densityIndex = getDensityIndexAFromHeating(ambient, heating);

		float densityA = (float)Densities[densityIndex + 0];
		float densityB = (float)Densities[densityIndex + 1];
		
		float ratioBtoA = getDensityRatioBtoAFromHeating(densityIndex, ambient, heating);
		float density = densityA + ratioBtoA * (densityB - densityA);
		*pDensity = convertFloatToDensity(density);
	}
	
	releaseLock();
	return 0;
}
int CurveGetHeatingFromOutput(float ambient, uint16_t density, float    *pHeating) {
	if (acquireReadLock()) return -1;
	
	if (DensitiesCount == 0) { releaseLock(); Log('e', "No density entries in fan items list"); return -1; }
	if ( AmbientsCount == 0) { releaseLock(); Log('e', "No ambient entries in fan items list"); return -1; }
	if ( AmbientsCount == 1)
	{
		*pHeating = getHeatingFromDensity(0, density);
	}
	else
	{
		int ambientIndex = getAmbientIndexA(ambient);

		float heatingA = getHeatingFromDensity(ambientIndex + 0, density);
		float heatingB = getHeatingFromDensity(ambientIndex + 1, density);
		
		float ratioBtoA = getAmbientRatioBtoA(ambientIndex, ambient);

		*pHeating = heatingA + ratioBtoA * (heatingB - heatingA);
	}
	
	releaseLock();
	return 0;
}

int CurveAddHeatingAtOutput  (float ambient, uint16_t density, float heatingToAdd) {
	
	if (acquireWriteLock()) return -1;
	
	if (DensitiesCount == 0) { releaseLock(); Log('e', "No density entries in fan items list"); return -1; }
	if ( AmbientsCount == 0) { releaseLock(); Log('e', "No ambient entries in fan items list"); return -1; }
	if ( AmbientsCount == 1)
	{
		addHeatingToCurve(0, density, heatingToAdd);
	}
	else
	{
		int ambientIndex = getAmbientIndexA(ambient);
		float ratioBtoA = getAmbientRatioBtoA(ambientIndex, ambient);
	
		float heatingB = ratioBtoA * heatingToAdd;
		float heatingA = heatingToAdd - heatingB;	
		
		addHeatingToCurve(ambientIndex + 0, density, heatingA);
		addHeatingToCurve(ambientIndex + 1, density, heatingB);
	}

	releaseLock();
	return 0;
}
int CurveGetMaxHeating       (float ambient, float *pMaxHeating, float *pMinHeating) {
	
	if (acquireReadLock()) return -1;
	
	*pMaxHeating = 0.0;
	*pMinHeating = 999.999;

	if (DensitiesCount == 0) { releaseLock(); Log('e', "No density entries in fan items list"); return -1; }
	if ( AmbientsCount == 0) { releaseLock(); Log('e', "No ambient entries in fan items list"); return -1; }
	if ( AmbientsCount == 1)
	{
		*pMaxHeating = Heatings[DensitiesCount - 1][0];
		*pMinHeating = Heatings[0][0];
	}
	else
	{
		int ambientIndex = getAmbientIndexA(ambient);
		float ratioBtoA = getAmbientRatioBtoA(ambientIndex, ambient);
	
		float maxHeatingA = Heatings[DensitiesCount - 1][ambientIndex + 0];
		float maxHeatingB = Heatings[DensitiesCount - 1][ambientIndex + 1];
		float minHeatingA = Heatings[0][ambientIndex + 0];
		float minHeatingB = Heatings[0][ambientIndex + 1];
				
		*pMaxHeating = maxHeatingA + ratioBtoA * (maxHeatingB - maxHeatingA);
		*pMinHeating = minHeatingA + ratioBtoA * (minHeatingB - minHeatingA);
	}

	releaseLock();
	return 0;
}

int CurveLoad() {
	CfgLock();
		char *filename = strdupa(CfgFanCurveFileName);
	CfgUnlock();
	
	if (acquireWriteLock()) return -1;
	
	FILE *fp = fopen(filename, "r");
	if (fp == NULL)
	{
		LogErrno("Error opening fan curve file for reading");
		releaseLock();
		return -1;
	}

	if (loadLines(fp))
	{
		releaseLock();
		fclose(fp);
		return -1;
	}
		
	fclose(fp);
	
	if (releaseLock()) return -1;
	return 0;
}
int CurveSave() {
	CfgLock();
		char *filename = strdupa(CfgFanCurveFileName);
	CfgUnlock();
	
	if (acquireReadLock()) return -1;
	
	FILE *fp = fopen(filename, "w");
	if (fp == NULL)
	{
		LogErrno("Error opening fan curve file for writing");
		releaseLock();
		return -1;
	}
	
	//Specify the columns
	for (int y = 0; y < AmbientsCount; y++)
	{
		if (fprintf(fp,"\t%5.2f", Ambients[y]) < 0)
		{
			LogErrno("Error writing to fan curve file");
			fclose(fp);
			releaseLock();
			return -1;
		}
	}
	
	//Add the data points
	if (DensitiesCount > 0)
	{
		if (fprintf(fp,"\n") < 0)
		{
			LogErrno("Error writing to fan curve file");
			fclose(fp);
			releaseLock();
			return -1;
		}
	}
	for (int x = 0; x < DensitiesCount; x++)
	{
		if (fprintf(fp,"%4u", Densities[x]) < 0)
		{
			LogErrno("Error writing to fan curve file");
			fclose(fp);
			releaseLock();
			return -1;
		}
		for (int y = 0; y < AmbientsCount; y++)
		{
			if (fprintf(fp,"\t%5.2f", Heatings[x][y]) < 0)
			{
				LogErrno("Error writing to fan curve file");
				fclose(fp);
				releaseLock();
				return -1;
			}
		}
		if (x < DensitiesCount - 1) 
		{
			if (fprintf(fp,"\n") < 0)
			{
				LogErrno("Error writing to fan curve file");
				fclose(fp);
				releaseLock();
				return -1;
			}
		}
	}

	fclose(fp);	
	
	if (releaseLock()) return -1;
	return 0;
}

void CurveAddPointsForHtmlChart(char **pp) {
	if (acquireReadLock())
	{
		*pp = stpcpy(*pp, "ERROR obtaining fan curve point lock");
		return;
	}

	//Specify the columns
	*pp = stpcpy(*pp, "['Density', ");
	for (int y = 0; y < AmbientsCount; y++)
	{
		int length = sprintf(*pp,"'%5.2f'", Ambients[y]);
		if (length < 0)
		{
			*pp = stpcpy(*pp, "ERROR writing fan curve column");
			releaseLock();
			return;
		}
		*pp += length;
		if (y < AmbientsCount - 1) *pp = stpcpy(*pp, ",");
	}
	*pp = stpcpy(*pp, "]");
	
	//Add the data points
	if (DensitiesCount > 0) *pp = stpcpy(*pp, ",\n");
	for (int x = 0; x < DensitiesCount; x++)
	{
		int length = sprintf(*pp, "[%4u", Densities[x]);
		if (length < 0)
		{
			*pp = stpcpy(*pp, "ERROR writing fan curve point");
			releaseLock();
			return;
		}
		*pp += length;
		
		for (int y = 0; y < AmbientsCount; y++)
		{
			int length = sprintf(*pp, ",%5.2f", Heatings[x][y]);
			if (length < 0)
			{
				*pp = stpcpy(*pp, "ERROR writing fan curve point");
				releaseLock();
				return;
			}
			*pp += length;
		}

		*pp = stpcpy(*pp, "]");
		
		
		if (x < DensitiesCount - 1) *pp = stpcpy(*pp, ",\n");
	}
	releaseLock();
	
}