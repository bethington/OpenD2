#include "Renderer_Allegro.hpp"
#include "GraphicsManager.hpp"
#include "Palette.hpp"
#include "DCC.hpp"
#include "Logging.hpp"
#include "imgui.h"
#include "imgui_impl_allegro5.h"
#include <cstring>
#include <cstdio>
#include <cmath>
#include <vector>

// From Input_Allegro.cpp
extern bool g_showImGuiOverlay;

///////////////////////////////////////////////////////
//
//	DCC Runtime Decode Helpers
//

// State for DCC decode callbacks (single-threaded, same pattern as GraphicsManager)
static ALLEGRO_BITMAP *g_dccDecodeBitmap = nullptr;
static D2Palette *g_dccDecodePalette = nullptr;
static std::vector<ALLEGRO_BITMAP *> *g_dccFrameBitmaps = nullptr;

static void *DCC_AllocCallback(unsigned int width, unsigned int height)
{
	g_dccDecodeBitmap = al_create_bitmap(width, height);
	if (g_dccDecodeBitmap)
	{
		ALLEGRO_BITMAP *prev = al_get_target_bitmap();
		al_set_target_bitmap(g_dccDecodeBitmap);
		al_clear_to_color(al_map_rgba(0, 0, 0, 0));
		al_set_target_bitmap(prev);
	}
	return (void *)g_dccDecodeBitmap;
}

static void DCC_DecodeCallback(void *pixels, void *extraData, int32_t frameNum,
	int32_t frameX, int32_t frameY, int32_t frameW, int32_t frameH)
{
	if (!pixels || !g_dccDecodeBitmap || frameW <= 0 || frameH <= 0)
		return;

	BYTE *srcPixels = (BYTE *)pixels;
	D2Palette *pal = g_dccDecodePalette;

	// Create individual frame bitmap
	ALLEGRO_BITMAP *frameBmp = al_create_bitmap(frameW, frameH);
	if (!frameBmp)
		return;

	ALLEGRO_LOCKED_REGION *lr = al_lock_bitmap(frameBmp,
		ALLEGRO_PIXEL_FORMAT_ABGR_8888, ALLEGRO_LOCK_WRITEONLY);
	if (lr)
	{
		for (int32_t py = 0; py < frameH; py++)
		{
			uint32_t *dst = (uint32_t *)((char *)lr->data + py * lr->pitch);
			for (int32_t px = 0; px < frameW; px++)
			{
				BYTE idx = srcPixels[py * frameW + px];
				if (idx == 0)
				{
					dst[px] = 0x00000000; // transparent
				}
				else if (pal)
				{
					BYTE b = (*pal)[idx][0]; // pal.dat stores BGR
					BYTE g = (*pal)[idx][1];
					BYTE r = (*pal)[idx][2];
					dst[px] = 0xFF000000 | (b << 16) | (g << 8) | r; // ABGR
				}
				else
				{
					dst[px] = 0xFF000000 | (idx << 16) | (idx << 8) | idx;
				}
			}
		}
		al_unlock_bitmap(frameBmp);
	}

	if (g_dccFrameBitmaps && frameNum >= 0)
	{
		if (frameNum >= (int32_t)g_dccFrameBitmaps->size())
			g_dccFrameBitmaps->resize(frameNum + 1, nullptr);
		(*g_dccFrameBitmaps)[frameNum] = frameBmp;
	}
	else
	{
		al_destroy_bitmap(frameBmp);
	}
}

///////////////////////////////////////////////////////
//
//	PNG Path Resolution
//

bool Renderer_Allegro::ResolvePNGPath(const char *basePath, const char *dc6Path,
	int direction, int frame, int numDirections, char *outPath, size_t outLen)
{
	if (!basePath || !dc6Path || !outPath)
		return false;

	// Build: basePath + dc6Path (minus .dc6) + /frame.png
	char stripped[1024];
	snprintf(stripped, sizeof(stripped), "%s", dc6Path);

	// Strip .dc6 extension (case-insensitive)
	size_t len = strlen(stripped);
	if (len > 4)
	{
		char *ext = stripped + len - 4;
		if (ext[0] == '.' &&
			(ext[1] == 'd' || ext[1] == 'D') &&
			(ext[2] == 'c' || ext[2] == 'C') &&
			(ext[3] == '6'))
		{
			*ext = '\0';
		}
	}

	if (numDirections <= 1)
		snprintf(outPath, outLen, "%s%s/%d.png", basePath, stripped, frame);
	else
		snprintf(outPath, outLen, "%s%s/d%df%d.png", basePath, stripped, direction, frame);

	// Normalize backslashes to forward slashes (Allegro prefers forward slashes)
	for (char *p = outPath; *p; p++)
	{
		if (*p == '\\')
			*p = '/';
	}

	return true;
}

