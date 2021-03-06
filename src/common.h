#ifndef COMM_H
#define COMM_H


/** Type definitions */

typedef long data_t;	// Type of data to be merged


/** Prototypes */

int parseArguments(int argc, char** argv);
double getTimestamp(void);
void coranks(int index, data_t *A, int lenA, int *corankA, data_t *B, int lenB, int *corankB);

#endif