#include "hashmap.h"

#include "hashmap_hasher.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX(a, b) (a) < (b) ? (b) : (a)

#ifdef HASHMAP_ENABLE_STATS
#define HASHMAP_STAT_INC(map, field) ((map)->stats.field += 1)
#else
#define HASHMAP_STAT_INC(map, field) ((void)(map))
#endif

HashMapSlotConfigurations hashmap_slot_config(struct HashMapSlotQuery *query) {
  size_t slot_alignment = MAX(query->key_alignment, query->value_alignment);

  size_t key_offset = 0;
  size_t value_offset =
      align_up(key_offset + query->key_size, query->value_alignment);

  size_t slot_size = align_up(value_offset + query->value_size, slot_alignment);

  // key bytes │ padding │ value bytes │ padding

  return (struct HashMapSlotConfigurations){
      .key_offset = key_offset,
      .value_offset = value_offset,
      .slot_size = slot_size,
  };
}

static inline HashMapSlot hashmap_get_slot_inner(HashMapInternal *map,
                                                 size_t idx, uint8_t *data) {
  // struct HashMapSlot {
  //   HashMapSlotHeader *header;
  //   const void *key;
  //   const void *value;
  // };

  uint8_t *slot_location = data + (map->slot_size * idx);
  void *key = slot_location + map->key_offset;
  void *value = slot_location + map->value_offset;

  return (HashMapSlot){
      .key = key,
      .value = value,
  };
}

static uint8_t *hashmap_setup_region(size_t new_capacity, uint8_t value) {

  uint8_t *region = malloc(new_capacity + HASHMAP_PROBE_GROUP_SIZE);
  memset(region, value, new_capacity + HASHMAP_PROBE_GROUP_SIZE);
  return region;
}

static inline HashMapSlot hashmap_get_slot_impl(HashMapInternal *map,
                                                size_t idx) {
  return hashmap_get_slot_inner(map, idx, map->data);
}

HashMapSlot hashmap_get_slot_externaly_impl(HashMapInternal *map, size_t idx) {
  return hashmap_get_slot_impl(map, idx);
}

Hash hashmap_calculate_hash(HashMapInternal *map, const void *key) {
#ifdef HASHMAP_ENABLE_STATS
  HASHMAP_STAT_INC(map, hash_calculations);
#endif

  if (map->hasher.one_shot != NULL) {
    return map->hasher.one_shot(map->hasher.algo_config, key, map->key_size);
  }

  HashMapHashBuilder ctx = {
      .ctx_data = map->hasher.algo_context,
      .algo = &map->hasher.algo,
  };

  map->hasher.algo.init(ctx.ctx_data, map->hasher.algo_config);
  map->hasher.hash_fn(&ctx, key, map->key_size);
  return map->hasher.algo.finalize(ctx.ctx_data);
}
static void hashmap_init_internal_impl(HashMapInternal *map,
                                       HashMapHasher hasher, HashMapEqFn eq_fn,
                                       size_t key_size, size_t key_alignment,
                                       size_t value_size,
                                       size_t value_alignment) {
  struct HashMapSlotQuery query = {
      .key_size = key_size,
      .key_alignment = key_alignment,
      .value_size = value_size,
      .value_alignment = value_alignment,
  };

  HashMapSlotConfigurations config = hashmap_slot_config(&query);
  map->len = 0;
  map->capacity = HASHMAP_DEFAULT_CAPACITY;
  map->mask_capacity = HASHMAP_DEFAULT_CAPACITY - 1;
  hashmap_set_load_factor_percent_impl(map, HASHMAP_LOAD_FACTOR_PERCENT);
  map->eq_fn = eq_fn;
  map->key_size = key_size;
  map->key_offset = config.key_offset;
  map->value_size = value_size;
  map->value_offset = config.value_offset;
  map->slot_size = config.slot_size;
  map->hasher = hasher;

#ifdef HASHMAP_ENABLE_STATS
  map->stats = (HashMapStats){0};
#endif

  map->control =
      hashmap_setup_region(HASHMAP_DEFAULT_CAPACITY, HASHMAP_HASH_EMPTY);
  HASHMAP_STAT_INC(map, allocations);
  map->distance = hashmap_setup_region(HASHMAP_DEFAULT_CAPACITY, 0);
  HASHMAP_STAT_INC(map, allocations);
  map->data = calloc(HASHMAP_DEFAULT_CAPACITY, map->slot_size);

  HASHMAP_STAT_INC(map, allocations);
  map->hasher.algo_context = calloc(map->hasher.algo.context_size, 1);

  HASHMAP_STAT_INC(map, allocations);

  ASSERT(map->hasher.algo_config != NULL);

  ASSERT(map->eq_fn != NULL);
  ASSERT(map->data != NULL);

  if (map->hasher.algo.init_algorithm != NULL) {
    HASHMAP_STAT_INC(map, allocations);
    map->hasher.algo_config = calloc(map->hasher.algo.config_size, 1);
    ASSERT(map->hasher.algo_context != NULL);

    map->hasher.algo.init_algorithm(map->hasher.algo_context);
  }
}

