#pragma once
#include "../D2Panel.hpp"

#define INV_MAX_EQUIP_LINES 13
#define INV_MAX_GRID_LINES 12
#define INV_MAX_SPRITES 13

class Inventory : public D2Panel
{
private:
	IRenderObject *m_background;
	IGraphicsReference *m_bgReference;
	IRenderObject *m_titleText;
	IRenderObject *m_equipLines[INV_MAX_EQUIP_LINES];
	IRenderObject *m_gridLines[INV_MAX_GRID_LINES];
	IRenderObject *m_goldText;

	// Item sprites for equipped items
	IGraphicsReference *m_itemSpriteRefs[INV_MAX_SPRITES];
	IRenderObject *m_itemSprites[INV_MAX_SPRITES];
	int m_nSpriteCount;

	bool m_bDirty;

	void RefreshItems();
	void ClearSprites();

public:
	Inventory();
	~Inventory();

	void Show();
	void Draw() override;
	void Tick(DWORD dwDeltaMs) override;
};
