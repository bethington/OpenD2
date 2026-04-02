#include "MapSelector.hpp"
#include "D2Client.hpp"
#include <algorithm>
#include <cstring>

#ifdef USE_ALLEGRO5
#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

// Local font for MapSelector text rendering
// (D2Client.dll can't call game.exe's Renderer_Allegro methods directly)
static ALLEGRO_FONT *s_pMapSelectorFont = nullptr;

static void EnsureFont()
{
	if (s_pMapSelectorFont == nullptr)
	{
		s_pMapSelectorFont = al_create_builtin_font();
	}
}

static void DrawAlText(float x, float y, float r, float g, float b, float a, const char *text)
{
	EnsureFont();
	if (s_pMapSelectorFont && text)
	{
		al_draw_text(s_pMapSelectorFont, al_map_rgba_f(r, g, b, a),
			x, y, ALLEGRO_ALIGN_LEFT, text);
	}
}

static void DrawAlTextF(float x, float y, float r, float g, float b, float a, const char *fmt, ...)
{
	EnsureFont();
	if (s_pMapSelectorFont && fmt)
	{
		char buffer[512];
		va_list args;
		va_start(args, fmt);
		vsnprintf(buffer, sizeof(buffer), fmt, args);
		va_end(args);
		al_draw_text(s_pMapSelectorFont, al_map_rgba_f(r, g, b, a),
			x, y, ALLEGRO_ALIGN_LEFT, buffer);
	}
}
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

MapSelector *gpMapSelector = nullptr;

MapSelector::MapSelector()
	: m_selectedIndex(0), m_scrollOffset(0), m_visibleRows(35),
	  m_hoverIndex(-1), m_mouseX(0), m_mouseY(0),
	  m_bActive(true), m_bSelectionMade(false),
	  m_previewDS1(INVALID_HANDLE), m_previewWidth(0), m_previewHeight(0),
	  m_lastPreviewIndex(-1)
{
}

MapSelector::~MapSelector()
{
	UnloadPreview();
}

/*
 *	Recursively scan a directory for .ds1 files
 */
static void ScanDirRecursive(const std::string &dir, const std::string &relBase,
							  std::vector<DS1FileEntry> &out)
{
#ifdef _WIN32
	std::string searchPath = dir + "\\*";
	WIN32_FIND_DATAA fd;
	HANDLE hFind = FindFirstFileA(searchPath.c_str(), &fd);
	if (hFind == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if (fd.cFileName[0] == '.')
			continue;

		std::string fullPath = dir + "\\" + fd.cFileName;
		std::string relPath = relBase.empty() ? fd.cFileName : relBase + "\\" + fd.cFileName;

		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			ScanDirRecursive(fullPath, relPath, out);
		}
		else
		{
			// Check .ds1 extension
			size_t len = strlen(fd.cFileName);
			if (len > 4)
			{
				const char *ext = fd.cFileName + len - 4;
				if (_stricmp(ext, ".ds1") == 0)
				{
					DS1FileEntry entry;
					entry.fullPath = fullPath;
					entry.relativePath = relPath;
					entry.displayName = fd.cFileName;

					// Extract act group from the relative path
					if (relPath.size() > 4 && (relPath[0] == 'A' || relPath[0] == 'a'))
					{
						size_t slash = relPath.find('\\');
						if (slash == std::string::npos)
							slash = relPath.find('/');
						if (slash != std::string::npos)
							entry.actGroup = relPath.substr(0, slash);
						else
							entry.actGroup = "Other";
					}
					else if (relPath.size() > 3 && (relPath[0] == 'e' || relPath[0] == 'E'))
					{
						entry.actGroup = "ACT5 (Expansion)";
					}
					else
					{
						size_t slash = relPath.find('\\');
						if (slash == std::string::npos)
							slash = relPath.find('/');
						if (slash != std::string::npos)
							entry.actGroup = relPath.substr(0, slash);
						else
							entry.actGroup = "Other";
					}

					out.push_back(entry);
				}
			}
		}
	} while (FindNextFileA(hFind, &fd));

	FindClose(hFind);
