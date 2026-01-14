#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "libbsp/bsp.h"

/*
https://www.gamers.org/dEngine/quake/spec/quake-spec34/qkspec_4.htm
*/
#define BSP_VERSION 29
#define BSP_LUMP_COUNT 15

enum {
	LUMP_ENTITIES = 0,
	LUMP_PLANES = 1,
	LUMP_MIPTEX = 2,
	LUMP_VERTICES = 3,
	LUMP_VISDATA = 4,
	LUMP_NODES = 5,
	LUMP_TEXINFO = 6,
	LUMP_FACES = 7,
	LUMP_LIGHTING = 8,
	LUMP_CLIPNODES = 9,
	LUMP_LEAVES = 10,
	LUMP_FACELISTS = 11,
	LUMP_EDGES = 12,
	LUMP_SURFEDGES = 13,
	LUMP_MODELS = 14
};


typedef struct {
	int32_t offset;
	int32_t length;
} bsp_lump_t;

typedef struct {
	int32_t version; /* 29 for Q1*/
	bsp_lump_t lumps[BSP_LUMP_COUNT];
} bsp_header_t;


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


struct bsp_t {
	bsp_header_t header;

	bsp_alloc_fn alloc;
	bsp_free_fn free;

	bsp_entity_t* entities;
	size_t num_entities;

	bsp_plane_t* planes;
	size_t num_planes;

	bsp_miptex_dir_t miptex_dir;
	bsp_miptex_t** miptex;
	uint8_t* miptex_raw;
	size_t miptex_raw_size;

	bsp_vertex_t* vertices;
	size_t num_vertices;

	bsp_visdata_t visdata;

	bsp_node_t* nodes;
	size_t num_nodes;

	bsp_texinfo_t* texinfo;
	size_t num_texinfo;

	bsp_face_t* faces;
	size_t num_faces;

	bsp_lighting_t lighting;

	bsp_clipnode_t* clipnodes;
	size_t num_clipnodes;

	bsp_leaf_t* leaves;
	size_t num_leaves;

	bsp_facelist_t facelist;

	bsp_edge_t* edges;
	size_t num_edges;

	bsp_surfedges_t surfedges;

	bsp_model_t* models;
	size_t num_models;
};


static void* bsp_malloc(bsp_t* bsp, size_t size) {
	return bsp->alloc(size);
}

static void* bsp_calloc(bsp_t* bsp, size_t nmemb, size_t size) {
	size_t total = nmemb * size;
	void* p = bsp_malloc(bsp, total);
	if(p) {
		memset(p, 0, total);
	}
	return p;
}

static void bsp_free_ptr(bsp_t* bsp, void* p) {
	if(!p) {
		return;
	}
	bsp->free(p);
}

static void* bsp_realloc_grow(bsp_t* bsp, void* old, size_t old_count, size_t new_count, size_t elem_size) {
	void* np = bsp_malloc(bsp, new_count * elem_size);
	if(!np) {
		return NULL;
	}
	if(old && old_count) {
		size_t copy = old_count * elem_size;
		size_t max = new_count * elem_size;
		if(copy > max)
			copy = max;
		memcpy(np, old, copy);
	}
	bsp_free_ptr(bsp, old);
	return np;
}

static int read_exact(FILE* fp, void* buf, size_t size) {
	return fread(buf, 1, size, fp) == size ? 1 : 0;
}

static int seek_lump(FILE* fp, const bsp_lump_t* l) {
	if(l->offset < 0 || l->length < 0) {
		return 0;
	}
	return fseek(fp, l->offset, SEEK_SET) == 0;
}

static void* alloc_array(bsp_t* bsp, size_t count, size_t elem_size) {
	if(count == 0) {
		return NULL;
	}
	return bsp_calloc(bsp, count, elem_size);
}

static int read_header(FILE* fp, bsp_header_t* hdr) {
	fprintf(stderr, "[BSP] Reading header...\n");
	if(!read_exact(fp, hdr, sizeof(*hdr))) {
		fprintf(stderr, "[BSP] ERROR: Failed to read header\n");
		return 0;
	}
	if(hdr->version != BSP_VERSION) {
		fprintf(stderr, "[BSP] ERROR: Unsupported BSP version: %d (expected %d)\n", hdr->version, BSP_VERSION);
		return 0;
	}
	fprintf(stderr, "[BSP] Header OK: version=%d\n", hdr->version);
	return 1;
}

