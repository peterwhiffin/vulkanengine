#include "types.h"
#include "cglm/cglm.h"
#include "cglm/struct.h"

void update_transform_matrices(struct transform *t)
{
	vec3s rads = {
		glm_rad(t->rot.x),
		glm_rad(t->rot.y),
		glm_rad(t->rot.z),
	};

	versors v = glms_euler_zyx_quat(rads);

	mat4s T = glms_translate(GLMS_MAT4_IDENTITY, t->pos);
	mat4s R = glms_quat_mat4(v);
	mat4s S = glms_scale(GLMS_MAT4_IDENTITY, t->scale);

	t->world_transform = glms_mat4_mul(T, glms_mat4_mul(R, S));

	t->is_dirty = false;
}

vec3s get_right(struct transform *t)
{
	return (vec3s){ t->world_transform.col[0].x, t->world_transform.col[0].y, t->world_transform.col[0].z };
}

vec3s get_up(struct transform *t)
{
	return (vec3s){ t->world_transform.col[1].x, t->world_transform.col[1].y, t->world_transform.col[1].z };
}

vec3s get_forward(struct transform *t)
{
	return (vec3s){ t->world_transform.col[2].x, t->world_transform.col[2].y, t->world_transform.col[2].z };
}

void set_position(struct transform *t, vec3s pos)
{
	t->pos = pos;
	t->is_dirty = true;
}

void set_rotation(struct transform *t, vec3s rot)
{
	t->rot = rot;
	t->is_dirty = true;
}

void set_scale(struct transform *t, vec3s scale)
{
	t->scale = scale;
	t->is_dirty = true;
}
