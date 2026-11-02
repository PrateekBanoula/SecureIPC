
#ifndef DATASET_H
#define DATASET_H

#include <stddef.h>

typedef struct {
    long count;
    long long sum;
    int minimum;
    int maximum;
} DatasetStats;

int dataset_analyze_range(
    const char *filename,
    size_t start_record,
    size_t end_record,
    DatasetStats *stats
);

#endif