static char* read_lump_text(bsp_t* bsp, FILE* fp, const bsp_lump_t* l) {
	char* buf = (char*)bsp_malloc(bsp, (size_t)l->length + 1);
	if(!buf) {
		return NULL;
	}
	if(fread(buf, 1, (size_t)l->length, fp) != (size_t)l->length) {
		bsp_free_ptr(bsp, buf);
		return NULL;
	}
	buf[l->length] = '\0';
	return buf;
}

static char* skip_whitespace(char* p) {
	while(*p && isspace((unsigned char)*p)) {
		p++;
	}
	return p;
}

static char* parse_string(bsp_t* bsp, char* p, char** out) {
	p = skip_whitespace(p);
	if(*p != '"') {
		return NULL;
	}
	p++;
	char* start = p;
	while(*p && *p != '"') {
		p++;
	}
	if(!*p) {
		return NULL;
	}
	size_t len = (size_t)(p - start);
	char* str = (char*)bsp_malloc(bsp, len + 1);
	if(!str) {
		return NULL;
	}
	memcpy(str, start, len);
	str[len] = '\0';
	*out = str;
	return p + 1;
}

static int read_entities(FILE* fp, const bsp_lump_t* l, bsp_t* bsp) {
	fprintf(stderr, "[BSP] Reading entities lump (offset=%d, length=%d)...\n", l->offset, l->length);
	if(l->length <= 0) {
		fprintf(stderr, "[BSP] Entities lump empty\n");
		bsp->entities = NULL;
		bsp->num_entities = 0;
		return 1;
	}
	if(!seek_lump(fp, l)) {
		fprintf(stderr, "[BSP] ERROR: Failed to seek to entities lump\n");
		return 0;
	}
	char* text = read_lump_text(bsp, fp, l);
	if(!text) {
		fprintf(stderr, "[BSP] ERROR: Failed to read entities text\n");
		return 0;
	}
	size_t capacity = 16;
	bsp_entity_t* entities = (bsp_entity_t*)bsp_calloc(bsp, capacity, sizeof(bsp_entity_t));
	size_t num_entities = 0;

	char* p = text;
	while(*p) {
		p = skip_whitespace(p);
		if(*p == '{') {
			p++;
			if(num_entities >= capacity) {
				size_t old = capacity;
				capacity *= 2;
				entities = (bsp_entity_t*)bsp_realloc_grow(bsp, entities, old, capacity, sizeof(bsp_entity_t));
				if(!entities) {
					bsp_free_ptr(bsp, text);
					return 0;
				}
			}
			bsp_entity_t* ent = &entities[num_entities++];
			ent->properties = NULL;
			ent->num_properties = 0;

			size_t prop_cap = 8;
			bsp_property_t* props = (bsp_property_t*)bsp_calloc(bsp, prop_cap, sizeof(bsp_property_t));
			size_t num_props = 0;

			while(*p && *p != '}') {
				char* key = NULL;
				char* val = NULL;
				p = parse_string(bsp, p, &key);
				if(!p) {
					break;
				}
				p = parse_string(bsp, p, &val);
				if(!p) {
					bsp_free_ptr(bsp, key);
					break;
				}

				if(num_props >= prop_cap) {
					size_t oldp = prop_cap;
					prop_cap *= 2;
					props = (bsp_property_t*)bsp_realloc_grow(bsp, props, oldp, prop_cap, sizeof(bsp_property_t));
					if(!props) {
						bsp_free_ptr(bsp, text);
						return 0;
					}
				}
				props[num_props].key = key;
				props[num_props].value = val;
				num_props++;

				p = skip_whitespace(p);
			}
			if(*p == '}') {
				p++;
			}


			ent->properties = props;
			ent->num_properties = num_props;
		} else {
			p++;
		}
	}

	bsp->entities = entities;
	bsp->num_entities = num_entities;
	fprintf(stderr, "[BSP] Entities loaded: %zu entities\n", num_entities);

	bsp_free_ptr(bsp, text);
	return 1;
}

