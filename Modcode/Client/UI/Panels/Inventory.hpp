#pragma once
#include "../D2Panel.hpp"

#define INV_MAX_EQUIP_LINES 13
#define INV_MAX_GRID_LINES 12

class Inventory : public D2Panel
{
private:
	IRenderObject *m_background;
	IGraphicsReference *m_bgReference;
	IRenderObject *m_titleText;
	IRenderObject *m_equipLines[INV_MAX_EQUIP_LINES];
	IRenderObject *m_gridLines[INV_MAX_GRID_LINES];
	IRenderObject *m_goldText;
	bool m_bDirty;

	void RefreshItems();

public:
	Inventory();
	~Inventory();

	void Show();
	void Draw() override;
	void Tick(DWORD dwDeltaMs) override;
};
