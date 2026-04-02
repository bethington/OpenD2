#pragma once
#include "D2Client.hpp"
#include <map>
#include <vector>
#include <string>

#ifdef USE_ALLEGRO5
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#endif

/*
 *	MapRenderer - Renders DS1 maps using real DT1 tile graphics
 *	Loads DS1 file, extracts DT1 references, decodes tile blocks,
 *	converts palette-indexed pixels to RGBA, and renders isometric tiles.
 */

struct TileKey
{
	long orientation;
	long mainIndex;
	long subIndex;
	bool operator<(const TileKey &o) const
	{
		if (orientation != o.orientation) return orientation < o.orientation;
		if (mainIndex != o.mainIndex) return mainIndex < o.mainIndex;
		return subIndex < o.subIndex;
	}
};

struct TileEntry
{
	handle dt1Handle;
	int blockIndex;
};

class MapRenderer
{
public:
	MapRenderer();
	~MapRenderer();

	// Load a DS1 map by relative path (e.g., "ACT1\\BARRACKS\\barE.ds1")
	bool LoadMap(const char *relativePath);
	void UnloadMap();

	// Rendering
	void Draw();

	// Camera control
	void ScrollCamera(float dx, float dy);
	void CenterCamera();

	// Input
	bool HandleKeyDown(DWORD keyCode);

	bool IsLoaded() const { return m_ds1Handle != INVALID_HANDLE; }

private:
	void LoadDT1sFromDS1();
	void BuildTileLookup();
	void BuildTileLookupFromEditor();
	const TileEntry *FindTile(long orientation, long mainIndex, long subIndex);

#ifdef USE_ALLEGRO5
	ALLEGRO_BITMAP *DecodeTileBitmap(handle dt1, int blockIndex, int act);
#endif

	// Map state
	handle m_ds1Handle;
	int32_t m_mapWidth;
	int32_t m_mapHeight;
	DWORD m_act;

	// DT1 state
	std::vector<handle> m_loadedDT1s;
	std::map<TileKey, std::vector<TileEntry>> m_tileLookup;

	// Tile bitmap cache: (dt1Handle * 100000 + blockIndex) -> bitmap
#ifdef USE_ALLEGRO5
	std::map<uint64_t, ALLEGRO_BITMAP *> m_tileCache;
#endif

	// Camera
	float m_cameraX;
	float m_cameraY;

	// Constants
	static const int TILE_W = 160;
	static const int TILE_H = 80;
	static const int HALF_W = 80;
	static const int HALF_H = 40;
	static const int SCREEN_W = 1280;
	static const int SCREEN_H = 720;
	static constexpr float SCROLL_SPEED = 16.0f;
};

extern MapRenderer *gpMapRenderer;
