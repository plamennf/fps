#include "entity.h"
#include "entity_manager.h"

#include <stdlib.h>

void do_entity_destruction(Entity_Manager *manager) {
    for (Entity *e : manager->entities_to_be_destroyed) {
        int index = manager->all_entities.find(e);
        if (index != -1) {
            manager->all_entities.ordered_remove_by_index(index);
        }

        for (int i = 0; i < manager->entity_lookup.allocated; i++) {
            auto bucket = &manager->entity_lookup.buckets[i];
            if (bucket->key == e->id && bucket->value == e) {
                manager->entity_lookup.occupancy_mask[i] = false;
                break;
            }
        }
        manager->entity_lookup.count--;

        delete e;
    }
    manager->entities_to_be_destroyed.count = 0;
}

Entity *get_entity_by_id(Entity_Manager *manager, u32 id) {
    Entity **_e = manager->entity_lookup.find(id);
    if (!_e) return NULL;
    return *_e;
}

static u32 generate_id(Entity_Manager *manager) {
    for (;;) {
        u32 id = (u32)rand();
        Entity **_e = manager->entity_lookup.find(id);
        if (!_e) return id;
    }

    return 0;
}

static void register_entity(Entity_Manager *manager, Entity *e) { //, Entity_Type type) {
    u32 id = generate_id(manager);

    e->id      = id;
    e->manager = manager;
    //e->type    = type;
    e->scheduled_for_destruction = false;

    manager->entity_lookup.add(id, e);
    manager->all_entities.add(e);
}

Entity *make_entity(Entity_Manager *manager) {
    Entity *entity = new Entity();

    register_entity(manager, entity);

    return entity;
}