void free_entities(bsp_t* bsp) {
	if(!bsp || !bsp->entities) {
		return;
	}

	for(size_t i = 0; i < bsp->num_entities; i++) {
		bsp_entity_t* ent = &bsp->entities[i];
		if(ent->properties) {
			for(size_t j = 0; j < ent->num_properties; j++) {
				bsp_property_t* prop = &ent->properties[j];
				fprintf(stderr, "[BSP] Freeing prop %zu key %s of entity %zu\n",j, prop->key, i);
				bsp_free_ptr(bsp, (void*)prop->key); /* cast to silence Wdiscarded-qualifiers */
				fprintf(stderr, "[BSP] Freeing prop %zu value %s of entity %zu\n",j, prop->value, i);
				bsp_free_ptr(bsp, (void*)prop->value);
			}
			fprintf(stderr, "[BSP] Freeing props of entity %zu\n", i);
			bsp_free_ptr(bsp, ent->properties);
		}
	}
	fprintf(stderr, "[BSP] Freeing entities\n");
	bsp_free_ptr(bsp, bsp->entities);
	bsp->entities = NULL;
	bsp->num_entities = 0;
}


static int read_planes(FILE* fp, const bsp_lump_t* l, bsp_t* bsp) {
	fprintf(stderr, "[BSP] Reading planes lump (offset=%d, length=%d)...\n", l->offset, l->length);
	if(l->length <= 0) {
		fprintf(stderr, "[BSP] Planes lump empty\n");
		bsp->planes = NULL;
		bsp->num_planes = 0;
		return 1;
	}
	if(!seek_lump(fp, l)) {
		fprintf(stderr, "[BSP] ERROR: Failed to seek to planes lump\n");
		return 0;
	}
	size_t count = (size_t)l->length / sizeof(bsp_plane_t);
	fprintf(stderr, "[BSP] Allocating %zu planes...\n", count);
	bsp->planes = (bsp_plane_t*)alloc_array(bsp, count, sizeof(bsp_plane_t));
	if(!bsp->planes && count) {
		fprintf(stderr, "[BSP] ERROR: Failed to allocate planes array\n");
		return 0;
	}
	if(!read_exact(fp, bsp->planes, count * sizeof(bsp_plane_t))) {
		fprintf(stderr, "[BSP] ERROR: Failed to read planes data\n");
		return 0;
	}
	bsp->num_planes = count;
	fprintf(stderr, "[BSP] Planes loaded: %zu planes\n", count);
	return 1;
}

