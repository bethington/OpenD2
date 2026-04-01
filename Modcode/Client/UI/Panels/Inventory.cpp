#include "Inventory.hpp"
#include <string.h>

// Body location names matching D2's body location codes (0-12)
static const char16_t *g_szBodySlots[] = {
    u"None", u"Head", u"Neck", u"Torso", u"R.Hand", u"L.Hand",
    u"R.Ring", u"L.Ring", u"Belt", u"Boots", u"Gloves", u"R.Swap", u"L.Swap"};
#define NUM_BODY_SLOTS 13

// Quality names
static const char16_t *g_szQualityNames[] = {
    u"", u"Low", u"Normal", u"Superior", u"Magic", u"Set", u"Rare", u"Unique", u"Crafted"};
#define NUM_QUALITY_NAMES 9

Inventory::Inventory()
    : m_background(nullptr), m_bgReference(nullptr), m_bDirty(true)
{
    m_bVisible = false;
    x = 400; // Right side of screen

    // Title
    m_titleText = engine->renderer->AllocateObject(1);
    m_titleText->AttachFontResource(cl.font30);
    m_titleText->SetTextColor(TextColor_Gold);
    m_titleText->SetDrawCoords(420, 10, 0, 0);
    m_titleText->SetText(u"Inventory");

    // Equipment slot lines
    for (int i = 0; i < INV_MAX_EQUIP_LINES; i++)
    {
        m_equipLines[i] = engine->renderer->AllocateObject(1);
        m_equipLines[i]->AttachFontResource(cl.font16);
        m_equipLines[i]->SetDrawCoords(420, 42 + i * 16, 0, 0);
    }

    // Grid item lines (scrollable inventory list)
    for (int i = 0; i < INV_MAX_GRID_LINES; i++)
    {
        m_gridLines[i] = engine->renderer->AllocateObject(1);
        m_gridLines[i]->AttachFontResource(cl.font16);
        m_gridLines[i]->SetDrawCoords(420, 270 + i * 16, 0, 0);
    }

    // Gold line
    m_goldText = engine->renderer->AllocateObject(1);
    m_goldText->AttachFontResource(cl.font16);
    m_goldText->SetTextColor(TextColor_Gold);
    m_goldText->SetDrawCoords(420, 468, 0, 0);

    // Try to load inventory background
    m_bgReference = engine->graphics->CreateReference(
        "data\\global\\ui\\PANEL\\invchar6R.dc6", UsagePolicy_Permanent);
    if (m_bgReference)
    {
        m_background = engine->renderer->AllocateObject(0);
        m_background->AttachCompositeTextureResource(m_bgReference, 0, 1);
        m_background->SetDrawCoords(400, 0, 320, 432);
    }
}

Inventory::~Inventory()
{
    engine->renderer->Remove(m_titleText);
    for (int i = 0; i < INV_MAX_EQUIP_LINES; i++)
        engine->renderer->Remove(m_equipLines[i]);
    for (int i = 0; i < INV_MAX_GRID_LINES; i++)
        engine->renderer->Remove(m_gridLines[i]);
    engine->renderer->Remove(m_goldText);
    if (m_background)
        engine->renderer->Remove(m_background);
}