#else
	DIR *d = opendir(dir.c_str());
	if (!d)
		return;

	struct dirent *ent;
	while ((ent = readdir(d)) != nullptr)
	{
		if (ent->d_name[0] == '.')
			continue;

		std::string fullPath = dir + "/" + ent->d_name;
		std::string relPath = relBase.empty() ? ent->d_name : relBase + "/" + ent->d_name;

		struct stat st;
		if (stat(fullPath.c_str(), &st) != 0)
			continue;

		if (S_ISDIR(st.st_mode))
		{
			ScanDirRecursive(fullPath, relPath, out);
		}
		else
		{
			size_t len = strlen(ent->d_name);
			if (len > 4)
			{
				const char *ext = ent->d_name + len - 4;
				if (strcasecmp(ext, ".ds1") == 0)
				{
					DS1FileEntry entry;
					entry.fullPath = fullPath;
					entry.relativePath = relPath;
					entry.displayName = ent->d_name;

					size_t slash = relPath.find('/');
					if (slash != std::string::npos)
						entry.actGroup = relPath.substr(0, slash);
					else
						entry.actGroup = "Other";

					out.push_back(entry);
				}
			}
		}
	}
	closedir(d);
#endif
}

void MapSelector::ScanDirectory(const char *szBasePath)
{
	m_files.clear();

	// Build the tiles directory path
	char tilesPath[MAX_D2PATH_ABSOLUTE];
	snprintf(tilesPath, sizeof(tilesPath), "%sdata/global/tiles", szBasePath);

	// Normalize path separators
	for (char *p = tilesPath; *p; p++)
	{
		if (*p == '/')
			*p = '\\';
	}

	engine->Print(PRIORITY_MESSAGE, "MapSelector: Scanning %s for .ds1 files...", tilesPath);

	ScanDirRecursive(tilesPath, "", m_files);

	// Sort by act group then by relative path
	std::sort(m_files.begin(), m_files.end(),
		[](const DS1FileEntry &a, const DS1FileEntry &b)
		{
			if (a.actGroup != b.actGroup)
				return a.actGroup < b.actGroup;
			return a.relativePath < b.relativePath;
		});

	engine->Print(PRIORITY_MESSAGE, "MapSelector: Found %d .ds1 files", (int)m_files.size());

	m_selectedIndex = 0;
	m_scrollOffset = 0;
}

const char *MapSelector::GetSelectedPath() const
{
	if (m_selectedIndex >= 0 && m_selectedIndex < (int)m_files.size())
	{
		return m_files[m_selectedIndex].relativePath.c_str();
	}
	return nullptr;
}

void MapSelector::LoadPreview(int index)
{
	if (index == m_lastPreviewIndex)
		return;

	UnloadPreview();
	m_lastPreviewIndex = index;

	if (index < 0 || index >= (int)m_files.size())
		return;

	// Load the DS1 to get its dimensions
	char ds1Path[MAX_D2PATH];
	snprintf(ds1Path, MAX_D2PATH, "data\\global\\tiles\\%s", m_files[index].relativePath.c_str());

	// Normalize separators
	for (char *p = ds1Path; *p; p++)
	{
		if (*p == '/')
			*p = '\\';
	}

	m_previewDS1 = engine->DS1_Load(ds1Path);
	if (m_previewDS1 != INVALID_HANDLE)
	{
		engine->DS1_GetSize(m_previewDS1, m_previewWidth, m_previewHeight);
	}
}

void MapSelector::UnloadPreview()
{
	// DS1 handles are managed by the engine's hash map
	m_previewDS1 = INVALID_HANDLE;
	m_previewWidth = 0;
	m_previewHeight = 0;
	m_lastPreviewIndex = -1;
}

