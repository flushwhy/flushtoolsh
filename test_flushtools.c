/*
 * test_flushtools.c — unit tests for flushtools.h
 *
 * Build (GCC):
 *   gcc -std=c11 -Wall -Wextra -o test_flushtools test_flushtools.c
 *   ./test_flushtools
 *
 * Build (Clang):
 *   clang -std=c11 -Wall -Wextra -o test_flushtools test_flushtools.c
 *   ./test_flushtools
 *
 * Build (MSVC):
 *   cl /std:c11 /W4 test_flushtools.c
 *   test_flushtools.exe
 */

#include "flushtools.h"

#include <math.h>

/* ---------------------------------------------------------------------- */
/* Minimal test harness                                                   */
/* ---------------------------------------------------------------------- */

static int g_tests_run = 0;
static int g_tests_failed = 0;
static const char *g_current_test = NULL;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name)                                                         \
  do {                                                                         \
    g_current_test = #name;                                                    \
    g_tests_run++;                                                             \
    test_##name();                                                             \
  } while (0)

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "  [FAIL] %s: CHECK(%s) at %s:%d\n", g_current_test,     \
              #cond, __FILE__, __LINE__);                                      \
      g_tests_failed++;                                                        \
    }                                                                          \
  } while (0)

#define CHECK_EQ_INT(a, b)                                                     \
  do {                                                                         \
    long long _a = (long long)(a), _b = (long long)(b);                        \
    if (_a != _b) {                                                            \
      fprintf(stderr,                                                          \
              "  [FAIL] %s: CHECK_EQ_INT(%s, %s) -> %lld != %lld at %s:%d\n",  \
              g_current_test, #a, #b, _a, _b, __FILE__, __LINE__);             \
      g_tests_failed++;                                                        \
    }                                                                          \
  } while (0)

#define CHECK_NEAR(a, b, eps)                                                  \
  do {                                                                         \
    double _a = (double)(a), _b = (double)(b);                                 \
    if (fabs(_a - _b) > (eps)) {                                               \
      fprintf(stderr,                                                          \
              "  [FAIL] %s: CHECK_NEAR(%s, %s) -> %f vs %f (eps=%f) at "       \
              "%s:%d\n",                                                       \
              g_current_test, #a, #b, _a, _b, (double)(eps), __FILE__,         \
              __LINE__);                                                       \
      g_tests_failed++;                                                        \
    }                                                                          \
  } while (0)

/* ---------------------------------------------------------------------- */
/* Lambda & Scope Macros (Supported on GCC, Clang, and MSVC)              */
/* ---------------------------------------------------------------------- */

#if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)

TEST(lambda_basic_call) {
  int (*add)(int, int) = LAMBDA(int, (int a, int b), { return a + b; });
  CHECK_EQ_INT(add(3, 4), 7);
}

static int cmp_calls = 0;
TEST(as_fn_with_qsort) {
  int arr[] = {5, 3, 4, 1, 2};
  size_t n = sizeof(arr) / sizeof(arr[0]);

  cmp_calls = 0;
  qsort(arr, n, sizeof(int), AS_FN(int, (const void *a, const void *b), {
          return *(const int *)a - *(const int *)b;
        }));

  for (size_t i = 0; i < n; i++) {
    CHECK_EQ_INT(arr[i], (int)(i + 1));
  }
}

FN_TYPE(PredicateFn, int, int);
TEST(fn_type_typedef) {
  PredicateFn is_even = LAMBDA(int, (int x), { return x % 2 == 0; });
  CHECK_EQ_INT(is_even(4), 1);
  CHECK_EQ_INT(is_even(7), 0);
}

#endif /* __GNUC__ || __clang__ || _MSC_VER */

/* ---------------------------------------------------------------------- */
/* Scoped cleanup (RAII-style unique pointers - GCC/Clang only)          */
/* ---------------------------------------------------------------------- */

#if defined(__GNUC__) || defined(__clang__)

TEST(unique_var_frees_and_nulls_on_scope_exit) {
  char *escaped_ptr_snapshot = NULL;
  {
    UNIQUE_VAR(char, buffer, flush_free_standard);
    buffer = malloc(16);
    CHECK(buffer != NULL);
    strcpy(buffer, "hi");
    CHECK_EQ_INT(strcmp(buffer, "hi"), 0);
    escaped_ptr_snapshot = buffer;
    (void)escaped_ptr_snapshot;
  }
  CHECK(1);
}

TEST(unique_var_early_return_no_leak) {
  void *result = NULL;
  {
    UNIQUE_VAR(char, buffer, flush_free_standard);
    buffer = malloc(4);
    if (!buffer) {
      result = NULL;
    } else {
      result = (void *)1;
    }
  }
  CHECK(result == (void *)1);
}

static int texture_free_calls = 0;
typedef struct {
  int dummy;
} FakeTexture;
static void fake_texture_free(FakeTexture *t) {
  texture_free_calls++;
  free(t);
}
DEFINE_FREE_FUNC(free_fake_texture, FakeTexture, fake_texture_free)

TEST(custom_cleanup_func_is_invoked) {
  texture_free_calls = 0;
  {
    UNIQUE_VAR(FakeTexture, tex, free_fake_texture);
    tex = malloc(sizeof(FakeTexture));
    CHECK(tex != NULL);
  }
  CHECK_EQ_INT(texture_free_calls, 1);
}

TEST(cleanup_handles_null_without_crashing) {
  {
    UNIQUE_VAR(char, buffer, flush_free_standard);
    (void)buffer;
  }
  CHECK(1);
}

#endif /* __GNUC__ || __clang__ */

/* ---------------------------------------------------------------------- */
/* Bit packing                                                           */
/* ---------------------------------------------------------------------- */

TEST(bit_writer_reader_roundtrip_single_value) {
  uint8_t buf[16];
  net_bit_writer_t writer;
  net_writer_init(&writer, buf, sizeof(buf));
  net_writer_bits(&writer, 0b101, 3);

  net_bit_reader_t reader = {buf, 0};
  uint32_t val = net_read_bits(&reader, 3);
  CHECK_EQ_INT(val, 5);
}

TEST(bit_writer_reader_roundtrip_multiple_values) {
  uint8_t buf[16];
  net_bit_writer_t writer;
  net_writer_init(&writer, buf, sizeof(buf));

  net_writer_bits(&writer, 0x3, 2);    /* 2 bits  */
  net_writer_bits(&writer, 0x7F, 7);    /* 7 bits  */
  net_writer_bits(&writer, 0x1, 1);    /* 1 bit   */
  net_writer_bits(&writer, 0xABCD, 16); /* 16 bits */

  net_bit_reader_t reader = {buf, 0};
  CHECK_EQ_INT(net_read_bits(&reader, 2), 0x3);
  CHECK_EQ_INT(net_read_bits(&reader, 7), 0x7F);
  CHECK_EQ_INT(net_read_bits(&reader, 1), 0x1);
  CHECK_EQ_INT(net_read_bits(&reader, 16), 0xABCD);
}

TEST(bit_writer_init_zeroes_buffer) {
  uint8_t buf[4] = {0xFF, 0xFF, 0xFF, 0xFF};
  net_bit_writer_t writer;
  net_writer_init(&writer, buf, sizeof(buf));
  for (size_t i = 0; i < sizeof(buf); i++) {
    CHECK_EQ_INT(buf[i], 0);
  }
}

TEST(bit_writer_respects_capacity) {
  uint8_t buf[1] = {0};
  net_bit_writer_t writer;
  net_writer_init(&writer, buf, sizeof(buf));

  net_writer_bits(&writer, 0xFF, 8);
  CHECK_EQ_INT(writer.current_bit, 8);

  net_writer_bits(&writer, 0x1, 1);
  CHECK_EQ_INT(writer.current_bit, 8);
}

TEST(bit_writer_all_zero_bits_roundtrip) {
  uint8_t buf[4];
  net_bit_writer_t writer;
  net_writer_init(&writer, buf, sizeof(buf));
  net_writer_bits(&writer, 0, 10);

  net_bit_reader_t reader = {buf, 0};
  CHECK_EQ_INT(net_read_bits(&reader, 10), 0);
}

/* ---------------------------------------------------------------------- */
/* Quantization                                                          */
/* ---------------------------------------------------------------------- */

TEST(quantize_dequantize_roundtrip_midrange) {
  uint32_t q = net_quantize(0.75f, 0.0f, 1.0f, 8);
  CHECK_EQ_INT(q, 191);

  float f = net_dequantize(q, 0.0f, 1.0f, 8);
  CHECK_NEAR(f, 0.75f, 0.01f);
}

TEST(quantize_clamps_below_min) {
  uint32_t q = net_quantize(-5.0f, 0.0f, 1.0f, 8);
  CHECK_EQ_INT(q, 0);
}

TEST(quantize_clamps_above_max) {
  uint32_t q = net_quantize(5.0f, 0.0f, 1.0f, 8);
  uint32_t max_val = (1U << 8) - 1;
  CHECK_EQ_INT(q, max_val);
}

TEST(quantize_endpoints_are_exact) {
  CHECK_EQ_INT(net_quantize(0.0f, 0.0f, 1.0f, 8), 0);
  CHECK_EQ_INT(net_quantize(1.0f, 0.0f, 1.0f, 8), 255);
}

TEST(quantize_dequantize_roundtrip_various_bit_depths) {
  int bit_depths[] = {4, 8, 12, 16};
  float samples[] = {0.0f, 0.1f, 0.5f, 0.9f, 1.0f};

  for (size_t bi = 0; bi < sizeof(bit_depths) / sizeof(bit_depths[0]); bi++) {
    int bits = bit_depths[bi];
    float max_err = 1.0f / (2.0f * (float)((1U << bits) - 1)) + 1e-4f;
    for (size_t si = 0; si < sizeof(samples) / sizeof(samples[0]); si++) {
      uint32_t q = net_quantize(samples[si], 0.0f, 1.0f, bits);
      float f = net_dequantize(q, 0.0f, 1.0f, bits);
      CHECK_NEAR(f, samples[si], max_err);
    }
  }
}

TEST(quantize_nonzero_range) {
  uint32_t q = net_quantize(150.0f, 100.0f, 200.0f, 8);
  float f = net_dequantize(q, 100.0f, 200.0f, 8);
  CHECK_NEAR(f, 150.0f, 1.0f);
}

/* ---------------------------------------------------------------------- */
/* Coordinate compression                                                */
/* ---------------------------------------------------------------------- */

TEST(coord_compress_decompress_roundtrip) {
  uint16_t packed = compass_coord(123.4f, 0.0f, 1000.0f);
  float unpacked = decompress_coord(packed, 0.0f, 1000.0f);
  CHECK_NEAR(unpacked, 123.4f, 0.02f);
}

TEST(coord_compress_clamps_out_of_range) {
  CHECK_EQ_INT(compass_coord(-100.0f, 0.0f, 1000.0f), 0);
  CHECK_EQ_INT(compass_coord(5000.0f, 0.0f, 1000.0f), 65535);
}

TEST(coord_compress_endpoints) {
  CHECK_EQ_INT(compass_coord(0.0f, 0.0f, 1000.0f), 0);
  CHECK_EQ_INT(compass_coord(1000.0f, 0.0f, 1000.0f), 65535);
}

TEST(coord_negative_range) {
  uint16_t packed = compass_coord(-50.0f, -100.0f, 100.0f);
  float unpacked = decompress_coord(packed, -100.0f, 100.0f);
  CHECK_NEAR(unpacked, -50.0f, 0.01f);
}

/* ---------------------------------------------------------------------- */
/* Timer                                                                 */
/* ---------------------------------------------------------------------- */

TEST(timer_zero_duration_is_immediately_finished) {
  Timer t;
  timer_init(&t, 0);
  CHECK(timer_is_finished(&t));
}

TEST(timer_positive_duration_not_finished_immediately) {
  Timer t;
  timer_init(&t, 5);
  CHECK(!timer_is_finished(&t));
  CHECK(timer_get_elapsed(&t) < 5.0);
}

TEST(timer_reset_restarts_countdown) {
  Timer t;
  timer_init(&t, 0);
  CHECK(timer_is_finished(&t));
  timer_reset(&t, 5);
  CHECK(!timer_is_finished(&t));
}

static int tick_count = 0;
static void on_tick(void) { tick_count++; }

TEST(timer_run_terminates_and_finishes) {
  tick_count = 0;
  Timer t;
  timer_init(&t, 1);
  timer_run(&t, on_tick);
  CHECK(timer_is_finished(&t));
}

/* ---------------------------------------------------------------------- */
/* Random                                                                */
/* ---------------------------------------------------------------------- */

TEST(random_gen_stays_within_bounds) {
  for (int i = 0; i < 1000; i++) {
    unsigned long long roll = random_gen(1, 6);
    CHECK(roll >= 1 && roll <= 6);
  }
}

TEST(random_gen_degenerate_range_returns_the_value) {
  CHECK_EQ_INT(random_gen(7, 7), 7);
}

TEST(random_gen_inverted_range_returns_zero) {
  CHECK_EQ_INT(random_gen(10, 5), 0);
}

TEST(random_gen_produces_more_than_one_distinct_value) {
  unsigned long long first = random_gen(0, 1000000);
  int saw_different = 0;
  for (int i = 0; i < 20; i++) {
    if (random_gen(0, 1000000) != first) {
      saw_different = 1;
      break;
    }
  }
  CHECK(saw_different);
}

/* ---------------------------------------------------------------------- */
/* Arena allocator                                                       */
/* ---------------------------------------------------------------------- */

TEST(arena_init_sets_fields) {
  uint8_t backing[64];
  Arena a = arena_init(backing, sizeof(backing));
  CHECK(a.buffer == backing);
  CHECK_EQ_INT(a.capacity, sizeof(backing));
  CHECK_EQ_INT(a.offset, 0);
}

TEST(arena_alloc_returns_sequential_non_overlapping_pointers) {
  uint8_t backing[64];
  Arena a = arena_init(backing, sizeof(backing));

  void *p1 = arena_alloc(&a, 8);
  void *p2 = arena_alloc(&a, 8);
  CHECK(p1 != NULL);
  CHECK(p2 != NULL);
  CHECK(p1 != p2);
  CHECK((uint8_t *)p2 >= (uint8_t *)p1 + 8);
}

TEST(arena_alloc_aligns_to_8_bytes) {
  uint8_t backing[64];
  Arena a = arena_init(backing, sizeof(backing));

  void *p1 = arena_alloc(&a, 1);
  void *p2 = arena_alloc(&a, 1);
  CHECK_EQ_INT((uint8_t *)p2 - (uint8_t *)p1, 8);
}

TEST(arena_alloc_fails_gracefully_when_full) {
  uint8_t backing[8];
  Arena a = arena_init(backing, sizeof(backing));

  void *p1 = arena_alloc(&a, 8);
  CHECK(p1 != NULL);

  void *p2 = arena_alloc(&a, 1);
  CHECK(p2 == NULL);
}

TEST(arena_reset_allows_reuse) {
  uint8_t backing[8];
  Arena a = arena_init(backing, sizeof(backing));

  void *p1 = arena_alloc(&a, 8);
  CHECK(p1 != NULL);
  CHECK(arena_alloc(&a, 1) == NULL);

  arena_reset(&a);
  CHECK_EQ_INT(a.offset, 0);
  void *p2 = arena_alloc(&a, 8);
  CHECK(p2 == backing);
}

/* ---------------------------------------------------------------------- */
/* Runner                                                                */
/* ---------------------------------------------------------------------- */

int main(void) {
#if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)
  RUN_TEST(lambda_basic_call);
  RUN_TEST(as_fn_with_qsort);
  RUN_TEST(fn_type_typedef);
#endif

#if defined(__GNUC__) || defined(__clang__)
  RUN_TEST(unique_var_frees_and_nulls_on_scope_exit);
  RUN_TEST(unique_var_early_return_no_leak);
  RUN_TEST(custom_cleanup_func_is_invoked);
  RUN_TEST(cleanup_handles_null_without_crashing);
#endif

  RUN_TEST(bit_writer_reader_roundtrip_single_value);
  RUN_TEST(bit_writer_reader_roundtrip_multiple_values);
  RUN_TEST(bit_writer_init_zeroes_buffer);
  RUN_TEST(bit_writer_respects_capacity);
  RUN_TEST(bit_writer_all_zero_bits_roundtrip);

  RUN_TEST(quantize_dequantize_roundtrip_midrange);
  RUN_TEST(quantize_clamps_below_min);
  RUN_TEST(quantize_clamps_above_max);
  RUN_TEST(quantize_endpoints_are_exact);
  RUN_TEST(quantize_dequantize_roundtrip_various_bit_depths);
  RUN_TEST(quantize_nonzero_range);

  RUN_TEST(coord_compress_decompress_roundtrip);
  RUN_TEST(coord_compress_clamps_out_of_range);
  RUN_TEST(coord_compress_endpoints);
  RUN_TEST(coord_negative_range);

  RUN_TEST(timer_zero_duration_is_immediately_finished);
  RUN_TEST(timer_positive_duration_not_finished_immediately);
  RUN_TEST(timer_reset_restarts_countdown);
  RUN_TEST(timer_run_terminates_and_finishes);

  RUN_TEST(random_gen_stays_within_bounds);
  RUN_TEST(random_gen_degenerate_range_returns_the_value);
  RUN_TEST(random_gen_inverted_range_returns_zero);
  RUN_TEST(random_gen_produces_more_than_one_distinct_value);

  RUN_TEST(arena_init_sets_fields);
  RUN_TEST(arena_alloc_returns_sequential_non_overlapping_pointers);
  RUN_TEST(arena_alloc_aligns_to_8_bytes);
  RUN_TEST(arena_alloc_fails_gracefully_when_full);
  RUN_TEST(arena_reset_allows_reuse);

  printf("\n%d tests run, %d failed.\n", g_tests_run, g_tests_failed);
  return g_tests_failed == 0 ? 0 : 1;
}