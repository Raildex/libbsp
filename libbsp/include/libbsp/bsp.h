#ifndef ENGINE_SRC_BSP_H
#define ENGINE_SRC_BSP_H
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
typedef void* (*bsp_alloc_fn)(size_t size);
typedef void (*bsp_free_fn)(void* ptr);

typedef struct bsp_t bsp_t;

typedef struct {
	float normal[3];
	float dist;
	int32_t type; /* 0: Axial in x, 1: Axial in y, 2: Axial in z. Whatever that means*/
} bsp_plane_t;

typedef struct {
	int32_t nummiptex;
	int32_t* offsets;
} bsp_miptex_dir_t;

typedef struct {
	char name[16];
	uint32_t width;
	uint32_t height;
	uint32_t offsets[4];
} bsp_miptex_t;

typedef struct {
	float x;
	float y;
	float z;
} bsp_vertex_t;


typedef struct {
	uint8_t* data;
	size_t size;
} bsp_visdata_t;

typedef struct {
	int32_t plane_index;
	int16_t children[2];
	int16_t mins[3];
	int16_t maxs[3];
	uint16_t first_face;
	uint16_t num_faces;
} bsp_node_t;

typedef struct {
	float vecs[2][4];
	int32_t miptex;
	int32_t flags;
} bsp_texinfo_t;

typedef struct {
	int16_t plane_index;
	int16_t side; /* 0 front, 1 back */
	int32_t first_edge; /* index into surfedges */
	int16_t num_edges;
	int16_t texinfo; /* index into texinfo */
	uint8_t styles[4]; /* light styles */
	int32_t lightofs; /* offset into lighting lump */
} bsp_face_t;

typedef struct {
	uint8_t* data;
	size_t size;
} bsp_lighting_t;

typedef struct {
	int32_t planenum; /* offset into planes which splits the node*/
	int16_t children[2]; /* > 0 : front child node, -1: outside model, -2: inside model */
} bsp_clipnode_t;


typedef struct {
	int32_t contents;
	int16_t mins[3];
	int16_t maxs[3];
	uint16_t first_face;
	uint16_t num_faces;
	int8_t ambient_level[4];
} bsp_leaf_t;

typedef struct {
	int16_t* indices;
	size_t count;
} bsp_facelist_t;

typedef struct {
	uint16_t v[2]; /* vertex indices */
} bsp_edge_t;

typedef struct {
	int32_t* indices;
	size_t count;
} bsp_surfedges_t;

typedef struct {
	float mins[3];
	float maxs[3];
	float origin[3];
	int32_t headnode[4];
	int32_t first_face;
	int32_t num_faces;
} bsp_model_t;

typedef struct {
	const char* key;
	const char* value;
} bsp_property_t;

typedef struct {
	bsp_property_t* properties;
	size_t num_properties;
} bsp_entity_t;

bsp_t* bsp_create(const bsp_alloc_fn alloc, const bsp_free_fn free);
void bsp_destroy(bsp_t* bsp);

int bsp_load_file(bsp_t* bsp, FILE* f);

size_t bsp_entity_num_properties(const bsp_t* bsp, size_t entity_index);
const char* bsp_entity_property_key(const bsp_t* bsp, size_t entity_index, size_t prop_index);
const char* bsp_entity_property_value(const bsp_t* bsp, size_t entity_index, size_t prop_index);
const char* bsp_entity_get_property(const bsp_t* bsp, size_t entity_index, const char* key);

size_t bsp_num_vertices(const bsp_t* bsp);
size_t bsp_num_planes(const bsp_t* bsp);
size_t bsp_num_faces(const bsp_t* bsp);
size_t bsp_num_edges(const bsp_t* bsp);
size_t bsp_num_models(const bsp_t* bsp);

size_t bsp_visdata_size(const bsp_t* bsp);
size_t bsp_lighting_size(const bsp_t* bsp);

size_t bsp_miptex_count(const bsp_t* bsp);
size_t bsp_get_num_entities(const bsp_t* bsp);
const bsp_entity_t* bsp_get_entities(const bsp_t* bsp);
size_t bsp_get_num_planes(const bsp_t* bsp);
const bsp_plane_t* bsp_get_planes(const bsp_t* bsp);
const bsp_miptex_dir_t* bsp_get_miptex_dir(const bsp_t* bsp);
bsp_miptex_t** bsp_get_miptex(const bsp_t* bsp);
const uint8_t* bsp_get_miptex_raw(const bsp_t* bsp);
size_t bsp_get_miptex_raw_size(const bsp_t* bsp);
size_t bsp_get_num_vertices(const bsp_t* bsp);
const bsp_vertex_t* bsp_get_vertices(const bsp_t* bsp);
const bsp_visdata_t* bsp_get_visdata(const bsp_t* bsp);
size_t bsp_get_num_nodes(const bsp_t* bsp);
const bsp_node_t* bsp_get_nodes(const bsp_t* bsp);
size_t bsp_get_num_texinfo(const bsp_t* bsp);
const bsp_texinfo_t* bsp_get_texinfo(const bsp_t* bsp);
size_t bsp_get_num_faces(const bsp_t* bsp);
const bsp_face_t* bsp_get_faces(const bsp_t* bsp);
const bsp_lighting_t* bsp_get_lighting(const bsp_t* bsp);
size_t bsp_get_num_clipnodes(const bsp_t* bsp);
const bsp_clipnode_t* bsp_get_clipnodes(const bsp_t* bsp);
size_t bsp_get_num_leaves(const bsp_t* bsp);
const bsp_leaf_t* bsp_get_leaves(const bsp_t* bsp);
const bsp_facelist_t* bsp_get_facelist(const bsp_t* bsp);
size_t bsp_get_num_edges(const bsp_t* bsp);
const bsp_edge_t* bsp_get_edges(const bsp_t* bsp);
const bsp_surfedges_t* bsp_get_surfedges(const bsp_t* bsp);
size_t bsp_get_num_models(const bsp_t* bsp);
const bsp_model_t* bsp_get_models(const bsp_t* bsp);
#endif
