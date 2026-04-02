#include "MapRenderer.hpp"
#include "D2Client.hpp"
#include <cstring>
#include <cstdio>

#ifdef USE_ALLEGRO5
#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

// Editor bridge functions (C linkage)
extern "C" {
	void editor_bridge_init(const char *basePath);
	void editor_bridge_shutdown(void);
	int editor_bridge_load_ds1(const char *ds1RelativePath);

	// Editor types we need to read
	typedef struct {
		ALLEGRO_BITMAP *al_bmp;
		int w, h;
		unsigned char **line;
		int clip_x1, clip_y1;
		int clip_x2, clip_y2;
		void *vtable;
		unsigned char *data;
	} BITMAP_A4;

	typedef struct {
		int ds1_usage;
		char name[80];
		void *buffer;
		long buff_len;
		long x1, x2;
		long block_num;
		long bh_start;
		void *bh_buffer;
		int bh_buff_len;
		BITMAP_A4 **block_zoom[5]; // ZM_MAX = 5
		int bz_size[5];
	} DT1_S_EXT;

	typedef struct {
		long direction;
		long roof_y;
		short sound;
		char animated;
		long size_y;
		long size_x;
		long zeros1;
		long orientation;
		long main_index;
		long sub_index;
		long rarity;
		// ... rest of BLOCK_S
	} BLOCK_S_EXT;

	// Globals from editor
	extern DT1_S_EXT *glb_dt1;

	// DS1 globals
	typedef struct DS1_S_EXT DS1_S_EXT;
	extern DS1_S_EXT *glb_ds1;
}

static ALLEGRO_FONT *s_pFont = nullptr;
static bool s_editorInited = false;

static void EnsureFont()
{
	if (s_pFont == nullptr)
		s_pFont = al_create_builtin_font();
}
#endif

MapRenderer *gpMapRenderer = nullptr;

MapRenderer::MapRenderer()
	: m_ds1Handle(INVALID_HANDLE), m_mapWidth(0), m_mapHeight(0), m_act(1),
	  m_cameraX(0), m_cameraY(0)
{
}

MapRenderer::~MapRenderer()
{
	UnloadMap();
}

void MapRenderer::UnloadMap()
{
#ifdef USE_ALLEGRO5
	for (auto &pair : m_tileCache)
	{
		if (pair.second)
			al_destroy_bitmap(pair.second);
	}
	m_tileCache.clear();
#endif

	m_tileLookup.clear();
	m_loadedDT1s.clear();
	m_ds1Handle = INVALID_HANDLE;
	m_mapWidth = 0;
	m_mapHeight = 0;
}

bool MapRenderer::LoadMap(const char *relativePath)
{
	UnloadMap();

#ifdef USE_ALLEGRO5
	// Initialize editor bridge if not done yet
	if (!s_editorInited)
	{
		editor_bridge_init(openConfig->szBasePath);
		s_editorInited = true;
	}

	// Load DS1 using the engine (for cell access via module API)
	char ds1Path[MAX_D2PATH];
	snprintf(ds1Path, MAX_D2PATH, "data\\global\\tiles\\%s", relativePath);
	for (char *p = ds1Path; *p; p++)
		if (*p == '/') *p = '\\';

	m_ds1Handle = engine->DS1_Load(ds1Path);
	if (m_ds1Handle == INVALID_HANDLE)
	{
		engine->Print(PRIORITY_MESSAGE, "MapRenderer: Failed to load DS1 via engine");
		return false;
	}

	engine->DS1_GetSize(m_ds1Handle, m_mapWidth, m_mapHeight);
	m_act = engine->DS1_GetAct(m_ds1Handle);

	if (m_act >= 1 && m_act <= 5)
		engine->renderer->SetGlobalPalette((D2Palettes)(m_act - 1));

	// Load DT1s using the EDITOR's pipeline (dt1_add -> dt1_all_zoom_make)
	// This pre-renders all tile blocks into BITMAP_A4s with 8-bit palette data
	int editorDs1 = editor_bridge_load_ds1(relativePath);

	// Build tile lookup from the editor's loaded DT1s
	BuildTileLookupFromEditor();

	CenterCamera();

	engine->Print(PRIORITY_MESSAGE, "MapRenderer: Ready - %dx%d, Act %d, %d tile keys, editor ds1=%d",
		m_mapWidth, m_mapHeight, m_act, (int)m_tileLookup.size(), editorDs1);
#endif

	return true;
}

const TileEntry *MapRenderer::FindTile(long orientation, long mainIndex, long subIndex)
{
	TileKey key = {orientation, mainIndex, subIndex};
	auto it = m_tileLookup.find(key);
	if (it == m_tileLookup.end() || it->second.empty())
		return nullptr;
	return &it->second[0];
}

