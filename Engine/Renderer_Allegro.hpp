#pragma once
#ifdef USE_ALLEGRO5
#include "../Shared/D2Shared.hpp"
#include "Allegro5.hpp"

/*
 *	Allegro 5 Render Object - implements IRenderObject
 *	Phase 2 stub: minimal implementation for display testing.
 *	Phase 3 will add the full 8-bit palette rendering pipeline.
 */
class AllegroRenderObject : public IRenderObject
{
private:
	int m_x, m_y, m_w, m_h;
	bool m_bVisible;

public:
	AllegroRenderObject();

	// IRenderObject interface
	virtual void Draw() override;
	virtual void AttachTextureResource(IGraphicsReference *ref, int32_t frame) override;
	virtual void AttachCompositeTextureResource(IGraphicsReference *ref, int32_t startFrame, int32_t endFrame) override;
	virtual void AttachAnimationResource(IGraphicsReference *ref, bool bResetFrame) override;
	virtual void AttachTokenResource(ITokenReference *ref) override;
	virtual void AttachFontResource(IGraphicsReference *ref) override;
	virtual void SetPalshift(BYTE palette) override;
	virtual void SetDrawCoords(int x, int y, int w, int h) override;
	virtual void GetDrawCoords(int *x, int *y, int *w, int *h) override;
	virtual void SetColorModulate(float r, float g, float b, float a) override;
	virtual void SetDrawMode(int drawMode) override;
	virtual bool PixelPerfectDetection(int x, int y) override;
	virtual void SetText(const char16_t *text) override;
	virtual void SetTextAlignment(int x, int y, int w, int h, int horzAlignment, int vertAlignment) override;
	virtual void SetTextColor(int color) override;
	virtual void SetFramerate(int framerate) override;
	virtual void SetAnimationLoop(bool bLoop) override;
	virtual void AddAnimationFinishedCallback(void *extraData, AnimationFinishCallback callback) override;
	virtual void AddAnimationFrameCallback(int32_t frame, void *extraData, AnimationFrameCallback callback) override;
	virtual void RemoveAnimationFinishCallbacks() override;
	virtual void SetAnimationDirection(int direction) override;
	virtual void SetTokenMode(int newMode) override;
	virtual void SetTokenArmorLevel(int component, const char *armorLevel) override;
	virtual void SetTokenHitClass(int hitclass) override;
};

/*
 *	Allegro 5 Renderer - implements IRenderer
 */
class Renderer_Allegro : public IRenderer
{
private:
	ALLEGRO_DISPLAY *m_pDisplay;
	ALLEGRO_FONT *m_pBuiltinFont;
	D2Palettes m_currentPalette;

	// Render object pool
	static const int MAX_RENDER_OBJECTS = 4096;
	AllegroRenderObject m_renderObjects[MAX_RENDER_OBJECTS];
	bool m_objectInUse[MAX_RENDER_OBJECTS];
	int m_numAllocated;

public:
	Renderer_Allegro(D2GameConfigStrc *pConfig, OpenD2ConfigStrc *pOpenConfig, ALLEGRO_DISPLAY *pDisplay);
	~Renderer_Allegro();

	// Allegro-specific text drawing (not part of IRenderer)
	void DrawAlText(float x, float y, float r, float g, float b, float a, const char *text);
	void DrawTextF(float x, float y, float r, float g, float b, float a, const char *fmt, ...);

	// IRenderer interface
	virtual void Present() override;
	virtual void Clear() override;
	virtual void SetGlobalPalette(const D2Palettes palette) override;
	virtual D2Palettes GetGlobalPalette() override;
	virtual IRenderObject *AllocateObject(int stage) override;
	virtual void Remove(IRenderObject *Object) override;
	virtual void DeleteLoadedGraphicsData(void *loadedData, IGraphicsReference *ref) override;
	virtual void DrawRectangle(float x, float y, float w, float h, float strokeWidth,
							   float *strokeColor, float *fillColor) override;
	virtual void DrawLine(float x1, float x2, float y1, float y2, float strokeWidth,
						  float *strokeColor) override;
};

#endif // USE_ALLEGRO5
