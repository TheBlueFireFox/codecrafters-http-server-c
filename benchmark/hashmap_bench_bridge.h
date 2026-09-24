#include "hashmap.h"
/*
 * The C hashmap type we're benchmarking.
 */
typedef HashMap(uint64_t, uint64_t) U64Map;

void init_map_fnv1a(U64Map *map);
void init_map_siphash(U64Map *map);
