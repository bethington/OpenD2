#pragma once
#include "../D2Panel.hpp"

#define CHARSCREEN_NUM_LINES 16

class CharacterScreen : public D2Panel
{
private:
	IRenderObject *m_background;
	IGraphicsReference *m_bgReference;
	IRenderObject *m_lines[CHARSCREEN_NUM_LINES];
	bool m_bDirty;

	DWORD GetStatValue(WORD statId);
	void RefreshStats();

public:
	CharacterScreen();
	~CharacterScreen();

	void Show();
	void Draw() override;
	void Tick(DWORD dwDeltaMs) override;
};
