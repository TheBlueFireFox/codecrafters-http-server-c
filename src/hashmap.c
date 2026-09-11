#include "hashmap.h"

#include "hashmap_hasher.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>

#define MAX(a, b) (a) < (b) ? (b) : (a)

#ifdef HASHMAP_ENABLE_STATS
#define HASHMAP_STAT_INC(map, field) ((map)->stats.field += 1)
#else
#define HASHMAP_STAT_INC(map, field) ((void)(map))
#endif

HashMapSlotConfigurations hashmap_slot_config(struct HashMapSlotQuery *query) {
  size_t slot_alignment = MAX(query->header_alignment, query->key_alignment);
  slot_alignment = MAX(slot_alignment, query->value_alignment);

  size_t key_offset = align_up(query->header_size, query->key_alignment);
  size_t value_offset =
      align_up(key_offset + query->key_size, query->value_alignment);

  size_t slot_size = align_up(value_offset + query->value_size, slot_alignment);

  // HashMapSlotHeader         │ key bytes │ padding │ value bytes │ padding

  return (struct HashMapSlotConfigurations){
      .key_offset = key_offset,
      .value_offset = value_offset,
      .slot_size = slot_size,
  };
}

static HashMapSlot hashmap_get_slot_inner(HashMapInternal *map, size_t idx,
                                          uint8_t *data) {
  // struct HashMapSlot {
  //   HashMapSlotHeader *header;
  //   const void *key;
  //   const void *value;
  // };

  uint8_t *slot_location = data + (map->slot_size * idx);

  HashMapSlotHeader *header = (HashMapSlotHeader *)slot_location;
  void *key = slot_location + map->key_offset;
  void *value = slot_location + map->value_offset;

  return (HashMapSlot){
      .header = header,
      .key = key,
      .value = value,
  };
}

HashMapSlot hashmap_get_slot_impl(HashMapInternal *map, size_t idx) {
  return hashmap_get_slot_inner(map, idx, map->data);
}

static Hash hashmap_calculate_hash(HashMapInternal *map, const void *key) {
#ifdef HASHMAP_ENABLE_STATS
  HASHMAP_STAT_INC(map, hash_calculations);
#endif
  HashMapHashBuilder ctx = {
      .ctx_data = {0},
      .algo = &map->hash_algo,
  };
  map->hash_algo.init(ctx.ctx_data, map->algo_config);
  map->hash_fn(&ctx, key, map->key_size);
  Hash hash = map->hash_algo.finalize(ctx.ctx_data);
  if (hash == HASHMAP_HASH_EMPTY) {
    hash = 1;
  }
  return hash;
}

void hashmap_init_with_algo_impl(HashMapInternal *map, HashMapAlgorithm algo,
                                 HashMapHashFn hash_fn, HashMapEqFn eq_fn,
                                 size_t key_size, size_t key_alignment,
                                 size_t value_size, size_t value_alignment) {

  struct HashMapSlotQuery query = {
      .header_size = sizeof(HashMapSlotHeader),
      .header_alignment = alignof(HashMapSlotHeader),
      .key_size = key_size,
      .key_alignment = key_alignment,
      .value_size = value_size,
      .value_alignment = value_alignment,
  };

  HashMapSlotConfigurations config = hashmap_slot_config(&query);
  map->len = 0;
  map->capacity = HASHMAP_DEFAULT_CAPACITY;
  map->mask_capacity = HASHMAP_DEFAULT_CAPACITY - 1;
  map->load_factor_percent = HASHMAP_LOAD_FACTOR_PERCENT;
  map->grow_at = (map->capacity * map->load_factor_percent) / 100;
  map->hash_fn = hash_fn;
  map->eq_fn = eq_fn;
  map->key_size = key_size;
  map->key_offset = config.key_offset;
  map->value_size = value_size;
  map->value_offset = config.value_offset;
  map->slot_size = config.slot_size;
  map->hash_algo = algo;
#ifdef HASHMAP_ENABLE_STATS
  map->stats = (HashMapStats){0};
#endif

  map->control = calloc(HASHMAP_DEFAULT_CAPACITY, 1);
  map->distance = calloc(HASHMAP_DEFAULT_CAPACITY, 1);
  map->data = calloc(HASHMAP_DEFAULT_CAPACITY, map->slot_size);
  map->algo_config = calloc(128, 1);

  ASSERT(map->hash_fn != NULL);
  ASSERT(map->eq_fn != NULL);
  ASSERT(map->data != NULL);

  if (map->hash_algo.init_algorithm != NULL) {
    map->hash_algo.init_algorithm(map->algo_config);
  }
}

