#pragma once

#include "array.h"
#include "hash_table.h"

struct Entity;

struct Entity_Manager {
    Hash_Table <u32, Entity *> entity_lookup;
    Array <Entity *> all_entities;

    Array <Entity *> entities_to_be_destroyed;
};

void do_entity_destruction(Entity_Manager *manager);
Entity *get_entity_by_id(Entity_Manager *manager, u32 id);

Entity *make_entity(Entity_Manager *manager);