void hashmap_init_with_algo_impl(HashMapInternal *map, HashMapAlgorithm algo,
                                 HashMapHashFn hash_fn, HashMapEqFn eq_fn,
                                 size_t key_size, size_t key_alignment,
                                 size_t value_size, size_t value_alignment) {
  HashMapHasher hasher = {
      .algo = algo,
      .algo_config = NULL,
      .algo_context = NULL,
      .hash_fn = hash_fn,
      .one_shot = NULL,
  };
  hashmap_init_internal_impl(map, hasher, eq_fn, key_size, key_alignment,
                             value_size, value_alignment);
}

void hashmap_init_impl(HashMapInternal *map, HashMapHasher hasher,
                       HashMapEqFn eq_fn, size_t key_size, size_t key_alignment,
                       size_t value_size, size_t value_alignment) {
  hashmap_init_internal_impl(map, hasher, eq_fn, key_size, key_alignment,
                             value_size, value_alignment);
}

void hashmap_free_impl(HashMapInternal *map) {
  map->len = 0;
  map->capacity = 0;
  map->mask_capacity = 0;
  map->load_factor_percent = 0;
  map->grow_at = 0;
  map->key_size = 0;
  map->key_offset = 0;
  map->value_size = 0;
  map->value_offset = 0;
  map->slot_size = 0;
  map->hasher.hash_fn = NULL;
  map->hasher.one_shot = NULL;
  map->eq_fn = NULL;

  free(map->control);
  free(map->distance);
  free(map->data);
  free(map->hasher.algo_config);
  free(map->hasher.algo_context);

  map->hasher.algo_config = NULL;
  map->control = NULL;
  map->distance = NULL;
  map->data = NULL;
}

void hashmap_set_load_factor_percent_impl(HashMapInternal *map,
                                          size_t load_factor_percent) {
  ASSERT(load_factor_percent > 0 && load_factor_percent < 100);

  map->load_factor_percent = load_factor_percent;
  map->grow_at = (map->capacity * map->load_factor_percent) / 100;
}

bool hashmap_is_empty_impl(HashMapInternal *map) { return map->len == 0; }

size_t hashmap_len_impl(const HashMapInternal *map) { return map->len; }

size_t hashmap_capacity_impl(HashMapInternal *map) { return map->capacity; }

void hashmap_clear_impl(HashMapInternal *map) {
  map->len = 0;
  memset(map->control, HASHMAP_HASH_EMPTY,
         map->capacity + HASHMAP_PROBE_GROUP_SIZE);
}

static inline uint64_t hashmap_mod_capacity(HashMapInternal *map,
                                            uint64_t value) {
  return value & map->mask_capacity;
}

