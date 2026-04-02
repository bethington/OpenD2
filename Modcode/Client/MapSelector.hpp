#pragma once
#include "D2Client.hpp"
#include <vector>
#include <string>

// Forward declare for text rendering
#ifdef USE_ALLEGRO5
class Renderer_Allegro;
#endif

/*
 *	MapSelector - DS1 file browser with scrollable list and preview
 *	Launched when +mapviewer command-line flag is set.
 *	Scans the data directory for .ds1 files, groups by Act,
 *	and allows the user to select one to load in MapPreviewer mode.
 */

struct DS1FileEntry
{
	std::string fullPath;		// Full path on disk
	std::string relativePath;	// Path relative to tiles/ directory
	std::string displayName;	// Filename only (for display)
	std::string actGroup;		// Act grouping (ACT1, ACT2, etc.)
};

class MapSelector
{
public:
	MapSelector();
	~MapSelector();

	// Scan a directory for .ds1 files
	void ScanDirectory(const char *szBasePath);

	// UI interaction
	void Draw();
	bool HandleKeyDown(DWORD keyCode);
	bool HandleMouseDown(DWORD x, DWORD y);
	void HandleMouseMove(DWORD x, DWORD y);
	void HandleMouseWheel(int delta);

	// State
	bool IsActive() const { return m_bActive; }
	const char *GetSelectedPath() const;
	int GetSelectedIndex() const { return m_selectedIndex; }
	bool HasSelection() const { return m_bSelectionMade; }

private:
	void DrawFileList();
	void DrawPreview();
	void DrawHeader();
	void LoadPreview(int index);
	void UnloadPreview();

	std::vector<DS1FileEntry> m_files;
	int m_selectedIndex;
	int m_scrollOffset;
	int m_visibleRows;
	int m_hoverIndex;
	DWORD m_mouseX, m_mouseY;
	bool m_bActive;
	bool m_bSelectionMade;

	// Preview state
	handle m_previewDS1;
	int32_t m_previewWidth;
	int32_t m_previewHeight;
	int m_lastPreviewIndex;

	// Layout constants (1280x720)
	static const int SCREEN_W = 1280;
	static const int SCREEN_H = 720;
	static const int LIST_X = 10;
	static const int LIST_Y = 50;
	static const int LIST_W = 480;
	static const int ROW_H = 18;
	static const int PREVIEW_X = 510;
	static const int PREVIEW_Y = 50;
	static const int PREVIEW_W = 750;
	static const int PREVIEW_H = 620;
};

// Global accessor
extern MapSelector *gpMapSelector;
