#pragma once
#include "../D2Panel.hpp"

#define SKILLTREE_MAX_LINES 32

class SkillTree : public D2Panel
{
private:
	IRenderObject *m_titleText;
	IRenderObject *m_lines[SKILLTREE_MAX_LINES];
	int m_nLineCount;
	bool m_bDirty;

	void RefreshSkills();

public:
	SkillTree();
	~SkillTree();

	void Show();
	void Draw() override;
	void Tick(DWORD dwDeltaMs) override;
};