static int read_miptex(FILE* fp, const bsp_lump_t* l, bsp_t* bsp) {
	fprintf(stderr, "[BSP] Reading miptex lump (offset=%d, length=%d)...\n", l->offset, l->length);
	if(l->length <= 0) {
		fprintf(stderr, "[BSP] Miptex lump empty\n");
		bsp->miptex_raw = NULL;
		bsp->miptex_raw_size = 0;
		bsp->miptex_dir.nummiptex = 0;
		bsp->miptex_dir.offsets = NULL;
		bsp->miptex = NULL;
		return 1;
	}
	if(!seek_lump(fp, l)) {
		fprintf(stderr, "[BSP] ERROR: Failed to seek to miptex lump\n");
		return 0;
	}
	bsp->miptex_raw_size = (size_t)l->length;
	fprintf(stderr, "[BSP] Allocating %zu bytes for miptex raw data...\n", bsp->miptex_raw_size);
	bsp->miptex_raw = (uint8_t*)bsp_malloc(bsp, bsp->miptex_raw_size);
	if(!bsp->miptex_raw) {
		fprintf(stderr, "[BSP] ERROR: Failed to allocate miptex raw data\n");
		return 0;
	}
	if(!read_exact(fp, bsp->miptex_raw, bsp->miptex_raw_size)) {
		fprintf(stderr, "[BSP] ERROR: Failed to read miptex raw data\n");
		return 0;
	}

	if(bsp->miptex_raw_size < (size_t)sizeof(int32_t)) {
		return 0;
	}
	const uint8_t* p = bsp->miptex_raw;
	int32_t nummiptex = *(const int32_t*)p;
	p += sizeof(int32_t);
	fprintf(stderr, "[BSP] Miptex directory has %d entries\n", nummiptex);

	size_t dir_size = sizeof(int32_t) + (size_t)nummiptex * sizeof(int32_t);
	if(bsp->miptex_raw_size < dir_size) {
		fprintf(stderr, "[BSP] ERROR: Miptex directory truncated (need %zu bytes, have %zu)\n", dir_size, bsp->miptex_raw_size);
		return 0;
	}

	bsp->miptex_dir.nummiptex = nummiptex;
	fprintf(stderr, "[BSP] Allocating miptex offset array...\n");
	bsp->miptex_dir.offsets = (int32_t*)bsp_malloc(bsp, (size_t)nummiptex * sizeof(int32_t));
	if(!bsp->miptex_dir.offsets && nummiptex) {
		fprintf(stderr, "[BSP] ERROR: Failed to allocate miptex offsets\n");
		return 0;
	}
	memcpy(bsp->miptex_dir.offsets, p, (size_t)nummiptex * sizeof(int32_t));

	fprintf(stderr, "[BSP] Allocating miptex pointer array...\n");
	bsp->miptex = (bsp_miptex_t**)bsp_calloc(bsp, (size_t)nummiptex, sizeof(bsp_miptex_t*));
	if(!bsp->miptex && nummiptex) {
		fprintf(stderr, "[BSP] ERROR: Failed to allocate miptex pointers\n");
		return 0;
	}

	for(int32_t i = 0; i < nummiptex; ++i) {
		int32_t off = bsp->miptex_dir.offsets[i];
		if(off <= 0) {
			fprintf(stderr, "[BSP] Miptex %d: not present (offset=%d)\n", i, off);
			bsp->miptex[i] = NULL;
			continue;
		}
		if((size_t)off >= bsp->miptex_raw_size) {
			fprintf(stderr, "[BSP] ERROR: Miptex %d offset out of range: %d (raw size=%zu)\n", i, off, bsp->miptex_raw_size);
			bsp->miptex[i] = NULL;
			continue;
		}
		if((size_t)off + sizeof(bsp_miptex_t) > bsp->miptex_raw_size) {
			fprintf(stderr, "[BSP] ERROR: Miptex %d struct truncated at offset %d\n", i, off);
			bsp->miptex[i] = NULL;
			continue;
		}
		bsp->miptex[i] = (bsp_miptex_t*)(bsp->miptex_raw + off);
		fprintf(stderr, "[BSP] Miptex %d loaded at offset %d\n", i, off);
	}
	fprintf(stderr, "[BSP] Miptex lump loaded: %d textures\n", nummiptex);
	return 1;
}

static int read_vertices(FILE* fp, const bsp_lump_t* l, bsp_t* bsp) {
	fprintf(stderr, "[BSP] Reading vertices lump (offset=%d, length=%d)...\n", l->offset, l->length);
	if(l->length <= 0) {
		fprintf(stderr, "[BSP] Vertices lump empty\n");
		bsp->vertices = NULL;
		bsp->num_vertices = 0;
		return 1;
	}
	if(!seek_lump(fp, l)) {
		fprintf(stderr, "[BSP] ERROR: Failed to seek to vertices lump\n");
		return 0;
	}
	size_t count = (size_t)l->length / sizeof(bsp_vertex_t);
	fprintf(stderr, "[BSP] Allocating %zu vertices...\n", count);
	bsp->vertices = (bsp_vertex_t*)alloc_array(bsp, count, sizeof(bsp_vertex_t));
	if(!bsp->vertices && count) {
		fprintf(stderr, "[BSP] ERROR: Failed to allocate vertices array\n");
		return 0;
	}
	if(!read_exact(fp, bsp->vertices, count * sizeof(bsp_vertex_t))) {
		fprintf(stderr, "[BSP] ERROR: Failed to read vertices data\n");
		return 0;
	}
	bsp->num_vertices = count;
	fprintf(stderr, "[BSP] Vertices loaded: %zu vertices\n", count);
	return 1;
}

static int read_visdata(FILE* fp, const bsp_lump_t* l, bsp_t* bsp) {
	if(l->length <= 0) {
		bsp->visdata.data = NULL;
		bsp->visdata.size = 0;
		return 1;
	}
	if(!seek_lump(fp, l)) {
		return 0;
	}
	bsp->visdata.size = (size_t)l->length;
	bsp->visdata.data = (uint8_t*)bsp_malloc(bsp, bsp->visdata.size);
	if(!bsp->visdata.data && l->length)
		return 0;
	if(!read_exact(fp, bsp->visdata.data, bsp->visdata.size))
		return 0;
	return 1;
}

