#include "CharacterScreen.hpp"
#include "../../D2Client.hpp"
#include <string.h>

// Class token names for player character rendering
static const char *g_szClassTokens[D2CLASS_MAX] = {
    "AM", "SO", "NE", "PA", "BA", "DZ", "AI"
};

// D2 stat IDs
enum
{
    STAT_STRENGTH = 0,
    STAT_ENERGY = 1,
    STAT_DEXTERITY = 2,
    STAT_VITALITY = 3,
    STAT_STATPTS = 4,
    STAT_NEWSKILLS = 5,
    STAT_HITPOINTS = 6,
    STAT_MAXHP = 7,
    STAT_MANA = 8,
    STAT_MAXMANA = 9,
    STAT_STAMINA = 10,
    STAT_MAXSTAMINA = 11,
    STAT_LEVEL = 12,
    STAT_EXPERIENCE = 13,
    STAT_GOLD = 14,
    STAT_GOLDBANK = 15,
    STAT_FIRERESIST = 39,
    STAT_LIGHTRESIST = 41,
    STAT_COLDRESIST = 43,
    STAT_POISONRESIST = 45,
};

// D2 body location codes
static const char *g_szBodySlotNames[] = {
    "", "Head", "Neck", "Torso", "RHand", "LHand",
    "RRing", "LRing", "Belt", "Boots", "Gloves", "RSwap", "LSwap"};

CharacterScreen::CharacterScreen()
    : m_background(nullptr), m_bgReference(nullptr),
      m_token(nullptr), m_charRender(nullptr), m_nLastClass(-1), m_bDirty(true)
{
    m_bVisible = false;
    x = 0;
    y = 0;

    for (int i = 0; i < CHARSCREEN_NUM_LINES; i++)
    {
        m_lines[i] = engine->renderer->AllocateObject(1);
        m_lines[i]->AttachFontResource(cl.font16);
        m_lines[i]->SetDrawCoords(20, 20 + i * 18, 0, 0);
    }

    // Title line uses larger font
    m_lines[0]->AttachFontResource(cl.font30);
    m_lines[0]->SetTextColor(TextColor_Gold);
    m_lines[0]->SetDrawCoords(20, 10, 0, 0);

    // Sub-title
    m_lines[1]->SetTextColor(TextColor_White);
    m_lines[1]->SetDrawCoords(20, 40, 0, 0);

    // Stat labels start at line 2, offset down
    for (int i = 2; i < CHARSCREEN_NUM_LINES; i++)
    {
        m_lines[i]->SetDrawCoords(20, 50 + (i - 2) * 18, 0, 0);
    }

    // Color the resistance lines
    // Lines: 2=str, 3=dex, 4=vit, 5=ene, 6=blank, 7=life, 8=mana, 9=stam
    // 10=blank, 11=fire, 12=cold, 13=light, 14=poison, 15=gold/exp
    if (CHARSCREEN_NUM_LINES > 11)
        m_lines[11]->SetTextColor(TextColor_Red);
    if (CHARSCREEN_NUM_LINES > 12)
        m_lines[12]->SetTextColor(TextColor_Blue);
    if (CHARSCREEN_NUM_LINES > 13)
        m_lines[13]->SetTextColor(TextColor_Yellow);
    if (CHARSCREEN_NUM_LINES > 14)
        m_lines[14]->SetTextColor(TextColor_BrightGreen);

    // Try to load the character screen background
    m_bgReference = engine->graphics->CreateReference(
        "data\\global\\ui\\PANEL\\invchar6.dc6", UsagePolicy_Permanent);
    if (m_bgReference)
    {
        m_background = engine->renderer->AllocateObject(0);
        m_background->AttachCompositeTextureResource(m_bgReference, 0, 1);
        m_background->SetDrawCoords(0, 0, 320, 432);
    }
}

CharacterScreen::~CharacterScreen()
{
    for (int i = 0; i < CHARSCREEN_NUM_LINES; i++)
    {
        engine->renderer->Remove(m_lines[i]);
    }
    if (m_charRender)
    {
        engine->renderer->Remove(m_charRender);
    }
    if (m_token)
    {
        engine->graphics->DeleteReference(m_token);
    }
    if (m_background)
    {
        engine->renderer->Remove(m_background);
    }
}

void CharacterScreen::RefreshCharToken()
{
    D2SaveHeader &hdr = cl.currentSave.header;
    int charClass = hdr.nCharClass;
    if (charClass < 0 || charClass >= D2CLASS_MAX)
        charClass = 0;

    // Only recreate token if the class changed
    if (charClass == m_nLastClass && m_charRender != nullptr)
        return;

    // Clean up previous token
    if (m_charRender)
    {
        engine->renderer->Remove(m_charRender);
        m_charRender = nullptr;
    }
    if (m_token)
    {
        engine->graphics->DeleteReference(m_token);
        m_token = nullptr;
    }

    m_nLastClass = charClass;

    // Create new token
    m_token = engine->graphics->CreateReference(TOKEN_CHAR, g_szClassTokens[charClass]);
    if (!m_token)
        return;

    m_charRender = engine->renderer->AllocateObject(1);
    m_charRender->AttachTokenResource(m_token);
    m_charRender->SetTokenHitClass(WC_HTH);
    m_charRender->SetTokenMode(PLRMODE_NU);

    // Set appearance from save header
    for (int i = 0; i < COMP_MAX; i++)
    {
        m_charRender->SetTokenArmorLevel(i, "lit");
    }

    // Position character on the left side of the panel
    m_charRender->SetDrawCoords(160, 350, 0, 0);
}