bool MapSelector::HandleKeyDown(DWORD keyCode)
{
	if (!m_bActive)
		return false;

	int numFiles = (int)m_files.size();
	if (numFiles == 0)
		return false;

	switch (keyCode)
	{
	case B_UPARROW:
		if (m_selectedIndex > 0)
		{
			m_selectedIndex--;
			if (m_selectedIndex < m_scrollOffset)
				m_scrollOffset = m_selectedIndex;
			LoadPreview(m_selectedIndex);
		}
		return true;

	case B_DOWNARROW:
		if (m_selectedIndex < numFiles - 1)
		{
			m_selectedIndex++;
			if (m_selectedIndex >= m_scrollOffset + m_visibleRows)
				m_scrollOffset = m_selectedIndex - m_visibleRows + 1;
			LoadPreview(m_selectedIndex);
		}
		return true;

	case B_PAGEUP:
		m_selectedIndex -= m_visibleRows;
		if (m_selectedIndex < 0)
			m_selectedIndex = 0;
		m_scrollOffset = m_selectedIndex;
		LoadPreview(m_selectedIndex);
		return true;

	case B_PAGEDOWN:
		m_selectedIndex += m_visibleRows;
		if (m_selectedIndex >= numFiles)
			m_selectedIndex = numFiles - 1;
		if (m_selectedIndex >= m_scrollOffset + m_visibleRows)
			m_scrollOffset = m_selectedIndex - m_visibleRows + 1;
		LoadPreview(m_selectedIndex);
		return true;

	case B_HOME:
		m_selectedIndex = 0;
		m_scrollOffset = 0;
		LoadPreview(m_selectedIndex);
		return true;

	case B_END:
		m_selectedIndex = numFiles - 1;
		m_scrollOffset = numFiles - m_visibleRows;
		if (m_scrollOffset < 0)
			m_scrollOffset = 0;
		LoadPreview(m_selectedIndex);
		return true;

	case '\r': // Enter
		m_bSelectionMade = true;
		m_bActive = false;
		return true;

	case 27: // Escape
		m_bActive = false;
		m_bSelectionMade = false;
		return true;
	}

	return false;
}

bool MapSelector::HandleMouseDown(DWORD x, DWORD y)
{
	if (!m_bActive)
		return false;

	// Check if click is in the file list area
	if ((int)x >= LIST_X && (int)x <= LIST_X + LIST_W &&
		(int)y >= LIST_Y && (int)y <= LIST_Y + m_visibleRows * ROW_H)
	{
		int clickedRow = ((int)y - LIST_Y) / ROW_H;
		int clickedIndex = m_scrollOffset + clickedRow;
		if (clickedIndex >= 0 && clickedIndex < (int)m_files.size())
		{
			if (m_selectedIndex == clickedIndex)
			{
				// Double-click behavior: select and confirm
				m_bSelectionMade = true;
				m_bActive = false;
			}
			else
			{
				m_selectedIndex = clickedIndex;
				LoadPreview(m_selectedIndex);
			}
		}
		return true;
	}

	return false;
}

void MapSelector::HandleMouseMove(DWORD x, DWORD y)
{
	m_mouseX = x;
	m_mouseY = y;

	// Update hover index if mouse is over the file list
	if ((int)x >= LIST_X && (int)x <= LIST_X + LIST_W &&
		(int)y >= LIST_Y && (int)y <= LIST_Y + m_visibleRows * ROW_H)
	{
		int hoveredRow = ((int)y - LIST_Y) / ROW_H;
		int hoveredIndex = m_scrollOffset + hoveredRow;
		if (hoveredIndex >= 0 && hoveredIndex < (int)m_files.size())
			m_hoverIndex = hoveredIndex;
		else
			m_hoverIndex = -1;
	}
	else
	{
		m_hoverIndex = -1;
	}
}

void MapSelector::HandleMouseWheel(int delta)
{
	if (!m_bActive)
		return;

	int numFiles = (int)m_files.size();
	if (numFiles <= m_visibleRows)
		return;

	// Scroll 3 rows per wheel tick
	m_scrollOffset -= delta * 3;

	if (m_scrollOffset < 0)
		m_scrollOffset = 0;
	if (m_scrollOffset > numFiles - m_visibleRows)
		m_scrollOffset = numFiles - m_visibleRows;

	// Keep selection visible
	if (m_selectedIndex < m_scrollOffset)
		m_selectedIndex = m_scrollOffset;
	if (m_selectedIndex >= m_scrollOffset + m_visibleRows)
		m_selectedIndex = m_scrollOffset + m_visibleRows - 1;

	LoadPreview(m_selectedIndex);
}

