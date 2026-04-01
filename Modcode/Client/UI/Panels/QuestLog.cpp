#include "QuestLog.hpp"

// Difficulty progression names based on nCharTitle
// Title 0=none, then increments per difficulty completed
static const char16_t *g_szDifficultyProgress[] = {
    u"Not started",
    u"Normal completed",
    u"Nightmare completed",
    u"Hell completed",
};

static const char16_t *g_szDiffNames[] = {
    u"Normal", u"Nightmare", u"Hell"};

// Act town IDs to determine last-visited act
static int GetActFromTown(BYTE townId)
{
    // Town byte: 1=Act1, 2=Act2, 3=Act3, 4=Act4, 5=Act5
    if (townId >= 1 && townId <= 5)
        return townId;
    return 0;
}

QuestLog::QuestLog()
    : m_bDirty(true)
{
    m_bVisible = false;

    m_titleText = engine->renderer->AllocateObject(1);
    m_titleText->AttachFontResource(cl.font30);
    m_titleText->SetTextColor(TextColor_Gold);
    m_titleText->SetDrawCoords(420, 10, 0, 0);
    m_titleText->SetText(u"Quest Log");

    for (int i = 0; i < QUESTLOG_MAX_LINES; i++)
    {
        m_lines[i] = engine->renderer->AllocateObject(1);
        m_lines[i]->AttachFontResource(cl.font16);
        m_lines[i]->SetDrawCoords(420, 42 + i * 18, 0, 0);
    }
}

QuestLog::~QuestLog()
{
    engine->renderer->Remove(m_titleText);
    for (int i = 0; i < QUESTLOG_MAX_LINES; i++)
        engine->renderer->Remove(m_lines[i]);
}

void QuestLog::RefreshQuests()
{
    m_bDirty = false;
    D2SaveHeader &hdr = cl.currentSave.header;
    char16_t buf[128];

    for (int i = 0; i < QUESTLOG_MAX_LINES; i++)
        m_lines[i]->SetText(u"");

    // Character progression from title
    int title = hdr.nCharTitle;
    int progressIdx = 0;
    if (title > 0 && title <= 3)
        progressIdx = title;
    else if (title > 3)
        progressIdx = 3; // cap at Hell

    m_lines[0]->SetText(u"--- Progression ---");
    m_lines[0]->SetTextColor(TextColor_Gold);

    D2Lib::qsnprintf(buf, 128, u"Title Progress: %s", g_szDifficultyProgress[progressIdx]);
    m_lines[1]->SetText(buf);
    m_lines[1]->SetTextColor(TextColor_White);

    // Last-visited towns per difficulty
    m_lines[3]->SetText(u"--- Last Town ---");
    m_lines[3]->SetTextColor(TextColor_Gold);

    for (int d = 0; d < D2DIFF_MAX && (d + 4) < QUESTLOG_MAX_LINES; d++)
    {
        int act = GetActFromTown(hdr.nTowns[d]);
        if (act > 0)
            D2Lib::qsnprintf(buf, 128, u"%s: Act %d", g_szDiffNames[d], act);
        else
            D2Lib::qsnprintf(buf, 128, u"%s: --", g_szDiffNames[d]);
        m_lines[4 + d]->SetText(buf);
        m_lines[4 + d]->SetTextColor(TextColor_White);
    }

    // Note about future quest parsing
    m_lines[8]->SetText(u"(Quest details not yet parsed)");
    m_lines[8]->SetTextColor(TextColor_Grey);
}

void QuestLog::Show()
{
    m_bVisible = true;
    m_bDirty = true;
}

void QuestLog::Draw()
{
    if (!m_bVisible)
        return;

    if (m_bDirty)
        RefreshQuests();

    m_titleText->Draw();
    for (int i = 0; i < QUESTLOG_MAX_LINES; i++)
        m_lines[i]->Draw();

    DrawWidgets();
}

void QuestLog::Tick(DWORD dwDeltaMs)
{
    if (!m_bVisible)
        return;
    D2Panel::Tick(dwDeltaMs);
}
