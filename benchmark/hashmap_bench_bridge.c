#include "hashmap_bench_bridge.h"
#include "hashmap.h"

void init_map_fnv1a(U64Map* map) {
    hashmap_init_impl(
        (&(map)->internal),
        HASHMAP_HASHER_FOR(hashmap_key(map), fnv1a, Fnv1a),
        (hashmap_eq_fn_for_key(hashmap_key(map))),
        hashmap_key_size(map),
        hashmap_alignment_key(map),
        hashmap_value_size(map),
        hashmap_alignment_value(map)
    );
}

void init_map_siphash(U64Map* map) {
    // TODO: replace siphash with faster API
    hashmap_init_with_algo(
        map,
        SipHash,
        hashmap_hash_fn_for_key(hashmap_key(map)),
        hashmap_eq_fn_for_key(hashmap_key(map))
    );
}
