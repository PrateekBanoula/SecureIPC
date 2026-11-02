
#ifndef WORKER_H
#define WORKER_H

#include <stddef.h>
#include "dataset.h"

typedef enum {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED,
    RETRYING
} TaskStatus;

typedef struct {
    int task_id;
    int worker_id;
    size_t start_record;
    size_t end_record;
    TaskStatus status;
    int retry_count;
} WorkerTask;

int worker_execute(
    const char *filename,
    WorkerTask task,
    DatasetStats *result
);

#endif