static inline uint64_t hashmap_group_empties(uint64_t group) {
  // search 2A
  // 2A 91 17 2A 44 55 2A 12
  // 80 00 00 80 00 00 80 00
  return group & HASHMAP_MATCHES_MAP;
}

static inline uint8_t hashmap_group_find_first_empty(uint64_t group) {
  group = hashmap_group_empties(group);
  return group > 0 ? (__builtin_ctzll(group) / 8) : 8;
}

static inline uint64_t hashmap_group_match(uint64_t group, uint8_t fingerprint) {
  // search 2A
  // 2A 91 17 2A 44 55 2A 12
  // 80 00 00 80 00 00 80 00
  // will expand the fingerprint to repeate over all bytes
  uint64_t needle = (uint64_t)fingerprint * HASHMAP_NEEDLE_MAP;
  uint64_t x = group ^ needle; // every match will be 0x00
  uint64_t matching_groups =
      (x - HASHMAP_NEEDLE_MAP) & ~x & HASHMAP_MATCHES_MAP;
  return matching_groups;
}

static void hashmap_control_set(HashMapInternal *map, size_t idx, uint8_t h2,
                                uint8_t distance) {
  // inpreparation for "mirrored bytes" aka a copy of the first 8 also in the
  // back it would make a group lookup possible from any location
  map->control[idx] = h2;
  map->distance[idx] = distance;

  if (idx < HASHMAP_PROBE_GROUP_SIZE) {
    map->control[idx + map->capacity] = h2;
    map->distance[idx + map->capacity] = distance;
  }
}

static bool hashmap_put_inner_displaced(HashMapInternal *map, const void *key,
                                        const void *value, size_t idx,
                                        uint8_t h2, uint8_t distance,
                                        bool reinserting) {

  HashMapSlot slot = hashmap_get_slot_impl(map, idx);

  /*
   * Two carrying buffers.
   *
   * "current" contains the entry we're trying to place.
   * "next" is scratch space for the next resident we displace.
   */

  uint8_t key_a[map->key_size];
  uint8_t key_b[map->key_size];

  uint8_t value_a[map->value_size];
  uint8_t value_b[map->value_size];

  uint8_t *current_key = key_a;
  uint8_t *next_key = key_b;

  uint8_t *current_value = value_a;
  uint8_t *next_value = value_b;

  /*
   * Perform the first displacement specially.
   *
   * Save the resident entry into our first carrying buffer...
   */

  uint8_t current_h2 = map->control[idx];
  uint8_t current_distance = map->distance[idx];

  memcpy(current_key, slot.key, map->key_size);
  memcpy(current_value, slot.value, map->value_size);

  /*
   * ...and place the original input directly into the slot.
   *
   * Importantly, the original key/value never needed to be copied
   * into temporary storage.
   */

  hashmap_control_set(map, idx, h2, distance);

  memcpy(slot.key, key, map->key_size);
  memcpy(slot.value, value, map->value_size);

  HASHMAP_STAT_INC(map, insert_swaps);

  if (current_distance == HASHMAP_MAX_DISTANCE_ALLOWED) {
    // TODO: trigger resize or rather finish with this
    // delete and run resize at the end

    exit(EXIT_FAILURE);
  }

  /*
   * We're now carrying the resident that was displaced from this slot.
   */
  current_distance += 1;
  idx = hashmap_mod_capacity(map, idx + 1);

  while (true) {
    HASHMAP_STAT_INC(map, insert_probes);
    if (reinserting && current_distance % HASHMAP_PROBE_GROUP_SIZE == 0) {
      HASHMAP_STAT_INC(map, groups_scanned_during_reinsertion);
    }

    const uint8_t ctrl = map->control[idx];
    slot = hashmap_get_slot_impl(map, idx);

    /*
     * Found a home for the entry we're carrying.
     */
    if (ctrl == HASHMAP_HASH_EMPTY) {
      hashmap_control_set(map, idx, current_h2, current_distance);

      memcpy(slot.key, current_key, map->key_size);
      memcpy(slot.value, current_value, map->value_size);

      return false;
    }

    const size_t next_slot_distance = map->distance[idx];

    if (current_distance > next_slot_distance) {
      HASHMAP_STAT_INC(map, insert_swaps);

      /*
       * Save the resident we're about to evict into the unused
       * carrying buffer.
       */
      const uint8_t next_h2 = ctrl;
      const uint8_t next_distance = next_slot_distance;

      memcpy(next_key, slot.key, map->key_size);
      memcpy(next_value, slot.value, map->value_size);

      /*
       * Put our currently carried entry into this slot.
       */
      hashmap_control_set(map, idx, current_h2, current_distance);

      memcpy(slot.key, current_key, map->key_size);
      memcpy(slot.value, current_value, map->value_size);

      /*
       * The resident we just evicted becomes the new carried entry.
       */
      current_h2 = next_h2;
      current_distance = next_distance;

      uint8_t *tmp;

      tmp = current_key;
      current_key = next_key;
      next_key = tmp;

      tmp = current_value;
      current_value = next_value;
      next_value = tmp;
    }

    if (current_distance == HASHMAP_MAX_DISTANCE_ALLOWED) {
      // TODO: trigger resize or rather finish with this
      // delete and run resize at the end
      exit(EXIT_FAILURE);
    }

    idx = hashmap_mod_capacity(map, idx + 1);
    current_distance += 1;
  }
}