///////////////////////////////////////////////////////
//
//	Bitmap Cache
//

ALLEGRO_BITMAP *Renderer_Allegro::LoadOrGetBitmap(const char *pngPath)
{
	if (!pngPath)
		return nullptr;

	auto it = m_bitmapCache.find(pngPath);
	if (it != m_bitmapCache.end())
		return it->second;

	ALLEGRO_BITMAP *bmp = al_load_bitmap(pngPath);
	m_bitmapCache[pngPath] = bmp; // cache even if nullptr (avoid repeated load attempts)

	return bmp;
}

///////////////////////////////////////////////////////
//
//	AllegroRenderObject
//

AllegroRenderObject::AllegroRenderObject()
	: m_x(0), m_y(0), m_w(0), m_h(0),
	  m_type(RT_NONE), m_pRenderer(nullptr),
	  m_texture(nullptr), m_compositeBitmap(nullptr),
	  m_animFrames(nullptr), m_animFramesOwned(false), m_animFrameCount(0), m_animCurrentFrame(0),
	  m_lastDrawTime(0), m_animFramerate(25.0f), m_animLoop(true),
	  m_frameOffsetX(nullptr), m_frameOffsetY(nullptr),
	  m_fontRef(nullptr), m_textColor(0),
	  m_textAlignX(0), m_textAlignY(0), m_textAlignW(0), m_textAlignH(0),
	  m_horzAlign(0), m_vertAlign(0),
	  m_colorR(1.0f), m_colorG(1.0f), m_colorB(1.0f), m_colorA(1.0f),
	  m_drawMode(0), m_palshift(0)
{
	memset(m_textBuf, 0, sizeof(m_textBuf));
}

void AllegroRenderObject::Reset()
{
	// Destroy owned resources
	if (m_compositeBitmap)
	{
		al_destroy_bitmap(m_compositeBitmap);
		m_compositeBitmap = nullptr;
	}
	if (m_animFrames)
	{
		if (m_animFramesOwned)
		{
			for (int i = 0; i < m_animFrameCount; i++)
			{
				if (m_animFrames[i])
					al_destroy_bitmap(m_animFrames[i]);
			}
		}
		free(m_animFrames);
		m_animFrames = nullptr;
	}
	m_animFramesOwned = false;
	if (m_frameOffsetX)
	{
		free(m_frameOffsetX);
		m_frameOffsetX = nullptr;
	}
	if (m_frameOffsetY)
	{
		free(m_frameOffsetY);
		m_frameOffsetY = nullptr;
	}

	m_type = RT_NONE;
	m_texture = nullptr;
	m_animFrameCount = 0;
	m_animCurrentFrame = 0;
	m_fontRef = nullptr;
	memset(m_textBuf, 0, sizeof(m_textBuf));
}

///////////////////////////////////////////////////////
//
//	AttachTextureResource — single frame
//

void AllegroRenderObject::AttachTextureResource(IGraphicsReference *ref, int32_t frame)
{
	Reset();
	if (!ref || !m_pRenderer)
		return;

	const char *srcPath = ref->GetSourcePath();
	if (!srcPath)
		return;

	// Resolve PNG path
	char pngPath[1024];
	Renderer_Allegro::ResolvePNGPath(m_pRenderer->GetBasePath(), srcPath, 0, frame, 1, pngPath, sizeof(pngPath));

	m_texture = m_pRenderer->LoadOrGetBitmap(pngPath);
	if (!m_texture)
		return;

	m_type = RT_TEXTURE;

	// Auto-size from bitmap if not set
	if (m_w <= 0)
		m_w = al_get_bitmap_width(m_texture);
	if (m_h <= 0)
		m_h = al_get_bitmap_height(m_texture);
}

///////////////////////////////////////////////////////
//
//	AttachCompositeTextureResource — stitched multi-frame
//

