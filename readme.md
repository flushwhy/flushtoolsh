# FlushTools

A single-header C utility library that started as a helper for [Raylib](https://www.raylib.com/) games and grew into a general-purpose toolkit for systems and embedded C projects.

Drop `flushtools.h` into your project and you're done — no build system, no dependencies.

---

## Features

| Module | Description |
|---|---|
| **Lambda macros** | Anonymous function helpers for GCC, Clang, and MSVC |
| **Bit packing** | Compact bit-level serialization for network packets |
| **Quantization** | Float ↔ integer compression for bandwidth-sensitive data |
| **Coordinate compression** | 16-bit encode/decode for world-space coordinates |
| **Timer** | Simple second-resolution timer with callback support |
| **Random** | Fast non-cryptographic RNG seeded from a monotonic clock |

---

## Installation

Copy `flushtools.h` into your project and include it:

```c
#include "flushtools.h"
```

No linking required. Works with C11 and C17.

> **Clang lambda support** requires the Blocks runtime:
> ```
> clang -fblocks -lBlocksRuntime main.c -o main
> ```

---

## Usage

### Lambda macros

Write inline callbacks without naming a function. Compiler support varies — see the table below.

```c
// Store in a variable
int (*add)(int, int) = LAMBDA(int, (int a, int b), { return a + b; });
printf("%d\n", add(3, 4)); // 7

// Pass directly to a callback
qsort(arr, n, sizeof(int),
    AS_FN(int, (const void* a, const void* b), {
        return *(int*)a - *(int*)b;
    })
);

// Named function-pointer typedef
FN_TYPE(Predicate, int, int);
Predicate is_even = LAMBDA(int, (int x), { return x % 2 == 0; });
```

**MSVC** doesn't support anonymous functions in C, so use the two-step helper instead:

```c
DECL_FN(int, my_cmp, (const void* a, const void* b), {
    return *(int*)a - *(int*)b;
});
qsort(arr, n, sizeof(int), FN_PTR(my_cmp));
```

| Macro | GCC | Clang | MSVC |
|---|---|---|---|
| `LAMBDA` | ✅ | ✅ | ❌ use `DECL_FN` |
| `AS_FN` | ✅ | ✅ | ❌ use `DECL_FN` |
| `FN_TYPE` | ✅ | ✅ | ✅ |
| `BLOCK` (capture) | ❌ | ✅ | ❌ |
| `DECL_FN` / `FN_PTR` | ✅ | ✅ | ✅ |

---

### Bit packing

Serialize data at the bit level — useful for compact network packets in multiplayer games.

```c
uint8_t buf[16];
net_bit_writer_t writer;
net_writer_init(&writer, buf, sizeof(buf));
net_writer_bits(&writer, 0b101, 3);  // write 3 bits

net_bit_reader_t reader = { buf, 0 };
uint32_t val = net_read_bits(&reader, 3);  // read back → 5
```

---

### Quantization

Compress floats into N-bit integers for compact packet representation.

```c
// Encode a float in [0.0, 1.0] using 8 bits
uint32_t q = net_quantize(0.75f, 0.0f, 1.0f, 8);  // → 191

// Decode back
float f = net_dequantize(q, 0.0f, 1.0f, 8);        // → ~0.75
```

---

### Coordinate compression

Pack world-space floats into a `uint16_t` for network transmission.

```c
uint16_t packed = compass_coord(123.4f, 0.0f, 1000.0f);
float    unpacked = decompress_coord(packed, 0.0f, 1000.0f);
```

---

### Timer

Second-resolution timer with an optional per-tick callback.

```c
void on_tick(void) { printf("tick\n"); }

Timer t;
timer_init(&t, 5);   // 5 second timer
timer_run(&t, on_tick);

// Or poll manually
timer_init(&t, 10);
while (!timer_is_finished(&t)) {
    printf("elapsed: %.0fs\n", timer_get_elapsed(&t));
}
```

---

### Random

Non-cryptographic RNG seeded from the monotonic clock. Good for procedural generation, shuffles, and similar non-security tasks.

```c
unsigned long long roll = random_gen(1, 6);   // simulates a die roll
unsigned long long uid  = random_gen(0, UINT32_MAX);
```

> ⚠️ Not cryptographically secure. Do not use for passwords, tokens, or anything security-sensitive.

---

## Compiler Support

| Compiler | Standard | Notes |
|---|---|---|
| GCC | C11, C17 | Full support |
| Clang | C11, C17 | Full support; `-fblocks` for capture |
| MSVC | C11, C17 | `DECL_FN`/`FN_PTR` instead of `LAMBDA` |

---

## License

MIT License — Copyright (c) 2026 Austin Clark. See [LICENSE](./LICENSE) for details.