static inline bool hashmap_put_inner(HashMapInternal *map, Hash hash,
                                     const void *key, const void *value,
                                     bool reinserting) {
  Hash h1 = HASHMAP_HASH_H1(hash);
  uint8_t h2 = HASHMAP_HASH_H2(hash);

  size_t idx = hashmap_mod_capacity(map, h1);

  size_t distance = 0;
  // Round robbin based insert
  // if the currently selected pair has a larger travel distance then
  // the one in the current slot, we save the current pair into the slot, while
  // moving the ones from the slot out
  while (true) {
    HASHMAP_STAT_INC(map, insert_probes);
    if (reinserting && distance % HASHMAP_PROBE_GROUP_SIZE == 0) {
      HASHMAP_STAT_INC(map, groups_scanned_during_reinsertion);
    }
    // Empty slot found
    const uint8_t ctrl = map->control[idx];

    if (ctrl == HASHMAP_HASH_EMPTY) {

      HashMapSlot slot = hashmap_get_slot_impl(map, idx);

      hashmap_control_set(map, idx, h2, distance);
      memcpy(slot.key, key, map->key_size);
      memcpy(slot.value, value, map->value_size);

      return false;
    }

    // we found the same key
    if (ctrl == h2) {
      HashMapSlot slot = hashmap_get_slot_impl(map, idx);

      if (map->eq_fn(slot.key, key, map->key_size)) {
        memcpy(slot.value, value, map->value_size);
        return true;
      }
    }

    // How far has the current slot travelled
    uint8_t slot_distance = map->distance[idx];

    if (distance > slot_distance) {
      return hashmap_put_inner_displaced(map, key, value, idx, h2, distance,
                                         reinserting);
    }

    if (distance == HASHMAP_MAX_DISTANCE_ALLOWED) {
      // TODO: trigger resize
      exit(EXIT_FAILURE);
    }

    idx = hashmap_mod_capacity(map, idx + 1);
    distance += 1;
  }
}

