
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

#include "dataset.h"

int dataset_analyze_range(
    const char *filename,
    size_t start_record,
    size_t end_record,
    DatasetStats *stats
) {
    if (!filename || !stats ||
        start_record == 0 ||
        end_record < start_record) {
        return -1;
    }

    FILE *file = fopen(filename, "r");

    if (!file) {
        perror("Dataset open failed");
        return -1;
    }

    char *line = NULL;
    size_t capacity = 0;
    ssize_t length;

    int header_found = 0;
    size_t record_number = 0;
    int failed = 0;

    stats->count = 0;
    stats->sum = 0;
    stats->minimum = 0;
    stats->maximum = 0;

    while ((length = getline(&line, &capacity, file)) != -1) {

        /* Skip empty lines. */
        if (strspn(line, " \t\r\n") == (size_t)length)
            continue;

        /* Skip the CSV header. */
        if (!header_found) {
            header_found = 1;
            continue;
        }

        record_number++;

        if (record_number < start_record)
            continue;

        if (record_number > end_record)
            break;

        int id, marks, age, salary;
        int consumed = 0;

        int fields = sscanf(
            line,
            " %d , %d , %d , %d %n",
            &id,
            &marks,
            &age,
            &salary,
            &consumed
        );

        if (fields != 4 || consumed == 0) {
            fprintf(stderr,
                    "[Dataset] Invalid record: %zu\n",
                    record_number);
            failed = 1;
            break;
        }

        /* Reject unexpected trailing characters. */
        for (size_t i = (size_t)consumed; line[i] != '\0'; i++) {
            if (!isspace((unsigned char)line[i])) {
                fprintf(stderr,
                        "[Dataset] Invalid data at record: %zu\n",
                        record_number);
                failed = 1;
                break;
            }
        }

        if (failed)
            break;

        if (stats->count == 0) {
            stats->minimum = marks;
            stats->maximum = marks;
        } else {
            if (marks < stats->minimum)
                stats->minimum = marks;

            if (marks > stats->maximum)
                stats->maximum = marks;
        }

        stats->sum += marks;
        stats->count++;
    }

    free(line);
    fclose(file);

    if (failed || !header_found || stats->count == 0)
        return -1;

    return 0;
}
