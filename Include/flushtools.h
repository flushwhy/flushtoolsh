#ifndef FLUSHTOOLS_H
#define FLUSHTOOLS_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>


typedef struct Timer {
    int duration;            // Duration of the timer in seconds
    time_t start_time;       // Start time of the timer
} Timer;

// Initializes a Timer with a specified duration
void timer_init(Timer* timer, int duration) {
    if (duration < 0) {
        fprintf(stderr, "Error: Timer duration cannot be negative.\n");
        exit(EXIT_FAILURE);
    }
    timer->duration = duration;
    timer->start_time = time(NULL);
}

// Resets the timer with a new duration
void timer_reset(Timer* timer, int new_duration) {
    if (new_duration < 0) {
        fprintf(stderr, "Error: Timer duration cannot be negative.\n");
        exit(EXIT_FAILURE);
    }
    timer->duration = new_duration;
    timer->start_time = time(NULL);
}

// Checks if the timer has finished
int timer_is_finished(const Timer* timer) {
    return difftime(time(NULL), timer->start_time) >= timer->duration;
}

// Gets the elapsed time in seconds
double timer_get_elapsed(const Timer* timer) {
    return difftime(time(NULL), timer->start_time);
}

// Runs the timer and executes a callback function periodically
void timer_run(Timer* timer, void (*work_callback)(void)) {
    if (work_callback == NULL) {
        fprintf(stderr, "Error: work_callback cannot be NULL.\n");
        exit(EXIT_FAILURE);
    }

    time_t last_time = timer->start_time;
    while (!timer_is_finished(timer)) {
        time_t now = time(NULL);
        double elapsed = difftime(now, last_time);

        if (elapsed >= 1) {
            printf("Timer: %.0f seconds passed\n", difftime(now, timer->start_time));

            // Call the callback function
            work_callback();

            // Update last time
            last_time = now;
        }
    }
}

unsigned long long random_gen(unsigned long long min_val, unsigned long long max_val) {
    // This is a random number generator I made and use for non critical tasks like Password generation.
    // It is not cryptographically secure and should not be used for security purposes.

    if (max_val < min_val) {
        return 0; // Handle invalid range gracefully
    }

    // Seed the random number generator with a high-resolution timestamp
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    unsigned long long seed = ts.tv_nsec ^ ts.tv_sec;
    srand((unsigned int)seed);

    // Generate a random number within the range
    unsigned long long range = max_val - min_val + 1;
    return min_val + (rand() % range);
}


#endif // TIMER_H