void AllegroRenderObject::AttachCompositeTextureResource(IGraphicsReference *ref, int32_t startFrame, int32_t endFrame)
{
	Reset();
	if (!ref || !m_pRenderer)
		return;

	const char *srcPath = ref->GetSourcePath();
	if (!srcPath)
		return;

	// Get total stitched dimensions
	uint32_t totalW = 0, totalH = 0;
	ref->GetGraphicsInfo(false, startFrame, endFrame, &totalW, &totalH);

	if (totalW == 0 || totalH == 0)
		return;

	// Determine grid layout by scanning frame widths (replicating StitchStats logic)
	int numFrames = (endFrame < 0) ? (int)ref->GetNumberOfFrames() : (endFrame - startFrame + 1);
	int tilesPerRow = 0;
	for (int i = 0; i < numFrames; i++)
	{
		uint32_t fw = 0;
		ref->GetGraphicsData(nullptr, startFrame + i, &fw, nullptr, nullptr, nullptr);
		tilesPerRow++;
		if (fw != 256) // MAX_DC6_CELL_SIZE
			break;
	}
	if (tilesPerRow == 0)
		tilesPerRow = 1;

	// Create the composite bitmap
	ALLEGRO_BITMAP *prevTarget = al_get_target_bitmap();
	m_compositeBitmap = al_create_bitmap(totalW, totalH);
	if (!m_compositeBitmap)
		return;

	al_set_target_bitmap(m_compositeBitmap);
	al_clear_to_color(al_map_rgba(0, 0, 0, 0));

	// Load and blit each frame
	for (int i = 0; i < numFrames; i++)
	{
		int col = i % tilesPerRow;
		int row = i / tilesPerRow;

		// Get frame dimensions for Y offset calculation
		uint32_t fh = 0;
		ref->GetGraphicsData(nullptr, startFrame + i, nullptr, &fh, nullptr, nullptr);

		float blitX = (float)(col * 256);
		float blitY = (float)(row * 255); // 255 matches DC6 stitch offset

		char pngPath[1024];
		Renderer_Allegro::ResolvePNGPath(m_pRenderer->GetBasePath(), srcPath,
			0, startFrame + i, 1, pngPath, sizeof(pngPath));

		ALLEGRO_BITMAP *frameBmp = m_pRenderer->LoadOrGetBitmap(pngPath);
		if (frameBmp)
		{
			al_draw_bitmap(frameBmp, blitX, blitY, 0);
		}
	}

	al_set_target_bitmap(prevTarget);
	m_type = RT_COMPOSITE;

	if (m_w <= 0)
		m_w = (int)totalW;
	if (m_h <= 0)
		m_h = (int)totalH;
}

///////////////////////////////////////////////////////
//
//	AttachAnimationResource — multi-frame cycling
//

void AllegroRenderObject::AttachAnimationResource(IGraphicsReference *ref, bool bResetFrame)
{
	Reset();
	if (!ref || !m_pRenderer)
		return;

	const char *srcPath = ref->GetSourcePath();
	if (!srcPath)
		return;

	int frameCount = (int)ref->GetNumberOfFrames();
	if (frameCount <= 0)
		return;

	// Try PNG path first (pre-converted DC6 files)
	char pngTest[1024];
	Renderer_Allegro::ResolvePNGPath(m_pRenderer->GetBasePath(), srcPath,
		0, 0, 1, pngTest, sizeof(pngTest));

	ALLEGRO_BITMAP *testBmp = m_pRenderer->LoadOrGetBitmap(pngTest);

	if (testBmp)
	{
		// PNG path — load all frames from pre-converted PNGs
		m_animFrames = (ALLEGRO_BITMAP **)calloc(frameCount, sizeof(ALLEGRO_BITMAP *));
		m_frameOffsetX = (int *)calloc(frameCount, sizeof(int));
		m_frameOffsetY = (int *)calloc(frameCount, sizeof(int));

		if (!m_animFrames || !m_frameOffsetX || !m_frameOffsetY)
			return;

		m_animFrameCount = frameCount;
		m_animFrames[0] = testBmp;

		uint32_t fw = 0, fh = 0;
		int32_t offX = 0, offY = 0;
		ref->GetGraphicsData(nullptr, 0, &fw, &fh, &offX, &offY);
		m_frameOffsetX[0] = offX;
		m_frameOffsetY[0] = offY;
		if (m_w <= 0 && testBmp)
		{
			m_w = al_get_bitmap_width(testBmp);
			m_h = al_get_bitmap_height(testBmp);
		}

		for (int i = 1; i < frameCount; i++)
		{
			char pngPath[1024];
			Renderer_Allegro::ResolvePNGPath(m_pRenderer->GetBasePath(), srcPath,
				0, i, 1, pngPath, sizeof(pngPath));
			m_animFrames[i] = m_pRenderer->LoadOrGetBitmap(pngPath);

			ref->GetGraphicsData(nullptr, i, &fw, &fh, &offX, &offY);
			m_frameOffsetX[i] = offX;
			m_frameOffsetY[i] = offY;
		}
	}
	else
	{
		// No PNGs — try runtime DCC decode
		std::vector<ALLEGRO_BITMAP *> decodedFrames;
		g_dccFrameBitmaps = &decodedFrames;
		g_dccDecodePalette = Pal::GetPalette(m_pRenderer->GetGlobalPalette());

		ref->LoadSingleDirection(0, DCC_AllocCallback, DCC_DecodeCallback);

		g_dccFrameBitmaps = nullptr;

		// Clean up the strip bitmap (we have individual frames now)
		if (g_dccDecodeBitmap)
		{
			al_destroy_bitmap(g_dccDecodeBitmap);
			g_dccDecodeBitmap = nullptr;
		}

		int decoded = (int)decodedFrames.size();
		if (decoded <= 0)
			return;

		m_animFrameCount = decoded;
		m_animFramesOwned = true; // DCC-decoded frames are owned, must destroy on reset
		m_animFrames = (ALLEGRO_BITMAP **)calloc(decoded, sizeof(ALLEGRO_BITMAP *));
		m_frameOffsetX = (int *)calloc(decoded, sizeof(int));
		m_frameOffsetY = (int *)calloc(decoded, sizeof(int));

		if (!m_animFrames || !m_frameOffsetX || !m_frameOffsetY)
			return;

		for (int i = 0; i < decoded; i++)
		{
			m_animFrames[i] = decodedFrames[i];

			uint32_t fw = 0, fh = 0;
			int32_t offX = 0, offY = 0;
			ref->GetGraphicsData(nullptr, i, &fw, &fh, &offX, &offY);
			m_frameOffsetX[i] = offX;
			m_frameOffsetY[i] = offY;

			if (i == 0 && m_w <= 0 && m_animFrames[i])
			{
				m_w = al_get_bitmap_width(m_animFrames[i]);
				m_h = al_get_bitmap_height(m_animFrames[i]);
			}
		}
	}

	if (bResetFrame)
	{
		m_animCurrentFrame = 0;
		m_lastDrawTime = al_get_time();
	}

	m_type = RT_ANIMATION;
}

