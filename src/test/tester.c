#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <float.h>
#include <string.h>
#include "tester.h"


/** Constants */

// How many times shall we repeat a testcase
#define		REPEAT_TIMES	(100)

// Name of the directory where output files will be placed
#define		OUTPUT_DIR		"../output/"

// Name of the log containing failed test cases			
#define		ERROR_LOG		"error.log"


/** Type definitions */

typedef struct {
	char name[25];
	
	data_t *A;	
	data_t *B;

	TestSize refSize;
	TestSize actualSize;
	
} Testcase;

typedef struct {
	int times;
	double t_min;
	double t_avg;

} ExecutionStatistic;

typedef struct {
	data_t* merged;
	ExecutionStatistic stats;

} Result;


/** Prototypes */
static void executeSingleTest(Testcase test);
static Result executeMerge(Testcase test, Implementation *impl);
static Testcase generateTestcaseFromDefinition(TestcaseDefinition def, TestSize size);
static bool verifyResult(data_t* result, data_t* reference, int len);
static void updateExecutionStats(ExecutionStatistic *stats, double execTime);
static double calcSpeedup(double t_par, double t_seq);
static TestSize calcActualSize(TestSize refSize);

static data_t* allocArray(int len);
static data_t* fillArray(data_t start, data_t inc, int len);
static void freeArray(data_t *arr);
static FILE* openLog(char* filename);
static void openErrorLog(void);
static void openStatLog(char* testName);
static void cleanup(void);

static void printErrorStats(void);
static void printExecutionStats(ExecutionStatistic implStats, ExecutionStatistic refStats, TestSize refSize);


/** Global variables */

static Implementation *parImpl = NULL;
static int threads = 0;		
static FILE *errorLog = NULL;		
static FILE *statLog = NULL;
static int failedTests = 0;


/** Implementations */

void executeTestcases(Implementation *_parImpl, int _threads) {	
	parImpl = _parImpl;
	threads = _threads;
	openErrorLog();

	// Execute test cases
	for (int testNo=0; testNo<numberOfTests; testNo++) {
		openStatLog(testcases[testNo].name);
		
		for (int sizeNo=0; sizeNo<numberOfSizes; sizeNo++) {
			Testcase test = generateTestcaseFromDefinition(testcases[testNo], sizes[sizeNo]);
			executeSingleTest(test);
		}
	}

	printErrorStats();
	cleanup();
}


void executeSlave(void (*slaveFunc)(int, int), int _threads) {
	threads = _threads;

	for (int testNo=0; testNo<numberOfTests; testNo++) {
		for (int sizeNo=0; sizeNo<numberOfSizes; sizeNo++) {
			for (int i=0; i<REPEAT_TIMES; i++) {
				TestSize actualSize = calcActualSize(sizes[sizeNo]);
				slaveFunc(actualSize, actualSize);
			}
		}
	}
}


static void executeSingleTest(Testcase test) {
	// Execute parallel and reference implementations
	Result refResult = executeMerge(test, &refImpl);
	Result implResult = executeMerge(test, parImpl);
	printExecutionStats(implResult.stats, refResult.stats, test.refSize);

	// Verify that parallel implementation merged correctly
	if (!verifyResult(implResult.merged, refResult.merged, 2*test.actualSize)) {
		failedTests++;
		fprintf(errorLog, "%s [%d threads]: Testcase '%s' (%d) failed!\n",
			parImpl->name, threads, test.name, test.refSize);
	}

	// Free memory
	freeArray(test.A);
	freeArray(test.B);
	freeArray(refResult.merged);
	freeArray(implResult.merged);
}


static Result executeMerge(Testcase test, Implementation *impl) {
	data_t* merged = allocArray(2*test.actualSize);
	ExecutionStatistic stats = {};
	stats.t_min = DBL_MAX;
	
	for (int i=0; i<REPEAT_TIMES; i++) {
		double execTime = impl->func(test.A, test.actualSize, test.B, test.actualSize, merged);
		updateExecutionStats(&stats, execTime);
	}

	Result res = {merged, stats};
	return res;
}


static Testcase generateTestcaseFromDefinition(TestcaseDefinition def, TestSize size) {
	Testcase test;
	test.refSize = size;
	test.actualSize = calcActualSize(size);
	strncpy(test.name, def.name, strlen(def.name)+1);
	
	test.A = fillArray(def.A_start, def.A_inc, size);
	test.B = fillArray(def.B_start, def.B_inc, size);

	return test;
}


static bool verifyResult(data_t* result, data_t* reference, int len) {
	for (int i=0; i<len; i++) {
		if (result[i] != reference[i])
			return false;
	}

	return true;
}


static void updateExecutionStats(ExecutionStatistic *stats, double execTime) {
	if (stats->t_min > execTime)
		stats->t_min = execTime;

	stats->t_avg = (stats->t_avg*stats->times + execTime) / (stats->times+1);
	stats->times++;
}


static double calcSpeedup(double t_par, double t_seq) {
	double speedup = 0;

	if (t_par != 0)
		speedup = t_seq / t_par;

	return speedup;
}


static TestSize calcActualSize(TestSize refSize) {
	return refSize / threads * threads;
}


static data_t* allocArray(int len) {
	if (len <= 0)
		return NULL;
	
	return malloc(sizeof(data_t)*len);
}


static data_t* fillArray(data_t start, data_t inc, int len) {
	if (inc <= 0)
		return NULL;
		
	data_t *arr = allocArray(len);

	// Fill array
	for (int i=0; i<len; i++)
		arr[i] = start + inc*i;

	return arr;
}


static void freeArray(data_t *arr) {
	if (arr != NULL)
		free(arr);
}


static FILE* openLog(char* filename) {
	FILE *log = fopen(filename, "a");
	if (log == NULL)
		fprintf(stderr, "Could not open log '%s'", filename);

	return log;
}


static void openErrorLog(void) {
	char filename[100];
	sprintf(filename, "%s%s", OUTPUT_DIR, ERROR_LOG);
	
	errorLog = openLog(filename);
}


static void openStatLog(char* testName) {
	char filename[100];
	sprintf(filename, "%s%s_%s.dat", OUTPUT_DIR, parImpl->name, testName);
	
	statLog = openLog(filename);
}


static void cleanup(void) {
	if (errorLog != NULL)
		fclose(errorLog);

	if (statLog != NULL)
		fclose(statLog);
}


/** Printing functions */

static void printErrorStats(void) {
	int passedTests = numberOfTests*numberOfSizes - failedTests;
	
	printf("%s [%d worker%s]  ::  %d test%s passed, %d test%s failed!\n",
		parImpl->name, threads, (threads == 1) ? " " : "s",
		passedTests, (passedTests == 1) ? "" : "s",
		failedTests, (failedTests == 1) ? "" : "s");
}


static void printExecutionStats(ExecutionStatistic implStats, ExecutionStatistic refStats, TestSize refSize) {
	double min_speedup = calcSpeedup(implStats.t_min, refStats.t_min);
	double avg_speedup = calcSpeedup(implStats.t_avg, refStats.t_avg);
		
	fprintf(statLog, "%d %d %f %f %f %f\n",
		threads, refSize, implStats.t_min, min_speedup, implStats.t_avg, avg_speedup);
}