void MapSelector::DrawHeader()
{
	// Draw title bar
	float titleBg[] = {0.15f, 0.12f, 0.08f, 1.0f};
	float titleBorder[] = {0.6f, 0.5f, 0.3f, 1.0f};
	engine->renderer->DrawRectangle(0, 0, (float)SCREEN_W, 40, 0, nullptr, titleBg);
	engine->renderer->DrawRectangle(0, 38, (float)SCREEN_W, 2, 0, nullptr, titleBorder);

	// Status bar at bottom
	float countBg[] = {0.1f, 0.1f, 0.1f, 1.0f};
	engine->renderer->DrawRectangle(0, (float)(SCREEN_H - 40), (float)SCREEN_W, 40, 0, nullptr, countBg);

#ifdef USE_ALLEGRO5

	DrawAlText(10, 12, 0.9f, 0.8f, 0.5f, 1.0f, "OpenD2 Map Viewer - DS1 File Browser");
	DrawAlTextF(10, (float)(SCREEN_H - 30), 0.6f, 0.6f, 0.6f, 1.0f,
		"%d files | Up/Down: Navigate | Enter: Load | Esc: Exit | PgUp/PgDn: Scroll",
		(int)m_files.size());

	// Show selected file info
	if (m_selectedIndex >= 0 && m_selectedIndex < (int)m_files.size())
	{
		DrawAlTextF(400, 12, 0.7f, 0.7f, 0.7f, 1.0f, "[%d/%d]",
			m_selectedIndex + 1, (int)m_files.size());

		if (m_previewDS1 != INVALID_HANDLE)
		{
			DrawAlTextF(500, 12, 0.5f, 0.7f, 0.5f, 1.0f, "Size: %dx%d",
				m_previewWidth, m_previewHeight);
		}
	}
#endif
}