// PUT
//   │
//   ├─ hash key once
//   │
//   ├─ ideal = hash % capacity
//   │
//   ├─ linear probe
//   │
//   └─ swap whenever:
//        incoming_distance > existing_distance
static void hashmap_resize(HashMapInternal *map, size_t new_capacity) {
  // Resize storage

#ifdef HASHMAP_ENABLE_STATS
  struct timespec resize_start;
  (void)clock_gettime(CLOCK_MONOTONIC, &resize_start);
#endif

  uint8_t *old_data = map->data;
  uint8_t *old_control = map->control;
  uint8_t *old_distance = map->distance;

  HASHMAP_STAT_INC(map, resize_count);

  size_t old_capacity = map->capacity;

  map->control = hashmap_setup_region(new_capacity, HASHMAP_HASH_EMPTY);
  HASHMAP_STAT_INC(map, allocations);
  map->distance = hashmap_setup_region(new_capacity, 0);
  HASHMAP_STAT_INC(map, allocations);
  map->data = calloc(new_capacity, map->slot_size);
  HASHMAP_STAT_INC(map, allocations);
  ASSERT(map->control != NULL);
  ASSERT(map->distance != NULL);
  ASSERT(map->data != NULL);

  map->capacity = new_capacity;
  map->mask_capacity = new_capacity - 1;

  hashmap_set_load_factor_percent_impl(map, map->load_factor_percent);

  for (size_t i = 0; i < old_capacity; i += 1) {
    HashMapSlot slot = hashmap_get_slot_inner(map, i, old_data);
    if (old_control[i] != HASHMAP_HASH_EMPTY) {
      HASHMAP_STAT_INC(map, migrated_elements);
      HASHMAP_STAT_INC(map, entries_reinserted);
      Hash hash = hashmap_calculate_hash(map, slot.key);
      hashmap_put_inner(map, hash, slot.key, slot.value, true);
    }
  }
  free(old_data);
  free(old_control);
  free(old_distance);

#ifdef HASHMAP_ENABLE_STATS
  struct timespec resize_end;
  (void)clock_gettime(CLOCK_MONOTONIC, &resize_end);
  int64_t elapsed_ns =
      ((int64_t)(resize_end.tv_sec - resize_start.tv_sec) * 1000000000LL) +
      resize_end.tv_nsec - resize_start.tv_nsec;
  map->stats.resize_only_ns += (size_t)elapsed_ns;
#endif
}

static size_t hashmap_next_power_of_two(size_t value) {
  if (value <= 1) {
    return 1;
  }

  size_t power = 1;

  while (power < value) {
    power <<= 1;
  }

  return power;
}

void hashmap_reserve_impl(HashMapInternal *map, size_t size) {
  size_t load_factor_amount = (map->capacity * map->load_factor_percent) / 100;

  if (size <= load_factor_amount) {
    return;
  }

  // calculate the next larger power of 2 that fullfills the size and the load
  // factor requirements

  size_t required_size = (size * 100) / map->load_factor_percent;

  size_t new_capacity = hashmap_next_power_of_two(required_size);

  hashmap_resize(map, new_capacity);
}

bool hashmap_put_impl(HashMapInternal *map, const void *key,
                      const void *value) {
  HASHMAP_STAT_INC(map, put_calls);
  Hash hash = hashmap_calculate_hash(map, key);
  bool existing_entry = hashmap_put_inner(map, hash, key, value, false);

  if (existing_entry) {
    return true;
  }

  map->len += 1;

  if (map->len > map->grow_at) {
    HASHMAP_STAT_INC(map, growing_inserts);
    size_t new_capacity = map->capacity * 2;
    hashmap_resize(map, new_capacity);
  } else {
    HASHMAP_STAT_INC(map, reserved_inserts);
  }

  return false;
}

// LOOKUP
//   │
//   ├─ hash key once
//   │
//   ├─ linear probe
//   │
//   ├─ compare stored hash
//   ├─ find entry
//   │
//   └─ stop early whenever:
//        search_distance > existing_distance
//

