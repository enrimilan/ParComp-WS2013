#include "cilk_merge.h"
#include "sequential_merge.h"
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#define 	TASKS_PER_WORKER	(2) 


/** Prototypes */

void cilk_merge_part(int start, int end, int unit, data_t *A, int lenA, data_t *B, int lenB, data_t *result);


/** Implementations */

double cilk_merge(data_t *A, int lenA, data_t *B, int lenB, data_t *result) {
	int unit = (lenA+lenB) / __cilkrts_get_nworkers() * TASKS_PER_WORKER;
	
	double start = getTimestamp();
	cilk_merge_part(0, lenA+lenB, unit, A, lenA, B, lenB, result);
	double end = getTimestamp();

	return end-start;
	return 0;
}


void cilk_merge_part(int start, int end, int unit, data_t *A, int lenA, data_t *B, int lenB, data_t *result) {
	if (end-start <= unit) {
		int startA, endA, startB, endB;
		
		// Merge data
		coranks(start, A, lenA, &startA, B, lenB, &startB);
		coranks(end, A, lenA, &endA, B, lenB, &endB);
		(void) sequential_merge(A+startA, endA-startA, B+startB, endB-startB, result+start);
	}
	else {
		// Distribute work to other threads
		int middle = start + (end-start)/2;
		
		cilk_spawn cilk_merge_part(start, middle, unit, A, lenA, B, lenB, result);
		cilk_spawn cilk_merge_part(middle, end, unit, A, lenA, B, lenB, result);
	}
}