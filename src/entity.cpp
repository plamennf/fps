#include "entity.h"
#include "main.h"
#include "mesh_catalog.h"

void set_mesh(Entity *entity, char *mesh_name) {
    assert(entity);
    assert(mesh_name);

    entity->mesh = globals.mesh_catalog->find_or_load(mesh_name);
}