#ifdef USE_ALLEGRO5
void MapRenderer::BuildTileLookupFromEditor()
{
	m_tileLookup.clear();

	// Scan all loaded DT1s in the editor's global array
	for (int d = 0; d < 300; d++) // DT1_MAX = 300
	{
		DT1_S_EXT *dt1 = &glb_dt1[d];
		if (dt1->ds1_usage <= 0 || dt1->bh_buffer == NULL || dt1->block_num <= 0)
			continue;

		BLOCK_S_EXT *blocks = (BLOCK_S_EXT *)dt1->bh_buffer;
		// Block headers are 96 bytes each
		unsigned char *blockBase = (unsigned char *)dt1->bh_buffer;

		for (long b = 0; b < dt1->block_num; b++)
		{
			// Read block header fields at correct offsets (96 bytes per block)
			unsigned char *bh = blockBase + (b * 96);
			long orientation = *(long *)(bh + 20);
			long main_index = *(long *)(bh + 24);
			long sub_index = *(long *)(bh + 28);

			TileKey key = {orientation, main_index, sub_index};
			TileEntry entry;
			entry.dt1Handle = (handle)d; // Store DT1 index
			entry.blockIndex = (int)b;
			m_tileLookup[key].push_back(entry);
		}
	}
}

ALLEGRO_BITMAP *MapRenderer::DecodeTileBitmap(handle dt1Handle, int blockIndex, int act)
{
	int dt1Idx = (int)(intptr_t)dt1Handle;
	uint64_t cacheKey = (uint64_t)dt1Idx * 100000 + blockIndex;

	auto it = m_tileCache.find(cacheKey);
	if (it != m_tileCache.end())
		return it->second;

	// Get the pre-rendered bitmap from the editor's DT1 data
	DT1_S_EXT *dt1 = &glb_dt1[dt1Idx];
	if (dt1->block_zoom[0] == NULL || blockIndex >= dt1->block_num)
	{
		m_tileCache[cacheKey] = nullptr;
		return nullptr;
	}

	BITMAP_A4 *srcBmp = dt1->block_zoom[0][blockIndex]; // zoom 0 = 1:1
	if (srcBmp == NULL || srcBmp->line == NULL || srcBmp->w == 0 || srcBmp->h == 0)
	{
		m_tileCache[cacheKey] = nullptr;
		return nullptr;
	}

	if (srcBmp->w > 1024 || srcBmp->h > 1024)
	{
		m_tileCache[cacheKey] = nullptr;
		return nullptr;
	}

	// Convert the 8-bit palette-indexed BITMAP_A4 to an ALLEGRO_BITMAP
	D2Palette *pal = engine->PAL_GetPalette(act < 5 ? act : 0);

	ALLEGRO_BITMAP *bmp = al_create_bitmap(srcBmp->w, srcBmp->h);
	if (bmp == nullptr)
	{
		m_tileCache[cacheKey] = nullptr;
		return nullptr;
	}

	ALLEGRO_LOCKED_REGION *lr = al_lock_bitmap(bmp, ALLEGRO_PIXEL_FORMAT_ABGR_8888, ALLEGRO_LOCK_WRITEONLY);
	if (lr)
	{
		for (int y = 0; y < srcBmp->h; y++)
		{
			uint32_t *dst = (uint32_t *)((char *)lr->data + y * lr->pitch);
			unsigned char *src = srcBmp->line[y];
			for (int x = 0; x < srcBmp->w; x++)
			{
				unsigned char idx = src[x];
				if (idx == 0)
				{
					dst[x] = 0x00000000; // transparent
				}
				else if (pal)
				{
					BYTE r = (*pal)[idx][0];
					BYTE g = (*pal)[idx][1];
					BYTE b = (*pal)[idx][2];
					dst[x] = 0xFF000000 | (b << 16) | (g << 8) | r; // ABGR
				}
				else
				{
					dst[x] = 0xFF000000 | (idx << 16) | (idx << 8) | idx;
				}
			}
		}
		al_unlock_bitmap(bmp);
	}

	m_tileCache[cacheKey] = bmp;
	return bmp;
}
#endif

void MapRenderer::CenterCamera()
{
	float centerIsoX = (float)(m_mapWidth / 2 - m_mapHeight / 2) * HALF_W;
	float centerIsoY = (float)(m_mapWidth / 2 + m_mapHeight / 2) * HALF_H;
	m_cameraX = centerIsoX - SCREEN_W / 2.0f;
	m_cameraY = centerIsoY - SCREEN_H / 2.0f;
}

void MapRenderer::ScrollCamera(float dx, float dy)
{
	m_cameraX += dx;
	m_cameraY += dy;
}

bool MapRenderer::HandleKeyDown(DWORD keyCode)
{
	switch (keyCode)
	{
	case B_LEFTARROW:  ScrollCamera(-SCROLL_SPEED * 4, 0); return true;
	case B_RIGHTARROW: ScrollCamera(SCROLL_SPEED * 4, 0);  return true;
	case B_UPARROW: case 'w': case 'W': ScrollCamera(0, -SCROLL_SPEED * 4); return true;
	case B_DOWNARROW: case 's': case 'S': ScrollCamera(0, SCROLL_SPEED * 4); return true;
	case B_HOME: CenterCamera(); return true;
	}
	return false;
}

