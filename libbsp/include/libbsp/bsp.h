#ifndef ENGINE_SRC_BSP_H
#define ENGINE_SRC_BSP_H
#include <stddef.h>
#include <stdio.h>

typedef void* (*bsp_alloc_fn)(size_t size);
typedef void  (*bsp_free_fn)(void* ptr);

typedef struct bsp_t bsp_t;

bsp_t* bsp_create(const bsp_alloc_fn alloc, const bsp_free_fn free);
void bsp_destroy(bsp_t* bsp);

int bsp_load_file(bsp_t* bsp, FILE* f);

size_t bsp_num_entities(const bsp_t* bsp);
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

#endif