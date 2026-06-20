#ifndef FLUSHTOOLS_H
#define FLUSHTOOLS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>



#if defined(__GNUC__) && !defined(__clang__)
/* ---- GCC: nested functions inside statement expressions ---------------- */

#define LAMBDA(ret, params, body)      \
    __extension__                      \
    ({                                 \
        ret __lambda_fn__ params body  \
        __lambda_fn__;                 \
    })

#define AS_FN(ret, params, body) \
    ((ret (*)params) LAMBDA(ret, params, body))

#elif defined(__clang__)
/* ---- Clang: Blocks extension ------------------------------------------ */
/* Compile with: clang -fblocks -lBlocksRuntime                            */

#define BLOCK(params, body) \
    (^params body)

#define LAMBDA(ret, params, body) \
    BLOCK(params, body)

#define AS_FN(ret, params, body) \
    BLOCK(params, body)

#elif defined(_MSC_VER)
/* ---- MSVC: named static function stamped out via __COUNTER__ ----------- */
/*                                                                           */
/* __COUNTER__ expands to a unique integer each time it is used,            */
/* giving each LAMBDA call site its own function name (__lambda_0__,        */
/* __lambda_1__, ...) so multiple lambdas in the same scope don't clash.    */
/*                                                                           */
/* Limitation: no capture — MSVC C has no closure mechanism.               */

#define _LAMBDA_CONCAT_(a, b) a##b
#define _LAMBDA_NAME_(n)      _LAMBDA_CONCAT_(__lambda_, n##__)

#define LAMBDA(ret, params, body)             \
    ( (ret (*)params)(                        \
        (void)({                              \
            static ret _LAMBDA_NAME_(__COUNTER__) params body; \
            _LAMBDA_NAME_(__COUNTER__ - 1);   \
        })                                    \
    ) )

/*
 * MSVC __COUNTER__ approach above needs a small workaround because
 * compound-statement expressions ({}) aren't valid C in MSVC.
 * We use a helper macro that stamps a static function before the
 * expression that references it.
 *
 * Recommended MSVC usage — two-step via DECL_FN + FN_PTR:
 *
 *   DECL_FN(int, my_add, (int a, int b), { return a + b; });
 *   int (*add)(int, int) = FN_PTR(my_add);
 *
 * Or for one-liner callbacks, just write a named static function above
 * the call site and pass it directly — MSVC C has no anonymous functions.
 */
#undef LAMBDA   /* remove the broken compound-stmt version */


#define DECL_FN(ret, name, params, body) \
    static ret name params body


#define FN_PTR(name) (&name)

#define AS_FN(ret, params, body) /* not directly expressible on MSVC — use DECL_FN + FN_PTR */

#else
#  error "LAMBDA: unsupported compiler. Use GCC, Clang, or MSVC."
#endif /* compiler detection */

/* FN_TYPE is compiler-agnostic */
#define FN_TYPE(alias, ret, ...) \
    typedef ret (*alias)(__VA_ARGS__)

/* --- BIT-PACKING --- */

typedef struct {
    uint8_t* buffer;
    uint32_t current_bit;
    uint32_t bit_capacity;
} net_bit_writer_t;

typedef struct {
    const uint8_t* buffer;
    uint32_t current_bit;
} net_bit_reader_t;


 /*--- TIMER ---*/

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

/*-- QUANTIZATION ---*/

static inline uint32_t net_quantize(float value, float min, float max, int bits) {
    if (value < min) value = min;
    if (value > max) value = max;

    float normalized = (value - min) / (max - min);
    uint32_t max_val = (1U << bits) - 1;
    return (uint32_t)(normalized * max_val + 0.5f);
}

static inline float net_dequantize(uint32_t value, float min, float max, int bits) {
    uint32_t max_val = (1U << bits) - 1;
    float normalized = (float)value / (float)max_val;
    return min + normalized * (max - min);
}

static inline void net_writer_init(net_bit_writer_t* writer, uint8_t* buf, uint32_t byte_cap) {
    writer->buffer = buf;
    writer->current_bit = 0;
    writer->bit_capacity = byte_cap * 8;

    // clear mem buff
    memset(writer->buffer, 0, byte_cap);
}

static inline void net_writer_bits(net_bit_writer_t* writer, uint32_t value, int count){
    for (int i = 0; i < count; i++) {
        if (writer->current_bit >= writer->bit_capacity) return;

        if ((value >> i) & 1) {
            // Index: bit / 8 (shift 3), offset: bit % 8
            writer->buffer[writer->current_bit >> 3] |= (uint8_t)(1 << (writer->current_bit & 7));
        }
        writer->current_bit++;
    }
}

static inline uint32_t net_read_bits(net_bit_reader_t* reader, int count) {
    uint32_t value = 0;
    for (int i = 0; i < count; ++i){
        if ((reader->buffer[reader->current_bit >> 3] >> (reader->current_bit & 7)) & 1) {
            value |= (1 << i);
        }
       reader->current_bit++;
    }
   return value;
}

uint16_t compass_coord(float value, float min_val, float max_val) {
    if (value < min_val) value = min_val;
    if (value > max_val) value = max_val;
    float normalized = (value - min_val) / (max_val - min_val);
}

float decompress_coord(uint16_t value, float min_val, float max_val) {
    float normalized = (float)value / 65535.0f;
    return min_val + normalized * (max_val - min_val);
}

#endif // FLUSH_TOOLS_H
