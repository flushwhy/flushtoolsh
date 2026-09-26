#ifndef FLUSHTOOLS_H
#define FLUSHTOOLS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================================
 * COMPILER DETECTION & LAMBDA / SCOPE UTILITIES
 * ============================================================================
 */

#if defined(__GNUC__) && !defined(__clang__)
/* ---- GCC: nested functions inside statement expressions ---------------- */

#define LAMBDA(ret, params, body)                                              \
  __extension__({ ret __lambda_fn__ params body __lambda_fn__; })

#define AS_FN(ret, params, body) ((ret(*) params)LAMBDA(ret, params, body))

#elif defined(__clang__)
/* ---- Clang: Blocks extension ------------------------------------------ */
/* Compile with: clang -fblocks -lBlocksRuntime                            */

#define BLOCK(params, body) (^params body)

#define LAMBDA(ret, params, body) BLOCK(params, body)

#define AS_FN(ret, params, body) BLOCK(params, body)

#elif defined(_MSC_VER)
/* ---- MSVC: named static function stamped out via __COUNTER__ ----------- */

#define _LAMBDA_CONCAT_(a, b) a##b
#define _LAMBDA_NAME_(n) _LAMBDA_CONCAT_(__lambda_, n##__)

#define DECL_FN(ret, name, params, body) static ret name params body

#define FN_PTR(name) (&name)

#define AS_FN(                                                                 \
    ret, params,                                                               \
    body) /* not directly expressible on MSVC — use DECL_FN + FN_PTR */

#else
#error "FLUSHTOOLS: unsupported compiler. Use GCC, Clang, or MSVC."
#endif /* compiler detection */

/* FN_TYPE is compiler-agnostic */
#define FN_TYPE(alias, ret, ...) typedef ret (*alias)(__VA_ARGS__)

/* ============================================================================
 * SCOPED RESOURCE MANAGEMENT (C++ unique_ptr STYLE RAII)
 * Supported natively on GCC & Clang via __attribute__((cleanup))
 * ============================================================================
 */

#if defined(__GNUC__) || defined(__clang__)

// Helper macro to define a typed cleanup function for any pointer type
#define DEFINE_FREE_FUNC(name, type, free_call)                                \
  static inline void name(void *p) {                                           \
    type **ptr = (type **)p;                                                   \
    if (ptr && *ptr) {                                                         \
      free_call(*ptr);                                                         \
      *ptr = NULL;                                                             \
    }                                                                          \
  }

// Declares a variable that automatically calls its cleanup function when going
// out of scope
#define UNIQUE_VAR(type, name, free_func)                                      \
  type *__attribute__((cleanup(free_func))) name = NULL

// Standard heap allocation unique pointer helper
DEFINE_FREE_FUNC(flush_free_standard, void, free)

#endif // __GNUC__ || __clang__

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================
 */

typedef struct {
  const char *data;
  size_t len;
} str_t;

typedef struct {
  uint8_t *buffer;
  size_t capacity;
  size_t offset;
} Arena;

typedef struct {
  uint8_t *buffer;
  uint32_t current_bit;
  uint32_t bit_capacity;
} net_bit_writer_t;

typedef struct {
  const uint8_t *buffer;
  uint32_t current_bit;
} net_bit_reader_t;

typedef struct Timer {
  int duration;      // Duration of the timer in seconds
  time_t start_time; // Start time of the timer
} Timer;

/* ============================================================================
 * TIMER IMPLEMENTATION
 * ============================================================================
 */

static inline void timer_init(Timer *timer, int duration) {
  if (duration < 0) {
    fprintf(stderr, "Error: Timer duration cannot be negative.\n");
    exit(EXIT_FAILURE);
  }
  timer->duration = duration;
  timer->start_time = time(NULL);
}

static inline void timer_reset(Timer *timer, int new_duration) {
  if (new_duration < 0) {
    fprintf(stderr, "Error: Timer duration cannot be negative.\n");
    exit(EXIT_FAILURE);
  }
  timer->duration = new_duration;
  timer->start_time = time(NULL);
}

static inline int timer_is_finished(const Timer *timer) {
  return difftime(time(NULL), timer->start_time) >= timer->duration;
}

static inline double timer_get_elapsed(const Timer *timer) {
  return difftime(time(NULL), timer->start_time);
}