void MapRenderer::Draw()
{
#ifdef USE_ALLEGRO5
	engine->renderer->Clear();

	if (m_ds1Handle == INVALID_HANDLE || m_mapWidth == 0 || m_mapHeight == 0)
	{
		engine->renderer->Present();
		return;
	}

	int act = (int)m_act - 1;
	if (act < 0) act = 0;
	if (act > 4) act = 4;

	int dbgDrawn = 0, dbgEmpty = 0, dbgNoEntry = 0;

	// Pass 1: Floor tiles
	for (int ty = 0; ty < m_mapHeight; ty++)
	{
		for (int tx = 0; tx < m_mapWidth; tx++)
		{
			float sx = (float)(tx - ty) * HALF_W - m_cameraX;
			float sy = (float)(tx + ty) * HALF_H - m_cameraY;

			if (sx + TILE_W < 0 || sx > SCREEN_W || sy + TILE_H < 0 || sy > SCREEN_H + 200)
				continue;

			DS1Cell *cell = engine->DS1_GetCellAt(m_ds1Handle, tx, ty, DS1Cell_Floor);
			if (cell == nullptr) continue;
			if (cell->prop1 == 0 && cell->prop2 == 0 && cell->prop3 == 0 && cell->prop4 == 0)
			{
				dbgEmpty++;
				continue;
			}

			long mainIndex = ((cell->prop4 & 0x03) << 4) | (cell->prop3 >> 4);
			long subIndex = cell->prop2;

			const TileEntry *entry = FindTile(0, mainIndex, subIndex);
			if (entry == nullptr)
			{
				dbgNoEntry++;
				continue;
			}

			ALLEGRO_BITMAP *tileBmp = DecodeTileBitmap(entry->dt1Handle, entry->blockIndex, act);
			if (tileBmp)
			{
				al_draw_bitmap(tileBmp, sx, sy, 0);
				dbgDrawn++;

				// DEBUG: draw green outline at each drawn tile so we can see positioning
				al_draw_rectangle(sx, sy, sx + al_get_bitmap_width(tileBmp),
					sy + al_get_bitmap_height(tileBmp),
					al_map_rgba(0, 255, 0, 100), 1.0f);
			}
		}
	}

	// Pass 2: Wall tiles
	for (int ty = 0; ty < m_mapHeight; ty++)
	{
		for (int tx = 0; tx < m_mapWidth; tx++)
		{
			float sx = (float)(tx - ty) * HALF_W - m_cameraX;
			float sy = (float)(tx + ty) * HALF_H - m_cameraY;

			if (sx + TILE_W < 0 || sx > SCREEN_W || sy + 400 < 0 || sy > SCREEN_H + 200)
				continue;

			DS1Cell *cell = engine->DS1_GetCellAt(m_ds1Handle, tx, ty, DS1Cell_Wall);
			if (cell == nullptr) continue;
			if (cell->prop1 == 0 && cell->prop2 == 0 && cell->prop3 == 0 && cell->prop4 == 0)
				continue;

			long orientation = cell->orientation;
			if (orientation == 0) continue;

			long mainIndex = ((cell->prop4 & 0x03) << 4) | (cell->prop3 >> 4);
			long subIndex = cell->prop2;

			const TileEntry *entry = FindTile(orientation, mainIndex, subIndex);
			if (entry == nullptr) continue;

			ALLEGRO_BITMAP *tileBmp = DecodeTileBitmap(entry->dt1Handle, entry->blockIndex, act);
			if (tileBmp)
			{
				int bmpH = al_get_bitmap_height(tileBmp);
				float wallY = sy + TILE_H - (float)bmpH;
				al_draw_bitmap(tileBmp, sx, wallY, 0);
				dbgDrawn++;
			}
		}
	}

	// HUD
	EnsureFont();
	if (s_pFont)
	{
		al_draw_filled_rectangle(0, 0, (float)SCREEN_W, 30, al_map_rgba(0, 0, 0, 180));

		char info[512];
		snprintf(info, sizeof(info),
			"Map: %dx%d | Act %d | Tiles: %d | Drawn: %d Empty: %d NoEntry: %d | Cam: %.0f,%.0f | Esc: Back",
			m_mapWidth, m_mapHeight, m_act,
			(int)m_tileLookup.size(), dbgDrawn, dbgEmpty, dbgNoEntry,
			m_cameraX, m_cameraY);
		al_draw_text(s_pFont, al_map_rgb(200, 180, 120), 10, 8, ALLEGRO_ALIGN_LEFT, info);
	}

	engine->renderer->Present();
#endif
}