void hashmap_init_impl(HashMapInternal *map, HashMapHashFn hash_fn,
                       HashMapEqFn eq_fn, size_t key_size, size_t key_alignment,
                       size_t value_size, size_t value_alignment) {
  hashmap_init_with_algo_impl(map, SipHash, hash_fn, eq_fn, key_size,
                              key_alignment, value_size, value_alignment);
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
  map->hash_fn = NULL;
  map->eq_fn = NULL;

  free(map->control);
  free(map->distance);
  free(map->data);
  free(map->algo_config);

  map->control = NULL;
  map->distance = NULL;
  map->data = NULL;
  map->algo_config = NULL;
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
  memset(map->data, 0, map->capacity * map->slot_size);
}

static uint64_t hashmap_mod_capacity(HashMapInternal *map, uint64_t value) {
  return value & map->mask_capacity;
}

static size_t hashmap_probe_distance(HashMapInternal *map, size_t idx,
                                     Hash hash) {
  size_t ideal = hashmap_mod_capacity(map, hash);

  return hashmap_mod_capacity(map, idx + map->capacity - ideal);
}

static Hash hashmap_hash_get_h1(Hash hash) {
  return hash >> HASHMAP_HASH_H1_SHIFT;
}

static uint8_t hashmap_hash_get_h2(Hash hash) {
  return hash & HASHMAP_HASH_H2_MASK;
}

static uint64_t hashmap_group_empties(uint8_t *ctrl) {
  // search 2A
  // 2A 91 17 2A 44 55 2A 12
  // 80 00 00 80 00 00 80 00
  uint64_t group;
  memcpy(&group, ctrl, sizeof(uint64_t));
  return group & HASHMAP_MATCHES_MAP;
}

static uint8_t hashmap_group_find_first_empty(uint8_t *ctrl) {
  uint64_t group = hashmap_group_empties(ctrl);
  return group > 0 ? (__builtin_ctzll(group) / 8) : 8;
}

static uint64_t hashmap_group_match(uint8_t *ctrl, uint8_t fingerprint) {
  // search 2A
  // 2A 91 17 2A 44 55 2A 12
  // 80 00 00 80 00 00 80 00
  uint64_t group;
  memcpy(&group, ctrl, sizeof(uint64_t));

  // will expand the fingerprint to repeate over all bytes
  uint64_t needle = (uint64_t)fingerprint * HASHMAP_NEEDLE_MAP;
  uint64_t x = group ^ needle; // every match will be 0x00
  uint64_t matching_groups =
      (x - HASHMAP_NEEDLE_MAP) & ~x & HASHMAP_MATCHES_MAP;
  return matching_groups;
}

static uint8_t hashmap_group_find_distance_stop(const uint8_t *dist,
                                                uint8_t current_distance) {
  for (uint8_t i = 0; i < HASHMAP_PROBE_GROUP_SIZE; i += 1) {
    if (*(dist + i) < current_distance + 1) {
      return i;
    }
  }
  return HASHMAP_PROBE_GROUP_SIZE;
}

static bool hashmap_put_inner_displaced(HashMapInternal *map, Hash hash,
                                        const void *key, const void *value,
                                        size_t idx, HashMapSlot slot,
                                        size_t distance) {
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

  Hash current_hash = slot.header->hash;
  memcpy(current_key, slot.key, map->key_size);
  memcpy(current_value, slot.value, map->value_size);

  /*
   * ...and place the original input directly into the slot.
   *
   * Importantly, the original key/value never needed to be copied
   * into temporary storage.
   */

  slot.header->hash = hash;

  memcpy(slot.key, key, map->key_size);
  memcpy(slot.value, value, map->value_size);

  HASHMAP_STAT_INC(map, insert_swaps);

  /*
   * We're now carrying the resident that was displaced from this slot.
   */
  distance += 1;
  idx = hashmap_mod_capacity(map, idx + 1);

  while (true) {
    HASHMAP_STAT_INC(map, insert_probes);

    slot = hashmap_get_slot_impl(map, idx);

    /*
     * Found a home for the entry we're carrying.
     */
    if (slot.header->hash == HASHMAP_HASH_EMPTY) {
      slot.header->hash = current_hash;

      memcpy(slot.key, current_key, map->key_size);
      memcpy(slot.value, current_value, map->value_size);

      return false;
    }

    const size_t next_slot_distance =
        hashmap_probe_distance(map, idx, slot.header->hash);

    if (distance > next_slot_distance) {
      HASHMAP_STAT_INC(map, insert_swaps);

      /*
       * Save the resident we're about to evict into the unused
       * carrying buffer.
       */
      const Hash next_hash = slot.header->hash;

      memcpy(next_key, slot.key, map->key_size);
      memcpy(next_value, slot.value, map->value_size);

      /*
       * Put our currently carried entry into this slot.
       */
      slot.header->hash = current_hash;

      memcpy(slot.key, current_key, map->key_size);
      memcpy(slot.value, current_value, map->value_size);

      /*
       * The resident we just evicted becomes the new carried entry.
       */
      current_hash = next_hash;

      uint8_t *tmp;

      tmp = current_key;
      current_key = next_key;
      next_key = tmp;

      tmp = current_value;
      current_value = next_value;
      next_value = tmp;

      distance = next_slot_distance;
    }

    idx = hashmap_mod_capacity(map, idx + 1);
    distance += 1;
  }
}

