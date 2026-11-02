
#include <stdio.h>
#include "worker.h"

int worker_execute(
    const char *filename,
    WorkerTask task,
    DatasetStats *result
) {
    printf("[Worker %d] Processing records %zu to %zu\n",
           task.worker_id,
           task.start_record,
           task.end_record);

    return dataset_analyze_range(
        filename,
        task.start_record,
        task.end_record,
        result
    );
}
