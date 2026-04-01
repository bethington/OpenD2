#include "Automap.hpp"
#include "../../Game/D2Game.hpp"

static const char16_t *g_szDiffNames[] = {u"Normal", u"Nightmare", u"Hell"};
static const char16_t *g_szActNames[] = {u"Act I", u"Act II", u"Act III", u"Act IV", u"Act V"};

Automap::Automap()
{
    m_bVisible = false;

    for (int i = 0; i < AUTOMAP_MAX_LINES; i++)
    {
        m_lines[i] = engine->renderer->AllocateObject(1);
        m_lines[i]->AttachFontResource(cl.font16);
        m_lines[i]->SetDrawCoords(10, 10 + i * 16, 0, 0);
        m_lines[i]->SetTextColor(TextColor_White);
    }
    // First line is the header
    m_lines[0]->SetTextColor(TextColor_BrightGreen);
}

Automap::~Automap()
{
    for (int i = 0; i < AUTOMAP_MAX_LINES; i++)
        engine->renderer->Remove(m_lines[i]);
}

void Automap::Draw()
{
    if (!m_bVisible)
        return;

    char16_t buf[128];

    if (gpGame != nullptr)
    {
        // Show current game state info
        BYTE act = gpGame->GetCurrentAct();
        WORD area = gpGame->GetCurrentArea();
        BYTE diff = gpGame->GetDifficulty();

        const char16_t *actName = (act < MAX_ACTS) ? g_szActNames[act] : u"?";
        const char16_t *diffName = (diff < D2DIFF_MAX) ? g_szDiffNames[diff] : u"?";

        D2Lib::qsnprintf(buf, 128, u"Automap - %s %s", diffName, actName);
        m_lines[0]->SetText(buf);

        D2Lib::qsnprintf(buf, 128, u"Area: %d", (int)area);
        m_lines[1]->SetText(buf);

        D2UnitStrc *player = gpGame->GetLocalPlayer();
        if (player != nullptr)
        {
            D2Lib::qsnprintf(buf, 128, u"Position: %d, %d", (int)player->wX, (int)player->wY);
            m_lines[2]->SetText(buf);
        }
        else
        {
            m_lines[2]->SetText(u"");
        }
    }
    else
    {
        // Offline/save mode: show save info
        m_lines[0]->SetText(u"Automap");

        D2SaveHeader &hdr = cl.currentSave.header;
        char16_t name[32];
        for (int i = 0; i < 16 && hdr.szCharacterName[i]; i++)
            name[i] = (char16_t)hdr.szCharacterName[i];
        name[15] = 0;

        D2Lib::qsnprintf(buf, 128, u"Character: %s", name);
        m_lines[1]->SetText(buf);

        D2Lib::qsnprintf(buf, 128, u"Seed: %d", (int)hdr.dwSeed);
        m_lines[2]->SetText(buf);
    }

    m_lines[3]->SetText(u"(Map data not loaded)");
    m_lines[3]->SetTextColor(TextColor_Grey);

    for (int i = 0; i < AUTOMAP_MAX_LINES; i++)
        m_lines[i]->Draw();

    DrawWidgets();
}

void Automap::Tick(DWORD dwDeltaMs)
{
    if (!m_bVisible)
        return;
    D2Panel::Tick(dwDeltaMs);
}