void MapSelector::DrawFileList()
{
	int numFiles = (int)m_files.size();
	if (numFiles == 0)
		return;

	// Draw list background
	float listBg[] = {0.08f, 0.08f, 0.08f, 1.0f};
	engine->renderer->DrawRectangle((float)LIST_X - 2, (float)LIST_Y - 2,
		(float)LIST_W + 4, (float)(m_visibleRows * ROW_H) + 4, 0, nullptr, listBg);

#ifdef USE_ALLEGRO5

#endif

	std::string lastGroup;

	for (int i = 0; i < m_visibleRows && (m_scrollOffset + i) < numFiles; i++)
	{
		int fileIndex = m_scrollOffset + i;
		const DS1FileEntry &entry = m_files[fileIndex];

		float y = (float)(LIST_Y + i * ROW_H);

		// Draw group header if act changed
		if (entry.actGroup != lastGroup)
		{
			float groupBg[] = {0.12f, 0.1f, 0.06f, 1.0f};
			engine->renderer->DrawRectangle((float)LIST_X, y, (float)LIST_W, (float)ROW_H, 0, nullptr, groupBg);

#ifdef USE_ALLEGRO5
			DrawAlText((float)LIST_X + 4, y + 3, 0.8f, 0.65f, 0.3f, 1.0f, entry.actGroup.c_str());
#endif
			lastGroup = entry.actGroup;

			// This row is used for the group header, advance
			i++;
			if (i >= m_visibleRows)
				break;
			y = (float)(LIST_Y + i * ROW_H);
		}

		// Highlight hovered row
		if (fileIndex == m_hoverIndex && fileIndex != m_selectedIndex)
		{
			float hoverColor[] = {0.15f, 0.13f, 0.08f, 1.0f};
			engine->renderer->DrawRectangle((float)LIST_X, y, (float)LIST_W, (float)ROW_H, 0, nullptr, hoverColor);
		}

		// Highlight selected row
		if (fileIndex == m_selectedIndex)
		{
			float selColor[] = {0.25f, 0.2f, 0.1f, 1.0f};
			engine->renderer->DrawRectangle((float)LIST_X, y, (float)LIST_W, (float)ROW_H, 0, nullptr, selColor);

			// Selection indicator bar
			float barColor[] = {0.8f, 0.65f, 0.3f, 1.0f};
			engine->renderer->DrawRectangle((float)LIST_X, y, 3, (float)ROW_H, 0, nullptr, barColor);
		}

#ifdef USE_ALLEGRO5
		// Draw the filename with text
		if (fileIndex == m_selectedIndex)
		{
			DrawAlText((float)LIST_X + 8, y + 3, 1.0f, 0.9f, 0.6f, 1.0f,
				entry.relativePath.c_str());
		}
		else
		{
			DrawAlText((float)LIST_X + 8, y + 3, 0.65f, 0.65f, 0.65f, 0.9f,
				entry.relativePath.c_str());
		}
#endif
	}

	// Draw scrollbar
	if (numFiles > m_visibleRows)
	{
		float scrollBg[] = {0.15f, 0.15f, 0.15f, 1.0f};
		float scrollTrackH = (float)(m_visibleRows * ROW_H);
		engine->renderer->DrawRectangle((float)(LIST_X + LIST_W - 8), (float)LIST_Y,
			8, scrollTrackH, 0, nullptr, scrollBg);

		float thumbRatio = (float)m_visibleRows / (float)numFiles;
		float thumbH = scrollTrackH * thumbRatio;
		if (thumbH < 20)
			thumbH = 20;
		float thumbY = (float)LIST_Y + (scrollTrackH - thumbH) * ((float)m_scrollOffset / (float)(numFiles - m_visibleRows));

		float thumbColor[] = {0.5f, 0.4f, 0.25f, 1.0f};
		engine->renderer->DrawRectangle((float)(LIST_X + LIST_W - 8), thumbY,
			8, thumbH, 0, nullptr, thumbColor);
	}
}

