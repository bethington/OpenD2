#include "IngameMain.hpp"

// Stat IDs for HUD display
enum
{
    HUD_STAT_HP = 6,
    HUD_STAT_MAXHP = 7,
    HUD_STAT_MANA = 8,
    HUD_STAT_MAXMANA = 9
};

IngameMain::IngameMain()
    : m_newStatButton(nullptr), m_newSkillButton(nullptr), m_questLogButton(nullptr),
      m_miniPanelButton(nullptr), m_leftAttackButton(nullptr), m_rightAttackButton(nullptr),
      m_bgReference(nullptr), m_background(nullptr)
{
    x = 0;
    y = 480; // Bottom of 800x600 screen (HUD area: 480-600)

    // HP text (left orb area)
    m_hpText = engine->renderer->AllocateObject(1);
    m_hpText->AttachFontResource(cl.font16);
    m_hpText->SetDrawCoords(45, 555, 0, 0);
    m_hpText->SetTextColor(TextColor_Red);

    // Mana text (right orb area)
    m_manaText = engine->renderer->AllocateObject(1);
    m_manaText->AttachFontResource(cl.font16);
    m_manaText->SetDrawCoords(710, 555, 0, 0);
    m_manaText->SetTextColor(TextColor_Blue);

    // Level display (center)
    m_levelText = engine->renderer->AllocateObject(1);
    m_levelText->AttachFontResource(cl.font16);
    m_levelText->SetDrawCoords(380, 555, 0, 0);
    m_levelText->SetTextColor(TextColor_Gold);

    // Character name (center top of HUD)
    m_nameText = engine->renderer->AllocateObject(1);
    m_nameText->AttachFontResource(cl.font16);
    m_nameText->SetDrawCoords(350, 540, 0, 0);
    m_nameText->SetTextColor(TextColor_White);

    // Try to load the HUD control panel background
    m_bgReference = engine->graphics->CreateReference(
        "data\\global\\ui\\PANEL\\800ctrlpnl7.dc6", UsagePolicy_Permanent);
    if (m_bgReference)
    {
        m_background = engine->renderer->AllocateObject(0);
        m_background->AttachCompositeTextureResource(m_bgReference, 0, 1);
        m_background->SetDrawCoords(0, 480, 800, 120);
    }
}

IngameMain::~IngameMain()
{
    engine->renderer->Remove(m_hpText);
    engine->renderer->Remove(m_manaText);
    engine->renderer->Remove(m_levelText);
    engine->renderer->Remove(m_nameText);
    if (m_background)
        engine->renderer->Remove(m_background);
}

void IngameMain::Draw()
{
    // Draw HUD background
    if (m_background)
        m_background->Draw();

    // Update HP/Mana from save data
    D2SaveExtendedData &ext = cl.currentSave.extended;
    DWORD hp = 0, maxhp = 0, mana = 0, maxmana = 0;
    for (int i = 0; i < ext.nStatCount; i++)
    {
        switch (ext.stats[i].nStatId)
        {
        case HUD_STAT_HP:
            hp = ext.stats[i].dwValue >> 8;
            break;
        case HUD_STAT_MAXHP:
            maxhp = ext.stats[i].dwValue >> 8;
            break;
        case HUD_STAT_MANA:
            mana = ext.stats[i].dwValue >> 8;
            break;
        case HUD_STAT_MAXMANA:
            maxmana = ext.stats[i].dwValue >> 8;
            break;
        }
    }

    char16_t buf[64];
    D2Lib::qsnprintf(buf, 64, u"HP: %d/%d", (int)hp, (int)maxhp);
    m_hpText->SetText(buf);
    m_hpText->Draw();

    D2Lib::qsnprintf(buf, 64, u"MP: %d/%d", (int)mana, (int)maxmana);
    m_manaText->SetText(buf);
    m_manaText->Draw();

    // Level
    D2Lib::qsnprintf(buf, 64, u"Lv %d", (int)cl.currentSave.header.nCharLevel);
    m_levelText->SetText(buf);
    m_levelText->Draw();

    // Character name
    char16_t name[32];
    D2SaveHeader &hdr = cl.currentSave.header;
    for (int i = 0; i < 16 && hdr.szCharacterName[i]; i++)
        name[i] = (char16_t)hdr.szCharacterName[i];
    name[15] = 0;
    m_nameText->SetText(name);
    m_nameText->Draw();

    DrawWidgets();
}

void IngameMain::Tick(DWORD dwDeltaMs)
{
    D2Panel::Tick(dwDeltaMs);
}