///////////////////////////////////////////////////////
//
//	AttachTokenResource — basic single-layer rendering
//	Loads the torso (TR) DCC for the token's current mode/hitclass
//	and displays as an animation. Full COF multi-layer composition deferred.
//

void AllegroRenderObject::AttachTokenResource(ITokenReference *ref)
{
	Reset();
	if (!ref || !m_pRenderer)
		return;

	// Get the torso graphic for TN (town neutral) / HTH (hand to hand) with "lit" armor
	IGraphicsReference *trGraphic = ref->GetTokenGraphic(COMP_TORSO, WC_HTH, PLRMODE_TN, "lit");
	if (!trGraphic)
		return;

	// Use the DCC runtime decode path via AttachAnimationResource
	// trGraphic is a DCCReference — AttachAnimationResource handles DCC decode
	AttachAnimationResource(trGraphic, true);
	m_animFramerate = 12.0f; // Town neutral is slow idle
}

///////////////////////////////////////////////////////
//
//	AttachFontResource + SetText (temporary: Allegro built-in font)
//

void AllegroRenderObject::AttachFontResource(IGraphicsReference *ref)
{
	m_fontRef = ref;
	m_type = RT_FONT_TEXT;
}

void AllegroRenderObject::SetText(const char16_t *text)
{
	if (text)
	{
		int i = 0;
		for (; text[i] && i < 511; i++)
			m_textBuf[i] = text[i];
		m_textBuf[i] = 0;
	}
	else
	{
		m_textBuf[0] = 0;
	}
}

void AllegroRenderObject::SetTextAlignment(int x, int y, int w, int h, int horzAlignment, int vertAlignment)
{
	m_textAlignX = x;
	m_textAlignY = y;
	m_textAlignW = w;
	m_textAlignH = h;
	m_horzAlign = horzAlignment;
	m_vertAlign = vertAlignment;
}

void AllegroRenderObject::SetTextColor(int color)
{
	m_textColor = color;
}

///////////////////////////////////////////////////////
//
//	Text color mapping (approximate D2 colors)
//

