#include "cglm/types-struct.h"
#include "types.h"

void update_transform_matrices(struct transform *t);
vec3s get_right(struct transform *t);
vec3s get_up(struct transform *t);
vec3s get_forward(struct transform *t);
void set_position(struct transform *t, vec3s pos);
void set_rotation(struct transform *t, vec3s rot);
void set_scale(struct transform *t, vec3s scale);
