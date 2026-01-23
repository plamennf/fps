#include "entity.h"
#include "main.h"
#include "mesh_catalog.h"

void set_mesh(Entity *entity, char *mesh_name) {
    assert(entity);
    assert(mesh_name);

    entity->mesh_name = copy_string(mesh_name);
}