static int read_nodes(FILE* fp, const bsp_lump_t* l, bsp_t* bsp) {
	if(l->length <= 0) {
		bsp->nodes = NULL;
		bsp->num_nodes = 0;
		return 1;
	}
	if(!seek_lump(fp, l)) {
		return 0;
	}
	size_t count = (size_t)l->length / sizeof(bsp_node_t);
	bsp->nodes = (bsp_node_t*)alloc_array(bsp, count, sizeof(bsp_node_t));
	if(!bsp->nodes && count) {
		return 0;
	}
	if(!read_exact(fp, bsp->nodes, count * sizeof(bsp_node_t))) {
		return 0;
	}
	bsp->num_nodes = count;
	return 1;
}

static int read_texinfo(FILE* fp, const bsp_lump_t* l, bsp_t* bsp) {
	if(l->length <= 0) {
		bsp->texinfo = NULL;
		bsp->num_texinfo = 0;
		return 1;
	}
	if(!seek_lump(fp, l)) {
		return 0;
	}
	size_t count = (size_t)l->length / sizeof(bsp_texinfo_t);
	bsp->texinfo = (bsp_texinfo_t*)alloc_array(bsp, count, sizeof(bsp_texinfo_t));
	if(!bsp->texinfo && count) {
		return 0;
	}
	if(!read_exact(fp, bsp->texinfo, count * sizeof(bsp_texinfo_t))) {
		return 0;
	}
	bsp->num_texinfo = count;
	return 1;
}

static int read_faces(FILE* fp, const bsp_lump_t* l, bsp_t* bsp) {
	fprintf(stderr, "[BSP] Reading faces lump (offset=%d, length=%d)...\n", l->offset, l->length);
	if(l->length <= 0) {
		fprintf(stderr, "[BSP] Faces lump empty\n");
		bsp->faces = NULL;
		bsp->num_faces = 0;
		return 1;
	}
	if(!seek_lump(fp, l)) {
		fprintf(stderr, "[BSP] ERROR: Failed to seek to faces lump\n");
		return 0;
	}
	size_t count = (size_t)l->length / sizeof(bsp_face_t);
	fprintf(stderr, "[BSP] Allocating %zu faces...\n", count);
	bsp->faces = (bsp_face_t*)alloc_array(bsp, count, sizeof(bsp_face_t));
	if(!bsp->faces && count) {
		fprintf(stderr, "[BSP] ERROR: Failed to allocate faces array\n");
		return 0;
	}
	if(!read_exact(fp, bsp->faces, count * sizeof(bsp_face_t))) {
		fprintf(stderr, "[BSP] ERROR: Failed to read faces data\n");
		return 0;
	}
	bsp->num_faces = count;
	fprintf(stderr, "[BSP] Faces loaded: %zu faces\n", count);
	return 1;
}

static int read_lighting(FILE* fp, const bsp_lump_t* l, bsp_t* bsp) {
	if(l->length <= 0) {
		bsp->lighting.data = NULL;
		bsp->lighting.size = 0;
		return 1;
	}
	if(!seek_lump(fp, l)) {
		return 0;
	}
	bsp->lighting.size = (size_t)l->length;
	bsp->lighting.data = (uint8_t*)bsp_malloc(bsp, bsp->lighting.size);
	if(!bsp->lighting.data && l->length) {
		return 0;
	}
	if(!read_exact(fp, bsp->lighting.data, bsp->lighting.size)) {
		return 0;
	}
	return 1;
}

static int read_clipnodes(FILE* fp, const bsp_lump_t* l, bsp_t* bsp) {
	if(l->length <= 0) {
		bsp->clipnodes = NULL;
		bsp->num_clipnodes = 0;
		return 1;
	}
	if(!seek_lump(fp, l)) {
		return 0;
	}
	size_t count = (size_t)l->length / sizeof(bsp_clipnode_t);
	bsp->clipnodes = (bsp_clipnode_t*)alloc_array(bsp, count, sizeof(bsp_clipnode_t));
	if(!bsp->clipnodes && count) {
		return 0;
	}
	if(!read_exact(fp, bsp->clipnodes, count * sizeof(bsp_clipnode_t))) {
		return 0;
	}
	bsp->num_clipnodes = count;
	return 1;
}