void MapSelector::DrawPreview()
{
	// Draw preview area background
	float prevBg[] = {0.05f, 0.05f, 0.05f, 1.0f};
	float prevBorder[] = {0.3f, 0.25f, 0.15f, 1.0f};
	engine->renderer->DrawRectangle((float)PREVIEW_X, (float)PREVIEW_Y,
		(float)PREVIEW_W, (float)PREVIEW_H, 0, nullptr, prevBg);
	engine->renderer->DrawRectangle((float)PREVIEW_X, (float)PREVIEW_Y,
		(float)PREVIEW_W, (float)PREVIEW_H, 1.5f, prevBorder, nullptr);

	if (m_previewDS1 != INVALID_HANDLE && m_previewWidth > 0 && m_previewHeight > 0)
	{
		// Draw a miniature isometric grid preview of the DS1
		// Scale to fit in the preview area
		float tileW = 160.0f;
		float tileH = 80.0f;

		// Calculate the isometric bounding box
		float isoW = (float)(m_previewWidth + m_previewHeight) * (tileW / 2.0f);
		float isoH = (float)(m_previewWidth + m_previewHeight) * (tileH / 2.0f);

		float scale = 1.0f;
		if (isoW > 0 && isoH > 0)
		{
			float scaleX = (float)(PREVIEW_W - 20) / isoW;
			float scaleY = (float)(PREVIEW_H - 40) / isoH;
			scale = (scaleX < scaleY) ? scaleX : scaleY;
		}

		float halfW = tileW * scale / 2.0f;
		float halfH = tileH * scale / 2.0f;

		// Center the preview
		float offsetX = (float)PREVIEW_X + (float)PREVIEW_W / 2.0f;
		float offsetY = (float)PREVIEW_Y + 20.0f + (float)(m_previewHeight) * halfH;

		// Draw floor cells as colored diamonds
		for (int ty = 0; ty < m_previewHeight; ty++)
		{
			for (int tx = 0; tx < m_previewWidth; tx++)
			{
				DS1Cell *cell = engine->DS1_GetCellAt(m_previewDS1, tx, ty, DS1Cell_Floor);
				if (cell == nullptr)
					continue;
				if (cell->prop1 == 0 && cell->prop2 == 0 && cell->prop3 == 0 && cell->prop4 == 0)
					continue;

				float sx = offsetX + (float)(tx - ty) * halfW;
				float sy = offsetY + (float)(tx + ty) * halfH;

				// Color based on tile properties
				long mainIdx = ((cell->prop4 & 0x03) << 4) | (cell->prop3 >> 4);
				float r = ((mainIdx * 37) % 128 + 80) / 255.0f;
				float g = ((mainIdx * 53 + cell->prop2 * 17) % 128 + 80) / 255.0f;
				float b = ((cell->prop2 * 71) % 100 + 60) / 255.0f;
				float tileColor[] = {r, g, b, 0.85f};

				// Draw small diamond (approximated as small rect at this scale)
				float dw = halfW * 1.6f;
				float dh = halfH * 1.6f;
				if (dw < 2.0f) dw = 2.0f;
				if (dh < 2.0f) dh = 2.0f;
				engine->renderer->DrawRectangle(sx - dw / 2.0f, sy - dh / 2.0f,
					dw, dh, 0, nullptr, tileColor);
			}
		}

		// Draw wall cells as darker marks
		for (int ty = 0; ty < m_previewHeight; ty++)
		{
			for (int tx = 0; tx < m_previewWidth; tx++)
			{
				DS1Cell *cell = engine->DS1_GetCellAt(m_previewDS1, tx, ty, DS1Cell_Wall);
				if (cell == nullptr)
					continue;
				if (cell->prop1 == 0 && cell->prop2 == 0 && cell->prop3 == 0 && cell->prop4 == 0)
					continue;
				if (cell->orientation == 0)
					continue;

				float sx = offsetX + (float)(tx - ty) * halfW;
				float sy = offsetY + (float)(tx + ty) * halfH;

				float wallColor[] = {0.4f, 0.3f, 0.2f, 0.9f};
				float dw = halfW * 0.8f;
				if (dw < 1.5f) dw = 1.5f;
				engine->renderer->DrawRectangle(sx - dw / 2.0f, sy - halfH, dw, halfH * 1.5f,
					0, nullptr, wallColor);
			}
		}

		// Draw map info
		float infoBg[] = {0.1f, 0.1f, 0.1f, 0.8f};
		engine->renderer->DrawRectangle((float)PREVIEW_X + 5, (float)(PREVIEW_Y + PREVIEW_H - 30),
			(float)PREVIEW_W - 10, 25, 0, nullptr, infoBg);

#ifdef USE_ALLEGRO5
	
		DrawAlTextF((float)PREVIEW_X + 10, (float)(PREVIEW_Y + PREVIEW_H - 25),
			0.7f, 0.7f, 0.7f, 1.0f, "Map Size: %d x %d tiles",
			m_previewWidth, m_previewHeight);
#endif
	}
	else
	{
#ifdef USE_ALLEGRO5
		// No preview loaded - show instruction text
	
		DrawAlText((float)PREVIEW_X + 50, (float)(PREVIEW_Y + PREVIEW_H / 2 - 8),
			0.4f, 0.4f, 0.4f, 1.0f, "Select a .ds1 file to preview");
#endif
	}
}

void MapSelector::Draw()
{
	if (!m_bActive)
		return;

	// Clear screen
	engine->renderer->Clear();

	// Draw background
	float bg[] = {0.02f, 0.02f, 0.02f, 1.0f};
	engine->renderer->DrawRectangle(0, 0, (float)SCREEN_W, (float)SCREEN_H, 0, nullptr, bg);

	DrawHeader();
	DrawFileList();
	DrawPreview();

	// Load preview on first draw if not loaded yet
	if (m_lastPreviewIndex < 0 && !m_files.empty())
	{
		LoadPreview(0);
	}

	engine->renderer->Present();
}
