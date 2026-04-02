#ifdef USE_ALLEGRO5
#include "Renderer_Allegro.hpp"
#include "Palette.hpp"
#include "DCC.hpp"
#include "Logging.hpp"

///////////////////////////////////////////////////////
//
//	Allegro 5 Renderer Implementation
//	Phase 2: Minimal stub for window/input testing
//	Phase 3: Full 8-bit palette rendering pipeline
//

// =====================================================
// AllegroRenderObject
// =====================================================

AllegroRenderObject::AllegroRenderObject()
	: m_x(0), m_y(0), m_w(0), m_h(0), m_bVisible(false)
{
}

void AllegroRenderObject::Draw()
{
	// Phase 3: render to 8-bit framebuffer
}

void AllegroRenderObject::AttachTextureResource(IGraphicsReference *ref, int32_t frame)
{
	(void)ref; (void)frame;
}

void AllegroRenderObject::AttachCompositeTextureResource(IGraphicsReference *ref, int32_t startFrame, int32_t endFrame)
{
	(void)ref; (void)startFrame; (void)endFrame;
}

void AllegroRenderObject::AttachAnimationResource(IGraphicsReference *ref, bool bResetFrame)
{
	(void)ref; (void)bResetFrame;
}

void AllegroRenderObject::AttachTokenResource(ITokenReference *ref)
{
	(void)ref;
}

void AllegroRenderObject::AttachFontResource(IGraphicsReference *ref)
{
	(void)ref;
}

void AllegroRenderObject::SetPalshift(BYTE palette)
{
	(void)palette;
}

void AllegroRenderObject::SetDrawCoords(int x, int y, int w, int h)
{
	m_x = x; m_y = y; m_w = w; m_h = h;
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
	(void)r; (void)g; (void)b; (void)a;
}

void AllegroRenderObject::SetDrawMode(int drawMode)
{
	(void)drawMode;
}

bool AllegroRenderObject::PixelPerfectDetection(int x, int y)
{
	(void)x; (void)y;
	return false;
}

void AllegroRenderObject::SetText(const char16_t *text)
{
	(void)text;
}

void AllegroRenderObject::SetTextAlignment(int x, int y, int w, int h, int horzAlignment, int vertAlignment)
{
	(void)x; (void)y; (void)w; (void)h; (void)horzAlignment; (void)vertAlignment;
}

void AllegroRenderObject::SetTextColor(int color)
{
	(void)color;
}

void AllegroRenderObject::SetFramerate(int framerate)
{
	(void)framerate;
}

void AllegroRenderObject::SetAnimationLoop(bool bLoop)
{
	(void)bLoop;
}

void AllegroRenderObject::AddAnimationFinishedCallback(void *extraData, AnimationFinishCallback callback)
{
	(void)extraData; (void)callback;
}

void AllegroRenderObject::AddAnimationFrameCallback(int32_t frame, void *extraData, AnimationFrameCallback callback)
{
	(void)frame; (void)extraData; (void)callback;
}

void AllegroRenderObject::RemoveAnimationFinishCallbacks()
{
}

void AllegroRenderObject::SetAnimationDirection(int direction)
{
	(void)direction;
}

void AllegroRenderObject::SetTokenMode(int newMode)
{
	(void)newMode;
}

void AllegroRenderObject::SetTokenArmorLevel(int component, const char *armorLevel)
{
	(void)component; (void)armorLevel;
}

void AllegroRenderObject::SetTokenHitClass(int hitclass)
{
	(void)hitclass;
}

// =====================================================
// Renderer_Allegro
// =====================================================

Renderer_Allegro::Renderer_Allegro(D2GameConfigStrc *pConfig, OpenD2ConfigStrc *pOpenConfig, ALLEGRO_DISPLAY *pDisplay)
	: m_pDisplay(pDisplay), m_pBuiltinFont(nullptr), m_currentPalette(PAL_ACT1), m_numAllocated(0)
{
	(void)pConfig; (void)pOpenConfig;
	memset(m_objectInUse, 0, sizeof(m_objectInUse));

	// Create built-in font for text rendering
	m_pBuiltinFont = al_create_builtin_font();

	Log::Print(PRIORITY_MESSAGE, "Renderer_Allegro initialized");
}

Renderer_Allegro::~Renderer_Allegro()
{
	if (m_pBuiltinFont)
	{
		al_destroy_font(m_pBuiltinFont);
		m_pBuiltinFont = nullptr;
	}
	Log::Print(PRIORITY_MESSAGE, "Renderer_Allegro destroyed");
}

void Renderer_Allegro::DrawAlText(float x, float y, float r, float g, float b, float a, const char *text)
{
	if (m_pBuiltinFont && text)
	{
		al_set_target_backbuffer(m_pDisplay);
		al_draw_text(m_pBuiltinFont, al_map_rgba_f(r, g, b, a),
			x, y, ALLEGRO_ALIGN_LEFT, text);
	}
}

void Renderer_Allegro::DrawTextF(float x, float y, float r, float g, float b, float a, const char *fmt, ...)
{
	if (m_pBuiltinFont && fmt)
	{
		char buffer[512];
		va_list args;
		va_start(args, fmt);
		vsnprintf(buffer, sizeof(buffer), fmt, args);
		va_end(args);

		al_set_target_backbuffer(m_pDisplay);
		al_draw_text(m_pBuiltinFont, al_map_rgba_f(r, g, b, a),
			x, y, ALLEGRO_ALIGN_LEFT, buffer);
	}
}

void Renderer_Allegro::Present()
{
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
			m_numAllocated++;
			return &m_renderObjects[i];
		}
	}
	return nullptr;
}

void Renderer_Allegro::Remove(IRenderObject *Object)
{
	for (int i = 0; i < MAX_RENDER_OBJECTS; i++)
	{
		if (&m_renderObjects[i] == Object && m_objectInUse[i])
		{
			m_objectInUse[i] = false;
			m_numAllocated--;
			return;
		}
	}
}

void Renderer_Allegro::DeleteLoadedGraphicsData(void *loadedData, IGraphicsReference *ref)
{
	if (loadedData)
	{
		free(loadedData);
	}
	(void)ref;
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
	al_set_target_backbuffer(m_pDisplay);

	if (strokeColor && strokeWidth > 0)
	{
		al_draw_line(x1, y1, x2, y2,
			al_map_rgba_f(strokeColor[0], strokeColor[1], strokeColor[2], strokeColor[3]),
			strokeWidth);
	}
}

#endif // USE_ALLEGRO5