static int read_leaves(FILE* fp, const bsp_lump_t* l, bsp_t* bsp) {
	if(l->length <= 0) {
		bsp->leaves = NULL;
		bsp->num_leaves = 0;
		return 1;
	}
	if(!seek_lump(fp, l)) {
		return 0;
	}
	size_t count = (size_t)l->length / sizeof(bsp_leaf_t);
	bsp->leaves = (bsp_leaf_t*)alloc_array(bsp, count, sizeof(bsp_leaf_t));
	if(!bsp->leaves && count) {
		return 0;
	}
	if(!read_exact(fp, bsp->leaves, count * sizeof(bsp_leaf_t))) {
		return 0;
	}
	bsp->num_leaves = count;
	return 1;
}

static int read_facelists(FILE* fp, const bsp_lump_t* l, bsp_t* bsp) {
	if(l->length <= 0) {
		bsp->facelist.indices = NULL;
		bsp->facelist.count = 0;
		return 1;
	}
	if(!seek_lump(fp, l)) {
		return 0;
	}
	size_t count = (size_t)l->length / sizeof(int16_t);
	bsp->facelist.indices = (int16_t*)alloc_array(bsp, count, sizeof(int16_t));
	if(!bsp->facelist.indices && count) {
		return 0;
	}
	if(!read_exact(fp, bsp->facelist.indices, count * sizeof(int16_t))) {
		return 0;
	}
	bsp->facelist.count = count;
	return 1;
}

static int read_edges(FILE* fp, const bsp_lump_t* l, bsp_t* bsp) {
	if(l->length <= 0) {
		bsp->edges = NULL;
		bsp->num_edges = 0;
		return 1;
	}
	if(!seek_lump(fp, l)) {
		return 0;
	}
	size_t count = (size_t)l->length / sizeof(bsp_edge_t);
	bsp->edges = (bsp_edge_t*)alloc_array(bsp, count, sizeof(bsp_edge_t));
	if(!bsp->edges && count) {
		return 0;
	}
	if(!read_exact(fp, bsp->edges, count * sizeof(bsp_edge_t))) {
		return 0;
	}
	bsp->num_edges = count;
	return 1;
}

static int read_surfedges(FILE* fp, const bsp_lump_t* l, bsp_t* bsp) {
	if(l->length <= 0) {
		bsp->surfedges.indices = NULL;
		bsp->surfedges.count = 0;
		return 1;
	}
	if(!seek_lump(fp, l)) {
		return 0;
	}
	size_t count = (size_t)l->length / sizeof(int32_t);
	bsp->surfedges.indices = (int32_t*)alloc_array(bsp, count, sizeof(int32_t));
	if(!bsp->surfedges.indices && count) {
		return 0;
	}
	if(!read_exact(fp, bsp->surfedges.indices, count * sizeof(int32_t))) {
		return 0;
	}
	bsp->surfedges.count = count;
	return 1;
}

static int read_models(FILE* fp, const bsp_lump_t* l, bsp_t* bsp) {
	if(l->length <= 0) {
		bsp->models = NULL;
		bsp->num_models = 0;
		return 1;
	}
	if(!seek_lump(fp, l)) {
		return 0;
	}
	size_t count = (size_t)l->length / sizeof(bsp_model_t);
	bsp->models = (bsp_model_t*)alloc_array(bsp, count, sizeof(bsp_model_t));
	if(!bsp->models && count) {
		return 0;
	}
	if(!read_exact(fp, bsp->models, count * sizeof(bsp_model_t))) {
		return 0;
	}
	bsp->num_models = count;
	return 1;
}