void Inventory::RefreshItems()
{
    m_bDirty = false;
    D2SaveExtendedData &ext = cl.currentSave.extended;
    char16_t buf[128];

    // Clear all lines
    for (int i = 0; i < INV_MAX_EQUIP_LINES; i++)
        m_equipLines[i]->SetText(u"");
    for (int i = 0; i < INV_MAX_GRID_LINES; i++)
        m_gridLines[i]->SetText(u"");

    // Section header for equipped items
    m_equipLines[0]->SetText(u"--- Equipped ---");
    m_equipLines[0]->SetTextColor(TextColor_Gold);

    // Fill equipped items (nParent == 1 means equipped)
    int equipLine = 1;
    for (int i = 0; i < ext.nPlayerItemCount && equipLine < INV_MAX_EQUIP_LINES; i++)
    {
        D2SaveItemEntry &item = ext.playerItems[i];
        if (item.nParent != 1)
            continue;

        // Convert item code to char16_t
        char16_t code[8];
        for (int c = 0; c < 4; c++)
            code[c] = (char16_t)item.szCode[c];
        code[4] = 0;

        const char16_t *slotName = (item.nBodyLoc < NUM_BODY_SLOTS) ? g_szBodySlots[item.nBodyLoc] : u"?";
        const char16_t *qualName = (item.nQuality < NUM_QUALITY_NAMES) ? g_szQualityNames[item.nQuality] : u"";

        D2Lib::qsnprintf(buf, 128, u"%s: [%s] %s", slotName, code, qualName);
        m_equipLines[equipLine]->SetText(buf);

        // Color by quality
        int color = TextColor_White;
        switch (item.nQuality)
        {
        case 4:
            color = TextColor_Blue;
            break; // Magic
        case 5:
            color = TextColor_BrightGreen;
            break; // Set
        case 6:
            color = TextColor_Yellow;
            break; // Rare
        case 7:
            color = TextColor_Gold;
            break; // Unique
        case 8:
            color = TextColor_Orange;
            break; // Crafted
        }
        m_equipLines[equipLine]->SetTextColor(color);
        equipLine++;
    }

    // Section header for inventory items
    m_gridLines[0]->SetText(u"--- Inventory ---");
    m_gridLines[0]->SetTextColor(TextColor_Gold);

    // Fill inventory items (nParent == 0, nStorage == 1)
    int gridLine = 1;
    for (int i = 0; i < ext.nPlayerItemCount && gridLine < INV_MAX_GRID_LINES; i++)
    {
        D2SaveItemEntry &item = ext.playerItems[i];
        if (item.nParent != 0 || item.nStorage != 1)
            continue;

        char16_t code[8];
        for (int c = 0; c < 4; c++)
            code[c] = (char16_t)item.szCode[c];
        code[4] = 0;

        const char16_t *qualName = (item.nQuality < NUM_QUALITY_NAMES) ? g_szQualityNames[item.nQuality] : u"";

        D2Lib::qsnprintf(buf, 128, u"[%s] %s", code, qualName);
        if (item.nTotalSockets > 0)
        {
            char16_t buf2[128];
            D2Lib::qsnprintf(buf2, 128, u"%s (%d soc)", buf, (int)item.nTotalSockets);
            m_gridLines[gridLine]->SetText(buf2);
        }
        else
        {
            m_gridLines[gridLine]->SetText(buf);
        }

        int color = TextColor_White;
        switch (item.nQuality)
        {
        case 4:
            color = TextColor_Blue;
            break;
        case 5:
            color = TextColor_BrightGreen;
            break;
        case 6:
            color = TextColor_Yellow;
            break;
        case 7:
            color = TextColor_Gold;
            break;
        case 8:
            color = TextColor_Orange;
            break;
        }
        m_gridLines[gridLine]->SetTextColor(color);
        gridLine++;
    }

    // Gold display
    DWORD gold = 0, goldBank = 0;
    for (int i = 0; i < ext.nStatCount; i++)
    {
        if (ext.stats[i].nStatId == 14)
            gold = ext.stats[i].dwValue;
        if (ext.stats[i].nStatId == 15)
            goldBank = ext.stats[i].dwValue;
    }
    D2Lib::qsnprintf(buf, 128, u"Gold: %d   Stash: %d", (int)gold, (int)goldBank);
    m_goldText->SetText(buf);
}

void Inventory::Show()
{
    m_bVisible = true;
    m_bDirty = true;
}

void Inventory::Draw()
{
    if (!m_bVisible)
        return;

    if (m_bDirty)
        RefreshItems();

    if (m_background)
        m_background->Draw();

    m_titleText->Draw();
    for (int i = 0; i < INV_MAX_EQUIP_LINES; i++)
        m_equipLines[i]->Draw();
    for (int i = 0; i < INV_MAX_GRID_LINES; i++)
        m_gridLines[i]->Draw();
    m_goldText->Draw();

    DrawWidgets();
}

void Inventory::Tick(DWORD dwDeltaMs)
{
    if (!m_bVisible)
        return;
    D2Panel::Tick(dwDeltaMs);
}
