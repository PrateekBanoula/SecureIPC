
#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <semaphore.h>
#include <stddef.h>

#include "dataset.h"

#define SHARED_TASK_LIMIT 3

typedef struct {
    int task_id;
    int worker_id;
    int success;
    DatasetStats stats;
} SharedResult;

typedef struct {
    sem_t mutex;
    SharedResult results[SHARED_TASK_LIMIT];
} SharedData;

SharedData *shared_memory_create(
    char *name,
    size_t name_size,
    int *shm_fd
);

int shared_memory_store(
    SharedData *shared,
    int task_id,
    int worker_id,
    int success,
    const DatasetStats *stats
);

int shared_memory_read(
    SharedData *shared,
    int task_id,
    SharedResult *result
);

void shared_memory_destroy(
    SharedData *shared,
    int shm_fd,
    const char *name
);

#endif