static void bsp_cleanup(bsp_t* bsp) {
	if(!bsp) {
		return;
	}
	free_entities(bsp);
	bsp_free_ptr(bsp, bsp->planes);
	bsp_free_ptr(bsp, bsp->miptex_raw);
	bsp_free_ptr(bsp, bsp->miptex_dir.offsets);
	bsp_free_ptr(bsp, bsp->miptex);
	bsp_free_ptr(bsp, bsp->vertices);
	bsp_free_ptr(bsp, bsp->visdata.data);
	bsp_free_ptr(bsp, bsp->nodes);
	bsp_free_ptr(bsp, bsp->texinfo);
	bsp_free_ptr(bsp, bsp->faces);
	bsp_free_ptr(bsp, bsp->lighting.data);
	bsp_free_ptr(bsp, bsp->clipnodes);
	bsp_free_ptr(bsp, bsp->leaves);
	bsp_free_ptr(bsp, bsp->facelist.indices);
	bsp_free_ptr(bsp, bsp->edges);
	bsp_free_ptr(bsp, bsp->surfedges.indices);
	bsp_free_ptr(bsp, bsp->models);
}

bsp_t* bsp_create(const bsp_alloc_fn alloc, const bsp_free_fn free) {
	bsp_t* bsp;
	bsp = (bsp_t*)alloc(sizeof(bsp_t));
	if(!bsp) {
		return NULL;
	}
	memset(bsp, 0, sizeof(*bsp));
	bsp->alloc = alloc;
	bsp->free = free;
	return bsp;
}

void bsp_destroy(bsp_t* bsp) {
	if(!bsp) {
		return;
	}
	bsp_cleanup(bsp);
	bsp->free(bsp);
}

int bsp_load_file(bsp_t* out, FILE* fp) {
	fprintf(stderr, "[BSP] Starting BSP file load...\n");
	if(!out || !fp) {
		fprintf(stderr, "[BSP] ERROR: Invalid arguments to bsp_load_file\n");
		return 0;
	}
	if(!read_header(fp, &out->header)) {
		fprintf(stderr, "[BSP] ERROR: Failed to read BSP header\n");
		return 0;
	}

	if(!read_entities(fp, &out->header.lumps[LUMP_ENTITIES], out)) {
		bsp_cleanup(out);
		return 0;
	}
	if(!read_planes(fp, &out->header.lumps[LUMP_PLANES], out)) {
		bsp_cleanup(out);
		return 0;
	}
	if(!read_miptex(fp, &out->header.lumps[LUMP_MIPTEX], out)) {
		bsp_cleanup(out);
		return 0;
	}
	if(!read_vertices(fp, &out->header.lumps[LUMP_VERTICES], out)) {
		bsp_cleanup(out);
		return 0;
	}
	if(!read_visdata(fp, &out->header.lumps[LUMP_VISDATA], out)) {
		bsp_cleanup(out);
		return 0;
	}
	if(!read_nodes(fp, &out->header.lumps[LUMP_NODES], out)) {
		bsp_cleanup(out);
		return 0;
	}
	if(!read_texinfo(fp, &out->header.lumps[LUMP_TEXINFO], out)) {
		bsp_cleanup(out);
		return 0;
	}
	if(!read_faces(fp, &out->header.lumps[LUMP_FACES], out)) {
		bsp_cleanup(out);
		return 0;
	}
	if(!read_lighting(fp, &out->header.lumps[LUMP_LIGHTING], out)) {
		bsp_cleanup(out);
		return 0;
	}
	if(!read_clipnodes(fp, &out->header.lumps[LUMP_CLIPNODES], out)) {
		bsp_cleanup(out);
		return 0;
	}
	if(!read_leaves(fp, &out->header.lumps[LUMP_LEAVES], out)) {
		bsp_cleanup(out);
		return 0;
	}
	if(!read_facelists(fp, &out->header.lumps[LUMP_FACELISTS], out)) {
		bsp_cleanup(out);
		return 0;
	}
	if(!read_edges(fp, &out->header.lumps[LUMP_EDGES], out)) {
		bsp_cleanup(out);
		return 0;
	}
	if(!read_surfedges(fp, &out->header.lumps[LUMP_SURFEDGES], out)) {
		bsp_cleanup(out);
		return 0;
	}
	if(!read_models(fp, &out->header.lumps[LUMP_MODELS], out)) {
		bsp_cleanup(out);
		return 0;
	}

	fprintf(stderr, "[BSP] ========== BSP FILE LOADED SUCCESSFULLY =========\n");
	fprintf(stderr, "[BSP] Summary:\n");
	fprintf(stderr, "[BSP]   Entities: %zu\n", out->num_entities);
	fprintf(stderr, "[BSP]   Planes: %zu\n", out->num_planes);
	fprintf(stderr, "[BSP]   Miptex: %d\n", out->miptex_dir.nummiptex);
	fprintf(stderr, "[BSP]   Vertices: %zu\n", out->num_vertices);
	fprintf(stderr, "[BSP]   Visdata: %zu bytes\n", out->visdata.size);
	fprintf(stderr, "[BSP]   Nodes: %zu\n", out->num_nodes);
	fprintf(stderr, "[BSP]   Texinfo: %zu\n", out->num_texinfo);
	fprintf(stderr, "[BSP]   Faces: %zu\n", out->num_faces);
	fprintf(stderr, "[BSP]   Lighting: %zu bytes\n", out->lighting.size);
	fprintf(stderr, "[BSP]   Clipnodes: %zu\n", out->num_clipnodes);
	fprintf(stderr, "[BSP]   Leaves: %zu\n", out->num_leaves);
	fprintf(stderr, "[BSP]   Facelists: %zu\n", out->facelist.count);
	fprintf(stderr, "[BSP]   Edges: %zu\n", out->num_edges);
	fprintf(stderr, "[BSP]   Surfedges: %zu\n", out->surfedges.count);
	fprintf(stderr, "[BSP]   Models: %zu\n", out->num_models);
	fprintf(stderr, "[BSP] ====================================================\n");
	return 1;
}