static void GetTextColorRGB(int color, float &r, float &g, float &b)
{
	static const float colors[][3] = {
		{1.0f, 1.0f, 1.0f},		// White
		{1.0f, 0.2f, 0.2f},		// Red
		{0.0f, 1.0f, 0.0f},		// BrightGreen
		{0.4f, 0.4f, 1.0f},		// Blue
		{0.78f, 0.64f, 0.18f},		// Gold
		{0.5f, 0.5f, 0.5f},		// Grey
		{0.0f, 0.0f, 0.0f},		// Black
		{0.78f, 0.64f, 0.18f},		// Unknown7 (gold)
		{1.0f, 0.6f, 0.0f},		// Orange
		{1.0f, 1.0f, 0.0f},		// Yellow
		{0.0f, 0.5f, 0.0f},		// DarkGreen
		{0.6f, 0.2f, 0.8f},		// Purple
		{0.0f, 0.8f, 0.0f},		// MediumGreen
		{1.0f, 1.0f, 1.0f},		// Overwhite
	};

	int idx = color;
	if (idx < 0 || idx > 13)
		idx = 0;

	r = colors[idx][0];
	g = colors[idx][1];
	b = colors[idx][2];
}

///////////////////////////////////////////////////////
//
//	Draw
//

void AllegroRenderObject::Draw()
{
	if (m_type == RT_NONE || !m_pRenderer)
		return;

	// Set blend mode
	if (m_drawMode == 3)
	{
		// Additive blending (fire effects)
		al_set_blender(ALLEGRO_ADD, ALLEGRO_ONE, ALLEGRO_ONE);
	}
	else
	{
		// Standard alpha blending
		al_set_blender(ALLEGRO_ADD, ALLEGRO_ALPHA, ALLEGRO_INVERSE_ALPHA);
	}

	ALLEGRO_COLOR tint = al_map_rgba_f(
		m_colorR * m_colorA,
		m_colorG * m_colorA,
		m_colorB * m_colorA,
		m_colorA);

	switch (m_type)
	{
	case RT_TEXTURE:
		if (m_texture)
		{
			int bmpW = al_get_bitmap_width(m_texture);
			int bmpH = al_get_bitmap_height(m_texture);
			if (m_w > 0 && m_h > 0 && (m_w != bmpW || m_h != bmpH))
			{
				al_draw_tinted_scaled_bitmap(m_texture, tint,
					0, 0, (float)bmpW, (float)bmpH,
					(float)m_x, (float)m_y, (float)m_w, (float)m_h, 0);
			}
			else
			{
				al_draw_tinted_bitmap(m_texture, tint, (float)m_x, (float)m_y, 0);
			}
		}
		break;

	case RT_COMPOSITE:
		if (m_compositeBitmap)
		{
			int bmpW = al_get_bitmap_width(m_compositeBitmap);
			int bmpH = al_get_bitmap_height(m_compositeBitmap);
			if (m_w > 0 && m_h > 0 && (m_w != bmpW || m_h != bmpH))
			{
				al_draw_tinted_scaled_bitmap(m_compositeBitmap, tint,
					0, 0, (float)bmpW, (float)bmpH,
					(float)m_x, (float)m_y, (float)m_w, (float)m_h, 0);
			}
			else
			{
				al_draw_tinted_bitmap(m_compositeBitmap, tint, (float)m_x, (float)m_y, 0);
			}
		}
		break;

	case RT_ANIMATION:
		if (m_animFrames && m_animCurrentFrame < m_animFrameCount)
		{
			ALLEGRO_BITMAP *frame = m_animFrames[m_animCurrentFrame];
			if (frame)
			{
				float drawX = (float)(m_x + m_frameOffsetX[m_animCurrentFrame]);
				float drawY = (float)(m_y + m_frameOffsetY[m_animCurrentFrame]);
				al_draw_tinted_bitmap(frame, tint, drawX, drawY, 0);
			}

			// Advance animation
			double now = al_get_time();
			double delta = now - m_lastDrawTime;
			if (m_lastDrawTime == 0)
				delta = 0;
			m_lastDrawTime = now;

			if (m_animFramerate > 0 && delta > 0)
			{
				double frameDuration = 1.0 / (double)m_animFramerate;
				if (delta >= frameDuration)
				{
					int framesToAdvance = (int)(delta / frameDuration);
					m_animCurrentFrame += framesToAdvance;

					if (m_animCurrentFrame >= m_animFrameCount)
					{
						if (m_animLoop)
							m_animCurrentFrame %= m_animFrameCount;
						else
							m_animCurrentFrame = m_animFrameCount - 1;
					}
				}
			}
		}
		break;

	case RT_FONT_TEXT:
	{
		if (m_textBuf[0] == 0)
			break;

		ALLEGRO_FONT *font = m_pRenderer->GetBuiltinFont();
		if (!font)
			break;

		// Convert char16_t to char for Allegro's built-in font
		char narrowText[512];
		int i = 0;
		for (; m_textBuf[i] && i < 511; i++)
			narrowText[i] = (char)(m_textBuf[i] & 0x7F);
		narrowText[i] = 0;

		float tr, tg, tb;
		GetTextColorRGB(m_textColor, tr, tg, tb);

		// Calculate aligned position
		float drawX = (float)m_x;
		float drawY = (float)m_y;

		if (m_textAlignW > 0)
		{
			int textW = al_get_text_width(font, narrowText);
			if (m_horzAlign == 1) // ALIGN_CENTER
				drawX = (float)m_textAlignX + ((float)m_textAlignW - textW) / 2.0f;
			else if (m_horzAlign == 2) // ALIGN_RIGHT
				drawX = (float)(m_textAlignX + m_textAlignW) - textW;
			else
				drawX = (float)m_textAlignX;
		}

		if (m_textAlignH > 0)
		{
			int fontH = al_get_font_line_height(font);
			// Center vertically within the alignment rect
			drawY = (float)m_textAlignY + ((float)m_textAlignH - fontH) / 2.0f;
		}

		// Draw text with faux-bold (1px offset in X) to match D2's heavier look
		ALLEGRO_COLOR textColor = al_map_rgba_f(tr, tg, tb, m_colorA);
		al_draw_text(font, textColor, drawX, drawY, ALLEGRO_ALIGN_LEFT, narrowText);
		al_draw_text(font, textColor, drawX + 1, drawY, ALLEGRO_ALIGN_LEFT, narrowText);
		break;
	}

	default:
		break;
	}

	// Restore default blend mode
	al_set_blender(ALLEGRO_ADD, ALLEGRO_ALPHA, ALLEGRO_INVERSE_ALPHA);
}

