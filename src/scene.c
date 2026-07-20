#include "types.h"
#include "stdio.h"
#include "renderer.h"
#include "cglm/cglm.h"
#include "transform.h"

void entity_set_flag(struct entity *e, enum entity_flags flags)
{
	e->flags |= flags;
}

void entity_unset_flag(struct entity *e, enum entity_flags flags)
{
	e->flags &= ~flags;
}

struct entity *get_new_entity(struct render_state *ren)
{
	struct entity *e = &ren->entities[ren->entity_count];
	ren->entity_count += 1;
	*e = (struct entity){ 0 };
	e->transform.scale = (vec3s){ 1.0f, 1.0f, 1.0f };
	e->transform.is_dirty = true;
	snprintf(e->name, 128, "%s %u", "Entity", ren->entity_count);
	return e;
}

void entity_add_mesh_renderer(struct render_state *ren, struct entity *e, struct mesh *m)
{
	entity_set_flag(e, MESH_RENDERER);
	e->mesh_renderer.mesh = m;
	e->mesh_renderer.material_index = 0;

	vk_create_entity_uniform_buffer(ren, e);
	vk_create_entity_descriptor_sets(ren, e);
}

void entity_add_camera(struct entity *e)
{
	entity_set_flag(e, CAMERA);
	e->camera.fov = glm_rad(78.0f);
	e->camera.is_perspective = true;
}

void scene_init(struct render_state *ren)
{
	struct entity *e = get_new_entity(ren);
	entity_add_mesh_renderer(ren, e, &ren->meshes[0]);
	set_scale(&e->transform, (vec3s){ 0.01f, 0.01f, 0.01f });

	// e = get_new_entity(ren);
	// entity_add_mesh_renderer(ren, e, &ren->meshes[0]);
}