static bool hashmap_put_inner_direct(HashMapInternal *map, Hash hash,
                                     const void *key, const void *value) {
  size_t idx = hashmap_mod_capacity(map, hash);
  size_t distance = 0;
  // Round robbin based insert
  // if the currently selected pair has a larger travel distance then
  // the one in the current slot, we save the current pair into the slot, while
  // moving the ones from the slot out
  while (true) {
    HASHMAP_STAT_INC(map, insert_probes);
    HashMapSlot slot = hashmap_get_slot_impl(map, idx);
    // Empty slot found
    if (slot.header->hash == HASHMAP_HASH_EMPTY) {
      slot.header->hash = hash;
      memcpy(slot.key, key, map->key_size);
      memcpy(slot.value, value, map->value_size);

      return false;
    }

    // we found the same key
    if (slot.header->hash == hash && map->eq_fn(slot.key, key, map->key_size)) {
      memcpy(slot.value, value, map->value_size);
      return true;
    }

    // How far has the current slot travelled
    size_t slot_distance = hashmap_probe_distance(map, idx, slot.header->hash);

    if (distance > slot_distance) {
      return hashmap_put_inner_displaced(map, hash, key, value, idx, slot,
                                         distance);
    }

    idx = hashmap_mod_capacity(map, idx + 1);
    distance += 1;
  }
}

static bool hashmap_put_inner(HashMapInternal *map, Hash hash, const void *key,
                              const void *value) {
  return hashmap_put_inner_direct(map, hash, key, value);
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

  uint8_t *old_data = map->data;

  HASHMAP_STAT_INC(map, resize_count);

  size_t old_capacity = map->capacity;

  map->control = calloc(new_capacity, 1);
  map->distance = calloc(new_capacity, 1);
  map->data = calloc(new_capacity, map->slot_size);
  ASSERT(map->control != NULL);
  ASSERT(map->distance != NULL);
  ASSERT(map->data != NULL);

  map->capacity = new_capacity;
  map->mask_capacity = new_capacity - 1;
  map->grow_at = (map->capacity * map->load_factor_percent) / 100;

  for (size_t i = 0; i < old_capacity; i += 1) {
    HashMapSlot slot = hashmap_get_slot_inner(map, i, old_data);
    if (slot.header->hash != HASHMAP_HASH_EMPTY) {
      hashmap_put_inner(map, slot.header->hash, slot.key, slot.value);
    }
  }
  free(old_data);
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
  bool existing_entry = hashmap_put_inner(map, hash, key, value);

  if (existing_entry) {
    return true;
  }

  map->len += 1;

  if (map->len > map->grow_at) {
    size_t new_capacity = map->capacity * 2;
    hashmap_resize(map, new_capacity);
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
  Hash h1 = hashmap_hash_get_h1(hash);
  uint8_t h2 = hashmap_hash_get_h2(hash);
  *idx = hashmap_mod_capacity(map, h1);
  uint8_t distance = 0;

  while (true) {

    HASHMAP_STAT_INC(map, lookup_probes);
    uint8_t *ctrl = map->control + *idx;

    // Process a group at a time
    // Find first empty of this group
    uint8_t first_empty = hashmap_group_find_first_empty(ctrl);

    uint8_t first_robin_hood_stop =
        hashmap_group_find_distance_stop(map->distance + *idx, distance);

    /*
     * We cannot examine anything past either:
     *
     *   1. the first EMPTY slot
     *   2. the first Robin Hood termination slot
     */
    uint8_t stop = first_empty < first_robin_hood_stop ? first_empty
                                                       : first_robin_hood_stop;

    uint64_t matches = hashmap_group_match(ctrl, h2);
    while (matches > 0) {
      // returns the
      uint8_t bit = __builtin_ctzll(matches);
      uint8_t slot = bit / 8;
      // the first empty slot was closer then the first fitting h2
      if (first_empty < slot) {
        break;
      }

      if (slot >= stop) {
        *idx = *idx + slot;
        return false;
      }

      // we have a possible match
      HashMapSlot current_slot = hashmap_get_slot_impl(map, *idx + slot);
      if (map->eq_fn(current_slot.key, key, map->key_size)) {
        return true;
      }

      matches &= matches - 1;
    }

    *idx = hashmap_mod_capacity(map, *idx + HASHMAP_PROBE_GROUP_SIZE);
    distance += HASHMAP_PROBE_GROUP_SIZE;
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
    *(map->control + idx) = HASHMAP_HASH_EMPTY;

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
    slot.header->hash = next_slot.header->hash;
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
