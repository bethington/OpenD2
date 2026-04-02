/*
 *	Editor Global State and Bridge
 *
 *	Defines the editor's global variables and provides initialization.
 *	File loading is handled by the modified misc_load_mpq_file in misc.c
 *	which reads from disk using editor_bridge_basepath.
 */

#include "structs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Global variable definitions (referenced as extern in structs.h)
// ============================================================================

GLB_DS1EDIT_S glb_ds1edit;
CONFIG_S glb_config;
DS1_S *glb_ds1 = NULL;
DT1_S *glb_dt1 = NULL;

char glb_ds1edit_data_dir[80] = "";
char glb_ds1edit_tmp_dir[80] = "";

// Base path for file loading (used by misc_load_mpq_file in misc.c)
char editor_bridge_basepath[512] = "";

// Additional globals referenced by editor code (types must match structs.h externs)
WRKSPC_DATAS_S glb_wrkspc_datas[WRKSPC_MAX];
char glb_tiles_path[30] = "";
GAMMA_S glb_gamma_str[GC_MAX];

// TXT file path globals (from main.c in the original editor)
char *glb_path_lvltypes_mem = NULL;
char *glb_path_lvltypes_def = NULL;
char *glb_path_lvlprest_mem = NULL;
char *glb_path_lvlprest_def = NULL;
char *glb_path_obj_mem = NULL;
char *glb_path_obj_def = NULL;
char *glb_path_objects_mem = NULL;
char *glb_path_objects_def = NULL;
char **glb_txt_req_ptr[RQ_MAX] = {NULL};

// Stubs for Allegro 4 functions not needed in bridge mode
RLE_SPRITE *get_rle_sprite(BITMAP *bmp, int x, int y, int w, int h) { (void)bmp; (void)x; (void)y; (void)w; (void)h; return NULL; }
char *get_extension(const char *filename) { (void)filename; return ""; }
void show_video_bitmap(BITMAP *bmp) { (void)bmp; }

// ============================================================================
// Initialization
// ============================================================================

void editor_bridge_set_basepath(const char *basePath)
{
	strncpy(editor_bridge_basepath, basePath, sizeof(editor_bridge_basepath) - 1);
	editor_bridge_basepath[sizeof(editor_bridge_basepath) - 1] = '\0';

	// Ensure trailing separator
	size_t len = strlen(editor_bridge_basepath);
	if (len > 0 && editor_bridge_basepath[len - 1] != '\\' && editor_bridge_basepath[len - 1] != '/')
	{
		if (len < sizeof(editor_bridge_basepath) - 1)
		{
			editor_bridge_basepath[len] = '\\';
			editor_bridge_basepath[len + 1] = '\0';
		}
	}
}

void editor_bridge_init(const char *basePath)
{
	editor_bridge_set_basepath(basePath);

	memset(&glb_ds1edit, 0, sizeof(glb_ds1edit));
	memset(&glb_config, 0, sizeof(glb_config));

	if (glb_ds1 == NULL)
		glb_ds1 = (DS1_S *)calloc(DS1_MAX, sizeof(DS1_S));
	if (glb_dt1 == NULL)
		glb_dt1 = (DT1_S *)calloc(DT1_MAX, sizeof(DT1_S));
}

void editor_bridge_shutdown(void)
{
	if (glb_ds1)
	{
		free(glb_ds1);
		glb_ds1 = NULL;
	}
	if (glb_dt1)
	{
		free(glb_dt1);
		glb_dt1 = NULL;
	}
}

// ============================================================================
// Load a DS1 map using the editor's pipeline
// Returns the ds1 index in glb_ds1[], or -1 on failure.
// This calls dt1_add() for each DT1 referenced by the DS1,
// which in turn calls dt1_all_zoom_make() to pre-render all tiles.
// ============================================================================

int editor_bridge_load_ds1(const char *ds1RelativePath)
{
	int ds1_idx = 0; // Use slot 0

	// Build the full path for the DS1 file
	char ds1FullPath[512];
	snprintf(ds1FullPath, sizeof(ds1FullPath), "%sdata\\global\\tiles\\%s",
		editor_bridge_basepath, ds1RelativePath);

	// Normalize slashes
	{
		char *p;
		for (p = ds1FullPath; *p; p++)
			if (*p == '/') *p = '\\';
	}

	printf("EditorBridge: Loading DS1 %s\n", ds1FullPath);

	// Read the DS1 file directly (not through misc_load_mpq_file)
	FILE *f = fopen(ds1FullPath, "rb");
	if (f == NULL)
	{
		printf("EditorBridge: Cannot open DS1 file\n");
		return -1;
	}

	fseek(f, 0, SEEK_END);
	long fileSize = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (fileSize <= 0)
	{
		fclose(f);
		return -1;
	}

	// The editor's ds1_read expects the file to already be loaded
	// We need to call it properly. For now, use a simplified direct approach:
	// Read the DS1 header to get the file list, then load DT1s via dt1_add.

	unsigned char *buf = (unsigned char *)malloc(fileSize);
	fread(buf, 1, fileSize, f);
	fclose(f);

	// Parse DS1 header manually to extract DT1 file references
	unsigned long *readHead = (unsigned long *)buf;
	unsigned long version = *readHead++;
	unsigned long width = (*readHead++) + 1;
	unsigned long height = (*readHead++) + 1;
	unsigned long act = 1;

	if (version >= 8)
	{
		act = *readHead++;
		if (act > 5) act = 5;
	}

	unsigned long tagType = 0;
	if (version >= 10)
	{
		tagType = *readHead++;
	}

	// Initialize DS1 slot
	memset(&glb_ds1[ds1_idx], 0, sizeof(DS1_S));
	glb_ds1[ds1_idx].act = (int)act + 1; // Editor uses 1-based act
	glb_ds1[ds1_idx].width = (int)width;
	glb_ds1[ds1_idx].height = (int)height;
	glb_ds1[ds1_idx].version = (int)version;

	// Read file list and load DT1s
	int numDT1sLoaded = 0;
	if (version >= 3)
	{
		unsigned long fileCount = *readHead++;
		unsigned int fi;

		printf("EditorBridge: DS1 has %lu file refs, act=%lu, size=%lux%lu\n",
			fileCount, act, width, height);

		for (fi = 0; fi < fileCount; fi++)
		{
			char *fname = (char *)readHead;
			size_t n = strlen(fname) + 1;
			readHead = (unsigned long *)(((char *)readHead) + n);

			// Try to load as DT1
			size_t flen = strlen(fname);
			if (flen < 4) continue;

			const char *ext = fname + flen - 4;
			if (_stricmp(ext, ".dt1") != 0) continue;

			printf("EditorBridge: Loading DT1 [%u]: %s\n", fi, fname);

			int dt1_idx = dt1_add(fname);
			if (dt1_idx >= 0)
			{
				if (fi < DT1_IN_DS1_MAX)
					glb_ds1[ds1_idx].dt1_idx[fi] = dt1_idx;
				numDT1sLoaded++;
			}
		}
	}

	printf("EditorBridge: Loaded %d DT1 files\n", numDT1sLoaded);

	// Build the block table (maps tile properties to DT1 blocks)
	if (numDT1sLoaded > 0)
	{
		misc_make_block_table(ds1_idx);
	}

	free(buf);

	return ds1_idx;
}
