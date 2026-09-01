#include "hashmap.h"

#include "hashmap_hasher.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>

static size_t hashmap_calcualte_slot_size(size_t key_size, size_t value_size) {
  return sizeof(HashMapSlotHeader) + key_size + value_size;
}

static HashMapSlot hashmap_get_slot_inner(HashMapInternal *map, size_t idx,
                                          uint8_t *data) {
  // struct HashMapSlot {
  //   HashMapSlotHeader *header;
  //   const void *key;
  //   const void *value;
  // };
  size_t slot_size =
      hashmap_calcualte_slot_size(map->key_size, map->value_size);

  uint8_t *slot_location = data + (slot_size * idx);

  HashMapSlotHeader *header = (HashMapSlotHeader *)slot_location;
  void *key = slot_location + sizeof(HashMapSlotHeader);
  void *value = slot_location + sizeof(HashMapSlotHeader) + map->key_size;

  return (HashMapSlot){
      .header = header,
      .key = key,
      .value = value,
  };
}

static HashMapSlot hashmap_get_slot(HashMapInternal *map, size_t idx) {
  return hashmap_get_slot_inner(map, idx, map->data);
}

static Hash hashmap_calculate_hash(HashMapInternal *map, const void *key) {
  HashMapHashBuilder ctx = {
      .update = map->hash_algo.update,
  };
  map->hash_algo.init(&ctx.ctx);
  map->hash_fn(&ctx, key, map->key_size);
  return map->hash_algo.finalize(&ctx.ctx);
}

static void hashmap_init_data(HashMapInternal *map) {
  // set OCCUPIED to false
  for (size_t i = 0; i < map->capacity; i += 1) {
    HashMapSlot slot = hashmap_get_slot(map, i);
    slot.header->occupied = false;
  }
}

void hashmap_init_with_algo_impl(HashMapInternal *map, HashMapAlgorithm algo,
                                 HashMapHashFn hash_fn, HashMapEqFn eq_fn,
                                 size_t key_size, size_t value_size) {
  map->len = 0;
  map->capacity = HASHMAP_DEFAULT_CAPACITY;
  map->hash_fn = hash_fn;
  map->eq_fn = eq_fn;
  map->key_size = key_size;
  map->value_size = value_size;
  map->hash_algo = algo;

  size_t slot_size = hashmap_calcualte_slot_size(key_size, value_size);
  map->data = malloc(HASHMAP_DEFAULT_CAPACITY * slot_size);

  ASSERT(map->hash_fn != NULL);
  ASSERT(map->eq_fn != NULL);
  ASSERT(map->data != NULL);
  hashmap_init_data(map);
}

void hashmap_init_impl(HashMapInternal *map, HashMapHashFn hash_fn,
                       HashMapEqFn eq_fn, size_t key_size, size_t value_size) {
  hashmap_init_with_algo_impl(map, Fnv1a, hash_fn, eq_fn, key_size, value_size);
}

void hashmap_free_impl(HashMapInternal *map) {
  map->len = 0;
  map->capacity = 0;
  map->key_size = 0;
  map->value_size = 0;
  map->hash_fn = NULL;
  map->eq_fn = NULL;

  free(map->data);
  map->data = NULL;
}

size_t hashmap_len_impl(const HashMapInternal *map) { return map->len; }

static size_t hashmap_probe_distance(HashMapInternal *map, size_t idx,
                                     Hash hash) {
  size_t ideal = hash % map->capacity;

  return (idx + map->capacity - ideal) % map->capacity;
}

static size_t hashmap_load_factor_percent(HashMapInternal *map) {
  return 100 * map->len / map->capacity;
}