DWORD CharacterScreen::GetStatValue(WORD statId)
{
    D2SaveExtendedData &ext = cl.currentSave.extended;
    for (int i = 0; i < ext.nStatCount; i++)
    {
        if (ext.stats[i].nStatId == statId)
            return ext.stats[i].dwValue;
    }
    return 0;
}

void CharacterScreen::RefreshStats()
{
    m_bDirty = false;
    D2SaveHeader &hdr = cl.currentSave.header;
    char16_t buf[128];

    // Refresh the character token preview
    RefreshCharToken();

    // Line 0: Character name
    char16_t name[32];
    for (int i = 0; i < 16 && hdr.szCharacterName[i]; i++)
        name[i] = (char16_t)hdr.szCharacterName[i];
    name[15] = 0;
    m_lines[0]->SetText(name);

    // Line 1: Level and class
    D2Lib::qsnprintf(buf, 128, u"Level %d %s", (int)hdr.nCharLevel, Client_className(hdr.nCharClass));
    m_lines[1]->SetText(buf);

    // Lines 2-5: Primary stats
    D2Lib::qsnprintf(buf, 128, u"Strength:    %d", (int)GetStatValue(STAT_STRENGTH));
    m_lines[2]->SetText(buf);
    D2Lib::qsnprintf(buf, 128, u"Dexterity:   %d", (int)GetStatValue(STAT_DEXTERITY));
    m_lines[3]->SetText(buf);
    D2Lib::qsnprintf(buf, 128, u"Vitality:    %d", (int)GetStatValue(STAT_VITALITY));
    m_lines[4]->SetText(buf);
    D2Lib::qsnprintf(buf, 128, u"Energy:      %d", (int)GetStatValue(STAT_ENERGY));
    m_lines[5]->SetText(buf);

    // Line 6: Stat/skill points
    DWORD statPts = GetStatValue(STAT_STATPTS);
    DWORD skillPts = GetStatValue(STAT_NEWSKILLS);
    D2Lib::qsnprintf(buf, 128, u"Stat Pts: %d  Skill Pts: %d", (int)statPts, (int)skillPts);
    m_lines[6]->SetText(buf);

    // Lines 7-9: Life, Mana, Stamina (stored as value * 256)
    D2Lib::qsnprintf(buf, 128, u"Life:    %d / %d",
                     (int)(GetStatValue(STAT_HITPOINTS) >> 8), (int)(GetStatValue(STAT_MAXHP) >> 8));
    m_lines[7]->SetText(buf);
    D2Lib::qsnprintf(buf, 128, u"Mana:    %d / %d",
                     (int)(GetStatValue(STAT_MANA) >> 8), (int)(GetStatValue(STAT_MAXMANA) >> 8));
    m_lines[8]->SetText(buf);
    D2Lib::qsnprintf(buf, 128, u"Stamina: %d / %d",
                     (int)(GetStatValue(STAT_STAMINA) >> 8), (int)(GetStatValue(STAT_MAXSTAMINA) >> 8));
    m_lines[9]->SetText(buf);

    // Line 10: Separator
    m_lines[10]->SetText(u"--- Resistances ---");
    m_lines[10]->SetTextColor(TextColor_Gold);

    // Lines 11-14: Resistances
    D2Lib::qsnprintf(buf, 128, u"Fire:      %d", (int)GetStatValue(STAT_FIRERESIST));
    m_lines[11]->SetText(buf);
    D2Lib::qsnprintf(buf, 128, u"Cold:      %d", (int)GetStatValue(STAT_COLDRESIST));
    m_lines[12]->SetText(buf);
    D2Lib::qsnprintf(buf, 128, u"Lightning: %d", (int)GetStatValue(STAT_LIGHTRESIST));
    m_lines[13]->SetText(buf);
    D2Lib::qsnprintf(buf, 128, u"Poison:    %d", (int)GetStatValue(STAT_POISONRESIST));
    m_lines[14]->SetText(buf);

    // Line 15: Gold and Experience
    D2Lib::qsnprintf(buf, 128, u"Gold: %d  Stash: %d  Exp: %d",
                     (int)GetStatValue(STAT_GOLD), (int)GetStatValue(STAT_GOLDBANK),
                     (int)GetStatValue(STAT_EXPERIENCE));
    m_lines[15]->SetText(buf);
}

void CharacterScreen::Show()
{
    m_bVisible = true;
    m_bDirty = true;
}

void CharacterScreen::Draw()
{
    if (!m_bVisible)
        return;

    if (m_bDirty)
        RefreshStats();

    if (m_background)
        m_background->Draw();

    if (m_charRender)
        m_charRender->Draw();

    for (int i = 0; i < CHARSCREEN_NUM_LINES; i++)
        m_lines[i]->Draw();

    DrawWidgets();
}

void CharacterScreen::Tick(DWORD dwDeltaMs)
{
    if (!m_bVisible)
        return;
    D2Panel::Tick(dwDeltaMs);
}
