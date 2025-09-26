#pragma once

// Fallback macro definitions so editors/LSPs don’t flag these as undefined
// when compile_definitions from CMake aren’t visible to the indexer.

#ifndef KARING_MAX_LIMIT
#define KARING_MAX_LIMIT 100
#endif

#ifndef KARING_BUILD_LIMIT
#define KARING_BUILD_LIMIT KARING_MAX_LIMIT
#endif

#ifndef KARING_HARD_MAX_FILE_BYTES
#define KARING_HARD_MAX_FILE_BYTES 20971520
#endif

#ifndef KARING_HARD_MAX_TEXT_BYTES
#define KARING_HARD_MAX_TEXT_BYTES 10485760
#endif