///////////////////////////////////////////////////////
//
//	Property setters
//

void AllegroRenderObject::SetPalshift(BYTE palette)
{
	m_palshift = palette;
}

void AllegroRenderObject::SetDrawCoords(int x, int y, int w, int h)
{
	m_x = x;
	m_y = y;
	if (w > 0) m_w = w;
	if (h > 0) m_h = h;
}

void AllegroRenderObject::GetDrawCoords(int *x, int *y, int *w, int *h)
{
	if (x) *x = m_x;
	if (y) *y = m_y;
	if (w) *w = m_w;
	if (h) *h = m_h;
}

void AllegroRenderObject::SetColorModulate(float r, float g, float b, float a)
{
	m_colorR = r;
	m_colorG = g;
	m_colorB = b;
	m_colorA = a;
}

void AllegroRenderObject::SetDrawMode(int drawMode)
{
	m_drawMode = drawMode;
}

bool AllegroRenderObject::PixelPerfectDetection(int x, int y)
{
	ALLEGRO_BITMAP *bmp = nullptr;

	switch (m_type)
	{
	case RT_TEXTURE:
		bmp = m_texture;
		break;
	case RT_COMPOSITE:
		bmp = m_compositeBitmap;
		break;
	case RT_ANIMATION:
		if (m_animFrames && m_animCurrentFrame < m_animFrameCount)
			bmp = m_animFrames[m_animCurrentFrame];
		break;
	default:
		return false;
	}

	if (!bmp)
		return false;

	int localX = x - m_x;
	int localY = y - m_y;

	if (localX < 0 || localY < 0 ||
		localX >= al_get_bitmap_width(bmp) ||
		localY >= al_get_bitmap_height(bmp))
		return false;

	ALLEGRO_COLOR pixel = al_get_pixel(bmp, localX, localY);
	unsigned char r, g, b, a;
	al_unmap_rgba(pixel, &r, &g, &b, &a);
	return a > 0;
}

void AllegroRenderObject::SetFramerate(int framerate)
{
	m_animFramerate = (float)framerate;
}

void AllegroRenderObject::SetAnimationLoop(bool bLoop)
{
	m_animLoop = bLoop;
}

void AllegroRenderObject::AddAnimationFinishedCallback(void *extraData, AnimationFinishCallback callback)
{
	(void)extraData;
	(void)callback;
	// TODO: implement animation callbacks
}

void AllegroRenderObject::AddAnimationFrameCallback(int32_t frame, void *extraData, AnimationFrameCallback callback)
{
	(void)frame;
	(void)extraData;
	(void)callback;
	// TODO: implement animation callbacks
}

void AllegroRenderObject::RemoveAnimationFinishCallbacks()
{
	// TODO: implement animation callbacks
}

