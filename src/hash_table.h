#pragma once

#include <assert.h>

#include "general.h"

template <typename Key, typename Value>
struct Hash_Table {
    struct Bucket {
        Key key;
        Value value;
    };

    Bucket *buckets = nullptr;
    bool *occupancy_mask = nullptr;
    int allocated = 0;
    int count = 0;

    inline void deallocate() {
        if (buckets) {
            free(buckets);
            buckets = NULL;
        }

        if (occupancy_mask) {
            free(occupancy_mask);
            occupancy_mask = NULL;
        }

        allocated = 0;
        count = 0;
    }
    
    inline void grow() {
        const int HASH_TABLE_INITIAL_CAPACITY = 256;

        if (!buckets) {
            assert(allocated == 0);
            assert(count == 0);

            buckets = (Bucket *)calloc(HASH_TABLE_INITIAL_CAPACITY, sizeof(Bucket));
            occupancy_mask = (bool *)calloc(HASH_TABLE_INITIAL_CAPACITY, sizeof(bool));

            allocated = HASH_TABLE_INITIAL_CAPACITY;
            count = 0;
        } else {
            Hash_Table <Key, Value> new_hash_table = {
                (Bucket *)calloc(allocated * 2, sizeof(Bucket)),
                (bool *)calloc(allocated * 2, sizeof(bool)),
                allocated * 2,
                0,
            };

            for (int i = 0; i < count; i++) {
                if (occupancy_mask[i]) {
                    new_hash_table.add(buckets[i].key, buckets[i].value);
                }
            }
            
            free(buckets);
            free(occupancy_mask);

            *this = new_hash_table;
        }
    }

    inline void add(Key key, Value value) {
        if (count >= allocated) {
            grow();
        }

        auto hk = get_hash(key) & (allocated - 1);
        while (occupancy_mask[hk] && buckets[hk].key != key) {
            hk = (hk + 1) & (allocated - 1);
        }

        occupancy_mask[hk] = true;
        buckets[hk].key = key;
        buckets[hk].value = value;
        count++;
    }

    inline Value *find(Key key) {
        auto hk = get_hash(key) & (allocated - 1);
        for (int i = 0; i < allocated && occupancy_mask[hk] && buckets[hk].key != key; i++) {
            hk = (hk + 1) & (allocated - 1);
        }

        if (buckets && occupancy_mask[hk] && buckets[hk].key == key) {
            return &buckets[hk].value;
        } else {
            return nullptr;
        }
    }
};

template <typename Value>
struct String_Hash_Table {
    struct Bucket {
        char *key;
        Value value;
    };

    Bucket *buckets = nullptr;
    bool *occupancy_mask = nullptr;
    int allocated = 0;
    int count = 0;

    inline void grow() {
        const int HASH_TABLE_INITIAL_CAPACITY = 256;

        if (!buckets) {
            assert(allocated == 0);
            assert(count == 0);
            
            buckets = (Bucket *)calloc(HASH_TABLE_INITIAL_CAPACITY, sizeof(Bucket));
            occupancy_mask = (bool *)calloc(HASH_TABLE_INITIAL_CAPACITY, sizeof(bool));
            
            allocated = HASH_TABLE_INITIAL_CAPACITY;
            count = 0;
        } else {
            String_Hash_Table <Value> new_hash_table = {
                (Bucket *)calloc(allocated * 2, sizeof(Bucket)),
                (bool *)calloc(allocated * 2, sizeof(bool)),
                allocated * 2,
                0,
            };

            for (int i = 0; i < count; i++) {
                if (occupancy_mask[i]) {
                    new_hash_table.add(buckets[i].key, buckets[i].value);
                }
            }

            free(buckets);
            free(occupancy_mask);

            *this = new_hash_table;
        }
    }

    inline void add(char *key, Value value) {
        if (count >= allocated) {
            grow();
        }

        auto hk = get_hash(key) & (allocated - 1);
        while (occupancy_mask[hk] && !strings_match(buckets[hk].key, key)) {
            hk = (hk + 1) & (allocated - 1);
        }

        occupancy_mask[hk] = true;
        buckets[hk].key = copy_string(key);
        buckets[hk].value = value;
        count++;
    }

    inline Value *find(char *key) {
        auto hk = get_hash(key) & (allocated - 1);
        for (int i = 0; i < allocated && occupancy_mask[hk] && !strings_match(buckets[hk].key, key); i++) {
            hk = (hk + 1) & (allocated - 1);
        }

        if (buckets && occupancy_mask[hk] && strings_match(buckets[hk].key, key)) {
            return &buckets[hk].value;
        } else {
            return nullptr;
        }
    }
};
