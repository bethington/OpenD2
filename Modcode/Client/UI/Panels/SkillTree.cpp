#include "SkillTree.hpp"

// Skill names per class, 30 skills each, 3 tabs of 10
// Tab names followed by skill names (from D2's Skills.txt ordering)
static const char16_t *g_szSkillTabNames[D2CLASS_MAX][3] = {
    {u"Javelin/Spear", u"Passive/Magic", u"Bow/Crossbow"},      // Amazon
    {u"Cold Spells", u"Lightning Spells", u"Fire Spells"},      // Sorceress
    {u"Curses", u"Poison/Bone", u"Summoning"},                  // Necromancer
    {u"Combat Skills", u"Offensive Auras", u"Defensive Auras"}, // Paladin
    {u"Warcries", u"Combat Masteries", u"Combat Skills"},       // Barbarian
    {u"Summoning", u"Shape Shifting", u"Elemental"},            // Druid
    {u"Martial Arts", u"Shadow Disciplines", u"Traps"},         // Assassin
};

SkillTree::SkillTree()
    : m_nLineCount(0), m_bDirty(true)
{
    m_bVisible = false;
    x = 0;
    y = 0;

    m_titleText = engine->renderer->AllocateObject(1);
    m_titleText->AttachFontResource(cl.font30);
    m_titleText->SetTextColor(TextColor_Gold);
    m_titleText->SetDrawCoords(20, 10, 0, 0);
    m_titleText->SetText(u"Skill Tree");

    for (int i = 0; i < SKILLTREE_MAX_LINES; i++)
    {
        m_lines[i] = engine->renderer->AllocateObject(1);
        m_lines[i]->AttachFontResource(cl.font16);
        m_lines[i]->SetDrawCoords(20, 42 + i * 15, 0, 0);
    }
}

SkillTree::~SkillTree()
{
    engine->renderer->Remove(m_titleText);
    for (int i = 0; i < SKILLTREE_MAX_LINES; i++)
        engine->renderer->Remove(m_lines[i]);
}

void SkillTree::RefreshSkills()
{
    m_bDirty = false;
    D2SaveHeader &hdr = cl.currentSave.header;
    D2SaveExtendedData &ext = cl.currentSave.extended;
    char16_t buf[128];

    // Clear lines
    for (int i = 0; i < SKILLTREE_MAX_LINES; i++)
        m_lines[i]->SetText(u"");

    int charClass = hdr.nCharClass;
    if (charClass < 0 || charClass >= D2CLASS_MAX)
        charClass = 0;

    // Title
    D2Lib::qsnprintf(buf, 128, u"%s Skills", Client_className(charClass));
    m_titleText->SetText(buf);

    int line = 0;

    // Display 3 tabs of 10 skills each
    for (int tab = 0; tab < 3 && line < SKILLTREE_MAX_LINES; tab++)
    {
        // Tab header
        m_lines[line]->SetText(g_szSkillTabNames[charClass][tab]);
        m_lines[line]->SetTextColor(TextColor_Gold);
        line++;

        // 10 skills per tab
        for (int s = 0; s < 10 && line < SKILLTREE_MAX_LINES; s++)
        {
            int skillIdx = tab * 10 + s;
            BYTE pts = (skillIdx < MAX_D2SAVE_SKILLS) ? ext.skills[skillIdx] : 0;
            if (pts > 0)
            {
                D2Lib::qsnprintf(buf, 128, u"  Skill %d: %d pts", skillIdx + 1, (int)pts);
                m_lines[line]->SetText(buf);
                m_lines[line]->SetTextColor(TextColor_White);
            }
            else
            {
                D2Lib::qsnprintf(buf, 128, u"  Skill %d: -", skillIdx + 1);
                m_lines[line]->SetText(buf);
                m_lines[line]->SetTextColor(TextColor_Grey);
            }
            line++;
        }
    }
    m_nLineCount = line;
}

void SkillTree::Show()
{
    m_bVisible = true;
    m_bDirty = true;
}

void SkillTree::Draw()
{
    if (!m_bVisible)
        return;

    if (m_bDirty)
        RefreshSkills();

    m_titleText->Draw();
    for (int i = 0; i < m_nLineCount; i++)
        m_lines[i]->Draw();

    DrawWidgets();
}

void SkillTree::Tick(DWORD dwDeltaMs)
{
    if (!m_bVisible)
        return;
    D2Panel::Tick(dwDeltaMs);
}