void AllegroRenderObject::SetAnimationDirection(int direction)
{
	(void)direction;
	// TODO: multi-direction animation support
}

void AllegroRenderObject::SetTokenMode(int newMode)
{
	(void)newMode;
}

void AllegroRenderObject::SetTokenArmorLevel(int component, const char *armorLevel)
{
	(void)component;
	(void)armorLevel;
}

void AllegroRenderObject::SetTokenHitClass(int hitclass)
{
	(void)hitclass;
}

///////////////////////////////////////////////////////
//
//	Renderer_Allegro
//

Renderer_Allegro::Renderer_Allegro(D2GameConfigStrc *pConfig, OpenD2ConfigStrc *pOpenConfig, ALLEGRO_DISPLAY *pDisplay)
	: m_pDisplay(pDisplay), m_pBuiltinFont(nullptr), m_currentPalette(PAL_ACT1), m_numAllocated(0)
{
	(void)pConfig;
	memset(m_objectInUse, 0, sizeof(m_objectInUse));
	memset(m_basePath, 0, sizeof(m_basePath));

	// Store basepath for PNG resolution
	if (pOpenConfig && pOpenConfig->szBasePath[0])
	{
		snprintf(m_basePath, sizeof(m_basePath), "%s", pOpenConfig->szBasePath);
		// Ensure trailing slash
		size_t len = strlen(m_basePath);
		if (len > 0 && m_basePath[len - 1] != '/' && m_basePath[len - 1] != '\\')
		{
			m_basePath[len] = '/';
			m_basePath[len + 1] = '\0';
		}
	}

	// Try to load Exocet font (authentic D2 font), fall back to built-in
	char fontPath[1024];
	snprintf(fontPath, sizeof(fontPath), "%sdata/assets/fonts/ExocetBlizzardMedium.otf", m_basePath);
	m_pBuiltinFont = al_load_font(fontPath, 18, 0);
	if (!m_pBuiltinFont)
	{
		Log::Print(PRIORITY_MESSAGE, "Could not load Exocet font from %s, using built-in", fontPath);
		m_pBuiltinFont = al_create_builtin_font();
	}
	else
	{
		Log::Print(PRIORITY_MESSAGE, "Loaded Exocet font from %s", fontPath);
	}

	Log::Print(PRIORITY_MESSAGE, "Renderer_Allegro initialized (basePath: %s)", m_basePath);
}

Renderer_Allegro::~Renderer_Allegro()
{
	// Destroy all cached bitmaps
	for (auto &pair : m_bitmapCache)
	{
		if (pair.second)
			al_destroy_bitmap(pair.second);
	}
	m_bitmapCache.clear();

	// Reset all render objects (frees owned composites)
	for (int i = 0; i < MAX_RENDER_OBJECTS; i++)
	{
		if (m_objectInUse[i])
			m_renderObjects[i].Reset();
	}

	if (m_pBuiltinFont)
	{
		al_destroy_font(m_pBuiltinFont);
		m_pBuiltinFont = nullptr;
	}

	Log::Print(PRIORITY_MESSAGE, "Renderer_Allegro destroyed");
}

