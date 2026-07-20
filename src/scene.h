#include "types.h"

struct entity *get_new_entity(struct render_state *ren);
void entity_add_mesh_renderer(struct render_state *ren, struct entity *e, struct mesh *m);
void entity_add_camera(struct entity *e);
