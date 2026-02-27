# Defensive Coding: Parallel Arrays & Magic Numbers

Two common C/C++ bug patterns and how to prevent them.

## Problem 1: Parallel Arrays Out of Sync

When an enum and one or more arrays must have the same number of entries:

```cpp
enum Color { RED, GREEN, BLUE, COLOR_COUNT };
const char* names[COLOR_COUNT] = { "Red", "Green", "Blue" };
const char* codes[COLOR_COUNT] = { "#f00", "#0f0", "#00f" };
```

If someone adds `YELLOW` to the enum but forgets to update one of the arrays,
the compiler won't complain — the missing slot silently becomes NULL.
At runtime, accessing it crashes or returns garbage.

**Fix:** Add a `static_assert` after each array:

```cpp
static_assert(sizeof(names) / sizeof(names[0]) == COLOR_COUNT,
    "names array out of sync with Color enum");
static_assert(sizeof(codes) / sizeof(codes[0]) == COLOR_COUNT,
    "codes array out of sync with Color enum");
```

This costs nothing at runtime — it only runs at compile time.
If the sizes don't match, the build fails with a clear message.

## Problem 2: Magic Numbers Duplicated

When a size or count appears in multiple places:

```cpp
// Allocate 22 slots
mem = malloc(22 * 32);
// ... 200 lines later, a comment says "22 slots" but you add a 23rd user
```

The comment and the number are not connected. When one changes,
the other is easily forgotten.

**Fix:** Use a named constant:

```cpp
const int SLOT_COUNT = 24; // 22 active + 2 spare
mem = malloc(SLOT_COUNT * 32);
```

Now there is one source of truth. Changing the count in one place
updates it everywhere.

## General Principle

If two things must stay in sync, make the compiler enforce it.
Humans forget — compilers don't.