static bool hashmap_put_inner_impl(HashMapInternal *map, Hash hash,
                                   const void *key, const void *value) {
  size_t idx = hash % map->capacity;
  size_t distance = 0;

  // Round robbin based insert
  // if the currently selected pair has a larger travel distance then
  // the one in the current slot, we save the current pair into the slot, while
  // moving the ones from the slot out
  Hash current_hash;
  uint8_t current_key[map->key_size];
  uint8_t current_value[map->value_size];

  current_hash = hash;
  memcpy(current_key, key, map->key_size);
  memcpy(current_value, value, map->value_size);

  while (true) {
    HashMapSlot slot = hashmap_get_slot(map, idx);
    // Empty slot found
    if (!slot.header->occupied) {
      slot.header->occupied = true;
      slot.header->hash = current_hash;
      memcpy(slot.key, current_key, map->key_size);
      memcpy(slot.value, current_value, map->value_size);

      return false;
    }

    // we found the same key
    if (slot.header->hash == hash && map->eq_fn(slot.key, key, map->key_size)) {
      memcpy(slot.value, current_value, map->value_size);
      return true;
    }

    // How far has the current slot travelled
    size_t slot_distance = hashmap_probe_distance(map, idx, slot.header->hash);

    // our distance is larger then the one from below, so move the one below
    // further along
    if (distance > slot_distance) {
      Hash new_hash;
      uint8_t new_key[map->key_size];
      uint8_t new_value[map->value_size];

      // tmp copy
      new_hash = slot.header->hash;
      memcpy(new_key, slot.key, map->key_size);
      memcpy(new_value, slot.value, map->value_size);

      // into slot
      slot.header->hash = current_hash;
      memcpy(slot.key, current_key, map->key_size);
      memcpy(slot.value, current_value, map->value_size);

      // current ptr
      current_hash = new_hash;
      memcpy(current_key, new_key, map->key_size);
      memcpy(current_value, new_value, map->value_size);

      distance = slot_distance;
    }

    idx = (idx + 1) % map->capacity;
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
void hashmap_resize(HashMapInternal *map) {
  // Resize storage
  size_t slot_size =
      hashmap_calcualte_slot_size(map->key_size, map->value_size);

  uint8_t *old_data = map->data;

  size_t old_capacity = map->capacity;
  size_t new_capacity = map->capacity * 2;

  map->data = malloc(new_capacity * slot_size);
  ASSERT(map->data != NULL);

  map->capacity = new_capacity;
  hashmap_init_data(map);

  for (size_t i = 0; i < old_capacity; i += 1) {
    HashMapSlot slot = hashmap_get_slot_inner(map, i, old_data);
    if (slot.header->occupied) {
      hashmap_put_inner_impl(map, slot.header->hash, slot.key, slot.value);
    }
  }
  free(old_data);
}

bool hashmap_put_impl(HashMapInternal *map, const void *key,
                      const void *value) {
  Hash hash = hashmap_calculate_hash(map, key);
  bool existing_entry = hashmap_put_inner_impl(map, hash, key, value);

  if (existing_entry) {
    return true;
  }

  map->len += 1;

  size_t per = hashmap_load_factor_percent(map);

  if (per >= HASHMAP_LOAD_FACTOR_PERCENT) {
    hashmap_resize(map);
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
  Hash hash = hashmap_calculate_hash(map, key);
  *idx = hash % map->capacity;
  size_t distance = 0;

  while (true) {

    HashMapSlot slot = hashmap_get_slot(map, *idx);

    // Empty slot found
    if (!slot.header->occupied) {
      return NULL;
    }

    // How far has the current slot travelled
    size_t slot_distance = hashmap_probe_distance(map, *idx, slot.header->hash);

    // we have traveled further then the slot below, so we have reached the end
    // our "ideal" bucket
    if (distance > slot_distance) {
      return false;
    }

    if (slot.header->hash == hash && map->eq_fn(slot.key, key, map->key_size)) {
      return true;
    }

    *idx = (*idx + 1) % map->capacity;
    distance += 1;
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

  HashMapSlot slot = hashmap_get_slot(map, idx);
  return slot.value;
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
  size_t idx;
  bool exists = hashmap_lookup(map, key, &idx);

  if (!exists) {
    return false;
  }

  map->len -= 1;

  while (true) {
    HashMapSlot slot = hashmap_get_slot(map, idx);
    // end of cluster
    if (!slot.header->occupied) {
      return true;
    }
    slot.header->occupied = false;

    size_t next_idx = (idx + 1) % map->capacity;

    HashMapSlot next_slot = hashmap_get_slot(map, next_idx);

    if (!next_slot.header->occupied) {
      return true;
    }

    size_t next_slot_distance =
        hashmap_probe_distance(map, next_idx, slot.header->hash);

    // next_idx is the ideal position for the next entry
    if (next_slot_distance == 0) {
      return true;
    }

    // slide back the next idx
    slot.header->occupied = next_slot.header->occupied;
    slot.header->hash = next_slot.header->hash;
    memcpy(slot.key, next_slot.key, map->key_size);
    memcpy(slot.value, next_slot.value, map->value_size);

    idx = next_idx;
  }
}

bool hashmap_equal_cstr(const void *a, const void *b, size_t key_size) {
  (void)key_size;

  const char *sa = *(const char *const *)a;

  const char *sb = *(const char *const *)b;

  return strcmp(sa, sb) == 0;
}

bool hashmap_equal_bytes(const void *a, const void *b, size_t key_size) {
  return memcmp(a, b, key_size) == 0;
}