static bool hashmap_lookup(HashMapInternal *map, const void *key, size_t *idx) {
  HASHMAP_STAT_INC(map, lookup_calls);
  Hash hash = hashmap_calculate_hash(map, key);
  Hash h1 = HASHMAP_HASH_H1(hash);
  uint8_t h2 = HASHMAP_HASH_H2(hash);
  *idx = hashmap_mod_capacity(map, h1);

  while (true) {

    HASHMAP_STAT_INC(map, lookup_probes);
    uint8_t *ctrl = map->control + *idx;

    // Process a group at a time
    // Find first empty of this group
    uint64_t group;
    memcpy(&group, ctrl, sizeof(uint64_t));

    uint8_t stop = hashmap_group_find_first_empty(group);

    uint64_t matches = hashmap_group_match(group, h2);
    while (matches > 0) {
      // returns the
      uint8_t bit = __builtin_ctzll(matches);
      uint8_t slot = bit / 8;

      if (slot >= stop) {
        break;
      }

      // we have a possible match
      size_t slot_idx = hashmap_mod_capacity(map, *idx + slot);
      HashMapSlot current_slot = hashmap_get_slot_impl(map, slot_idx);
      if (map->eq_fn(current_slot.key, key, map->key_size)) {
        HASHMAP_STAT_INC(map, lookup_hits);
        *idx = hashmap_mod_capacity(map, *idx + slot);
        return true;
      }

      matches &= matches - 1;
    }

    /*
     * We checked every possible matching H2 before the stopping
     * point and none contained our key.
     *
     * EMPTY or Robin-Hood termination means the key cannot appear
     * later in the table.
     */
    if (stop < HASHMAP_PROBE_GROUP_SIZE) {
      *idx = hashmap_mod_capacity(map, *idx + stop);
      return false;
    }

    *idx = hashmap_mod_capacity(map, *idx + HASHMAP_PROBE_GROUP_SIZE);
  }
}

// GET
//   │
//   ├─ find entry
//   │
//   └─ return slot value ptr
void *hashmap_get_impl(HashMapInternal *map, const void *key) {
  size_t idx;
  bool exists = hashmap_lookup(map, key, &idx);

  if (!exists) {
    return NULL;
  }

  HashMapSlot slot = hashmap_get_slot_impl(map, idx);
  return slot.value;
}

bool hashmap_contains_impl(HashMapInternal *map, const void *key) {
  size_t idx;
  return hashmap_lookup(map, key, &idx);
}

// REMOVE
//   │
//   ├─ find entry
//   │
//   ├─ create hole
//   │
//   ├─ shift following entries backward
//   │    while their distance > 0
//   │
//   └─ mark final hole empty

bool hashmap_remove_impl(HashMapInternal *map, const void *key) {
  HASHMAP_STAT_INC(map, remove_calls);
  size_t idx;
  bool exists = hashmap_lookup(map, key, &idx);

  if (!exists) {
    return false;
  }

  map->len -= 1;

  while (true) {
    HASHMAP_STAT_INC(map, remove_probes);
    // end of cluster
    if (*(map->control + idx) == HASHMAP_HASH_EMPTY) {
      return true;
    }
    hashmap_control_set(map, idx, HASHMAP_HASH_EMPTY, 0);

    size_t next_idx = hashmap_mod_capacity(map, idx + 1);

    if (*(map->control + next_idx) == HASHMAP_HASH_EMPTY) {
      return true;
    }

    // how far has the next slot traveled
    uint8_t next_slot_distance = map->distance[next_idx];

    // next_idx is the ideal position for the next entry
    if (next_slot_distance == 0) {
      return true;
    }

    HashMapSlot slot = hashmap_get_slot_impl(map, idx);

    HashMapSlot next_slot = hashmap_get_slot_impl(map, next_idx);

    // slide back the next idx
    hashmap_control_set(map, idx, *(map->control + next_idx),
                        *(map->distance + next_idx) - 1);
    memcpy(slot.key, next_slot.key, map->key_size);
    memcpy(slot.value, next_slot.value, map->value_size);

    idx = next_idx;
  }
}

#ifdef HASHMAP_ENABLE_STATS
HashMapStats hashmap_stats_impl(const HashMapInternal *map) {
  return map->stats;
}

void hashmap_stats_reset_impl(HashMapInternal *map) {
  map->stats = (HashMapStats){0};
}
#endif