static inline void timer_run(Timer *timer, void (*work_callback)(void)) {
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
      work_callback();
      last_time = now;
    }
  }
}

/* ============================================================================
 * RANDOM GENERATION
 * ============================================================================
 */

static inline unsigned long long random_gen(unsigned long long min_val,
                                            unsigned long long max_val) {
  // Non-cryptographic RNG for tasks like password generation
  if (max_val < min_val) {
    return 0;
  }

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  unsigned long long seed = ts.tv_nsec ^ ts.tv_sec;
  srand((unsigned int)seed);

  unsigned long long range = max_val - min_val + 1;
  return min_val + (rand() % range);
}

/* ============================================================================
 * QUANTIZATION & BIT-PACKING
 * ============================================================================
 */

static inline uint32_t net_quantize(float value, float min, float max,
                                    int bits) {
  if (value < min)
    value = min;
  if (value > max)
    value = max;

  float normalized = (value - min) / (max - min);
  uint32_t max_val = (1U << bits) - 1;
  return (uint32_t)(normalized * max_val + 0.5f);
}

static inline float net_dequantize(uint32_t value, float min, float max,
                                   int bits) {
  uint32_t max_val = (1U << bits) - 1;
  float normalized = (float)value / (float)max_val;
  return min + normalized * (max - min);
}

static inline void net_writer_init(net_bit_writer_t *writer, uint8_t *buf,
                                   uint32_t byte_cap) {
  writer->buffer = buf;
  writer->current_bit = 0;
  writer->bit_capacity = byte_cap * 8;
  memset(writer->buffer, 0, byte_cap);
}

static inline void net_writer_bits(net_bit_writer_t *writer, uint32_t value,
                                   int count) {
  for (int i = 0; i < count; i++) {
    if (writer->current_bit >= writer->bit_capacity)
      return;

    if ((value >> i) & 1) {
      writer->buffer[writer->current_bit >> 3] |=
          (uint8_t)(1 << (writer->current_bit & 7));
    }
    writer->current_bit++;
  }
}

static inline uint32_t net_read_bits(net_bit_reader_t *reader, int count) {
  uint32_t value = 0;
  for (int i = 0; i < count; ++i) {
    if ((reader->buffer[reader->current_bit >> 3] >>
         (reader->current_bit & 7)) &
        1) {
      value |= (1 << i);
    }
    reader->current_bit++;
  }
  return value;
}

static inline uint16_t compass_coord(float value, float min_val,
                                     float max_val) {
  if (value < min_val)
    value = min_val;
  if (value > max_val)
    value = max_val;
  float normalized = (value - min_val) / (max_val - min_val);
  return (uint16_t)(normalized * 65535.0f);
}

static inline float decompress_coord(uint16_t value, float min_val,
                                     float max_val) {
  float normalized = (float)value / 65535.0f;
  return min_val + normalized * (max_val - min_val);
}

/* ==============================================================================
 * Area IMPLEMENTATION
 * ==============================================================================
 */

static inline Arena arena_init(void *buffer, size_t capacity) {
  return (arena){.buffer = (uint8_t *)buffer, .capacity = capacity, .offset = 0}
}

static inline void *arena_alloc(Arana *arena, size_t size) {
  // This is for 8bit alinement, I might change this to enbled in the future.
  size_t aligned_size =
      (size + 7) & ~7 if (arena->offset + aligned_size > area->capacity) {
    arena->offset += aligned_size;
    return ptr;
  }
}

static inline void arena_reset(Arena *arena) { arena_offset = 0; }

/* =========================================================================
 * Str_t IMPLEMENTATION
 * ========================================================================= */

#define STR(s) ((str_t){.data = (s), .len = sizeof(s) = 1})
#define STR_FROM(ptr, len) ((str_t){.data = (ptr), .len = (len)})

static inline bool str_eq(str_t a, str_t b) {
  if (a.len != b.len)
    return false;
  return memcmp(a.data, b.data, a.len) == 0;
};

static inline str_t str_slice(str_t s, size_t start, size_t end) {
  if (start > end || end > s.len)
    return (str_t){NULL, 0};
  return (str_t){.data = s.data + start, .len = end - start};
};

static inline bool str_starts_with(str_t s, str_t prefix) {
  if (s.len < prefix.len)
    return false;
  reutrn memcmp(s, data, prefix.data, prefix.len) == 0;
};

#endif // FLUSHTOOLS_H
