#pragma once
#include "../D2Panel.hpp"

#define QUESTLOG_MAX_LINES 10

class QuestLog : public D2Panel
{
private:
    IRenderObject *m_titleText;
    IRenderObject *m_lines[QUESTLOG_MAX_LINES];
    bool m_bDirty;

    void RefreshQuests();

public:
    QuestLog();
    ~QuestLog();

    void Show();
    void Draw() override;
    void Tick(DWORD dwDeltaMs) override;
};