size_t bsp_num_entities(const bsp_t* bsp) {
	if(!bsp) {
		return 0;
	}
	return bsp->num_entities;
}

size_t bsp_entity_num_properties(const bsp_t* bsp, size_t entity_index) {
	if(!bsp) {
		return 0;
	}
	if(entity_index >= bsp->num_entities) {
		return 0;
	}
	return bsp->entities[entity_index].num_properties;
}

const char* bsp_entity_property_key(const bsp_t* bsp, size_t entity_index, size_t prop_index) {
	if(!bsp) {
		return NULL;
	}
	if(entity_index >= bsp->num_entities) {
		return NULL;
	}
	const bsp_entity_t* ent = &bsp->entities[entity_index];
	if(prop_index >= ent->num_properties) {
		return NULL;
	}
	return ent->properties[prop_index].key;
}

const char* bsp_entity_property_value(const bsp_t* bsp, size_t entity_index, size_t prop_index) {
	if(!bsp) {
		return NULL;
	}
	if(entity_index >= bsp->num_entities) {
		return NULL;
	}
	const bsp_entity_t* ent = &bsp->entities[entity_index];
	if(prop_index >= ent->num_properties) {
		return NULL;
	}
	return ent->properties[prop_index].value;
}

const char* bsp_entity_get_property(const bsp_t* bsp, size_t entity_index, const char* key) {
	if(!bsp || !key) {
		return NULL;
	}
	if(entity_index >= bsp->num_entities) {
		return NULL;
	}
	const bsp_entity_t* ent = &bsp->entities[entity_index];
	for(size_t i = 0; i < ent->num_properties; ++i) {
		if(ent->properties[i].key && strcmp(ent->properties[i].key, key) == 0) {
			return ent->properties[i].value;
		}
	}
	return NULL;
}

size_t bsp_num_vertices(const bsp_t* bsp) {
	return bsp ? bsp->num_vertices : 0;
}
size_t bsp_num_planes(const bsp_t* bsp) {
	return bsp ? bsp->num_planes : 0;
}
size_t bsp_num_faces(const bsp_t* bsp) {
	return bsp ? bsp->num_faces : 0;
}
size_t bsp_num_edges(const bsp_t* bsp) {
	return bsp ? bsp->num_edges : 0;
}
size_t bsp_num_models(const bsp_t* bsp) {
	return bsp ? bsp->num_models : 0;
}

size_t bsp_visdata_size(const bsp_t* bsp) {
	return bsp ? bsp->visdata.size : 0;
}
size_t bsp_lighting_size(const bsp_t* bsp) {
	return bsp ? bsp->lighting.size : 0;
}

size_t bsp_miptex_count(const bsp_t* bsp) {
	return bsp ? bsp->miptex_dir.nummiptex : 0;
}