void Renderer_Allegro::Present()
{
	// ImGui frame — guard against context not being ready
	ImGuiContext *ctx = ImGui::GetCurrentContext();
	if (!ctx)
	{
		al_flip_display();
		return;
	}

	ImGui_ImplAllegro5_NewFrame();
	ImGui::NewFrame();

	if (g_showImGuiOverlay)
	{
		// Main editor panel
		ImGui::Begin("OpenD2 Editor", &g_showImGuiOverlay, ImGuiWindowFlags_MenuBar);

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("View"))
			{
				static bool showRenderer = true;
				static bool showLayers = true;
				static bool showDemo = false;
				ImGui::MenuItem("Renderer Info", nullptr, &showRenderer);
				ImGui::MenuItem("Layer Controls", nullptr, &showLayers);
				ImGui::MenuItem("ImGui Demo", nullptr, &showDemo);
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		// Renderer info section
		if (ImGui::CollapsingHeader("Renderer", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("Render Objects: %d / %d", m_numAllocated, MAX_RENDER_OBJECTS);
			ImGui::Text("Cached Bitmaps: %d", (int)m_bitmapCache.size());
			ImGui::Text("Palette: %d", (int)m_currentPalette);
			ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
		}

		// Layer visibility controls (placeholder for Phase 8 editor integration)
		if (ImGui::CollapsingHeader("Layer Controls"))
		{
			static bool showFloors = true, showWalls = true, showShadows = true;
			static bool showObjects = true, showPaths = false;
			ImGui::Checkbox("Floors (F1)", &showFloors);
			ImGui::Checkbox("Walls (F2)", &showWalls);
			ImGui::Checkbox("Shadows (F3)", &showShadows);
			ImGui::Checkbox("Objects (F4)", &showObjects);
			ImGui::Checkbox("Paths (F5)", &showPaths);
			ImGui::Text("(Layer toggles not yet wired to renderer)");
		}

		// Tile info (placeholder)
		if (ImGui::CollapsingHeader("Tile Info"))
		{
			ImGui::Text("Click a tile to inspect");
			ImGui::Text("(Tile selection not yet implemented)");
		}

		// Bitmap cache browser
		if (ImGui::CollapsingHeader("Bitmap Cache"))
		{
			ImGui::Text("Total: %d bitmaps", (int)m_bitmapCache.size());
			static char filterBuf[128] = "";
			ImGui::InputText("Filter", filterBuf, sizeof(filterBuf));

			ImGui::BeginChild("CacheList", ImVec2(0, 200), true);
			for (auto &pair : m_bitmapCache)
			{
				if (pair.second)
				{
					if (filterBuf[0] == '\0' || pair.first.find(filterBuf) != std::string::npos)
					{
						ImGui::Text("%s (%dx%d)", pair.first.c_str(),
							al_get_bitmap_width(pair.second), al_get_bitmap_height(pair.second));
					}
				}
			}
			ImGui::EndChild();
		}

		ImGui::End();
	}

	ImGui::Render();
	ImGui_ImplAllegro5_RenderDrawData(ImGui::GetDrawData());

	al_flip_display();
}

void Renderer_Allegro::Clear()
{
	al_set_target_backbuffer(m_pDisplay);
	al_clear_to_color(al_map_rgb(0, 0, 0));
}

void Renderer_Allegro::SetGlobalPalette(const D2Palettes palette)
{
	m_currentPalette = palette;
}

D2Palettes Renderer_Allegro::GetGlobalPalette()
{
	return m_currentPalette;
}

IRenderObject *Renderer_Allegro::AllocateObject(int stage)
{
	(void)stage;
	for (int i = 0; i < MAX_RENDER_OBJECTS; i++)
	{
		if (!m_objectInUse[i])
		{
			m_objectInUse[i] = true;
			m_renderObjects[i] = AllegroRenderObject();
			m_renderObjects[i].m_pRenderer = this;
			m_numAllocated++;
			return &m_renderObjects[i];
		}
	}
	Log::Print(PRIORITY_MESSAGE, "Renderer_Allegro: AllocateObject failed - pool full (%d)", MAX_RENDER_OBJECTS);
	return nullptr;
}

void Renderer_Allegro::Remove(IRenderObject *Object)
{
	if (Object == nullptr)
		return;

	AllegroRenderObject *obj = static_cast<AllegroRenderObject *>(Object);
	int index = (int)(obj - m_renderObjects);

	if (index >= 0 && index < MAX_RENDER_OBJECTS && m_objectInUse[index])
	{
		obj->Reset();
		m_objectInUse[index] = false;
		m_numAllocated--;
	}
}

void Renderer_Allegro::DeleteLoadedGraphicsData(void *loadedData, IGraphicsReference *ref)
{
	(void)ref;
	if (loadedData)
		free(loadedData);
}

void Renderer_Allegro::DrawRectangle(float x, float y, float w, float h, float strokeWidth,
	float *strokeColor, float *fillColor)
{
	al_set_target_backbuffer(m_pDisplay);

	if (fillColor)
	{
		al_draw_filled_rectangle(x, y, x + w, y + h,
			al_map_rgba_f(fillColor[0], fillColor[1], fillColor[2], fillColor[3]));
	}
	if (strokeColor && strokeWidth > 0)
	{
		al_draw_rectangle(x, y, x + w, y + h,
			al_map_rgba_f(strokeColor[0], strokeColor[1], strokeColor[2], strokeColor[3]),
			strokeWidth);
	}
}

void Renderer_Allegro::DrawLine(float x1, float x2, float y1, float y2, float strokeWidth,
	float *strokeColor)
{
	if (strokeColor)
	{
		al_set_target_backbuffer(m_pDisplay);
		al_draw_line(x1, y1, x2, y2,
			al_map_rgba_f(strokeColor[0], strokeColor[1], strokeColor[2], strokeColor[3]),
			strokeWidth);
	}
}
