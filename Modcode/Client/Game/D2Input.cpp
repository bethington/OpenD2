#include "D2Input.hpp"
#include "../D2Client.hpp"
#include "../UI/Menus/Ingame.hpp"
#include "../UI/Panels/Inventory.hpp"
#include "../UI/Panels/CharacterScreen.hpp"
#include "../UI/Panels/SkillTree.hpp"
#include "../UI/Panels/Automap.hpp"
#include "../UI/Panels/QuestLog.hpp"
#include "../UI/Panels/ChatPanel.hpp"
#include <string.h>

D2InputBindings *gpInputBindings = nullptr;

/////////////////////////////////////////////////
//
//	D2InputBindings implementation

D2InputBindings::D2InputBindings()
    : m_nBindCount(0), m_bRunToggle(false)
{
    memset(m_binds, 0, sizeof(m_binds));
    memset(m_skillSlots, 0, sizeof(m_skillSlots));
    memset(m_skillSlotLeft, 0, sizeof(m_skillSlotLeft));
    LoadDefaults();
}

/*
 *	Default key bindings matching original Diablo II.
 *	Informed by Ghidra: InitializeInputBindings, LoadAndValidateKeyBindings
 */
void D2InputBindings::LoadDefaults()
{
    m_nBindCount = 0;

    // F1-F8 = skill hotkeys 0-7 (standard D2 defaults)
    SetBind(BIND_SKILL_HOTKEY_0, 1073741882, 0); // F1
    SetBind(BIND_SKILL_HOTKEY_1, 1073741883, 0); // F2
    SetBind(BIND_SKILL_HOTKEY_2, 1073741884, 0); // F3
    SetBind(BIND_SKILL_HOTKEY_3, 1073741885, 0); // F4
    SetBind(BIND_SKILL_HOTKEY_4, 1073741886, 0); // F5
    SetBind(BIND_SKILL_HOTKEY_5, 1073741887, 0); // F6
    SetBind(BIND_SKILL_HOTKEY_6, 1073741888, 0); // F7
    SetBind(BIND_SKILL_HOTKEY_7, 1073741889, 0); // F8

    // UI toggles
    SetBind(BIND_TOGGLE_AUTOMAP, 1073741892, 0); // F11 (not standard, but Tab is complex)
    SetBind(BIND_TOGGLE_INVENTORY, 'x', 0);
    SetBind(BIND_TOGGLE_CHARACTER, 'c', 0);
    SetBind(BIND_TOGGLE_SKILLTREE, 't', 0);
    SetBind(BIND_TOGGLE_QUESTLOG, 'v', 0);
    SetBind(BIND_TOGGLE_PARTY, 'p', 0);
    SetBind(BIND_TOGGLE_BELT, '~', 0);
    SetBind(BIND_TOGGLE_CHAT, 13, 0); // Enter

    // Actions
    SetBind(BIND_SWAP_WEAPONS, 'w', 0);
    SetBind(BIND_RUN_TOGGLE, 'r', KEYMOD_CTRL);
    SetBind(BIND_CLEAR_SCREEN, 1073741897, 0); // Escape mapped to space/esc
}

void D2InputBindings::LoadFromConfig()
{
    // TODO: read bindings from INI/config file
}

void D2InputBindings::SaveToConfig()
{
    // TODO: write bindings to INI/config file
}

void D2InputBindings::SetBind(D2BindAction action, DWORD dwKey, DWORD dwModifiers)
{
    if (m_nBindCount >= MAX_KEYBINDS)
    {
        return;
    }

    m_binds[m_nBindCount].action = action;
    m_binds[m_nBindCount].dwKey = dwKey;
    m_binds[m_nBindCount].dwModifiers = dwModifiers;
    m_nBindCount++;
}

D2BindAction D2InputBindings::GetActionForKey(DWORD dwKey, DWORD dwModifiers) const
{
    for (int i = 0; i < m_nBindCount; i++)
    {
        if (m_binds[i].dwKey == dwKey && m_binds[i].dwModifiers == dwModifiers)
        {
            return m_binds[i].action;
        }
    }
    return BIND_NONE;
}

void D2InputBindings::AssignSkillHotkey(int nSlot, WORD wSkillId, bool bLeftSkill)
{
    if (nSlot < 0 || nSlot >= MAX_SKILL_HOTKEYS)
    {
        return;
    }

    m_skillSlots[nSlot] = wSkillId;
    m_skillSlotLeft[nSlot] = bLeftSkill;
}

WORD D2InputBindings::GetSkillHotkey(int nSlot) const
{
    if (nSlot < 0 || nSlot >= MAX_SKILL_HOTKEYS)
    {
        return 0;
    }
    return m_skillSlots[nSlot];
}

bool D2InputBindings::IsSkillHotkeyLeft(int nSlot) const
{
    if (nSlot < 0 || nSlot >= MAX_SKILL_HOTKEYS)
    {
        return false;
    }
    return m_skillSlotLeft[nSlot];
}

void D2InputBindings::ClearSkillHotkey(int nSlot)
{
    if (nSlot >= 0 && nSlot < MAX_SKILL_HOTKEYS)
    {
        m_skillSlots[nSlot] = 0;
        m_skillSlotLeft[nSlot] = false;
    }
}

void D2InputBindings::ClearAllSkillHotkeys()
{
    memset(m_skillSlots, 0, sizeof(m_skillSlots));
    memset(m_skillSlotLeft, 0, sizeof(m_skillSlotLeft));
}

D2BindAction D2InputBindings::ProcessKeyUp(DWORD dwKey, DWORD dwModifiers)
{
    return GetActionForKey(dwKey, dwModifiers);
}

/////////////////////////////////////////////////
//
//	Action handler
//	Routes a keybind action to the appropriate game function.

void D2Input_HandleBindAction(D2BindAction action)
{
    // Helper: get the Ingame menu if we're in-game
    D2Menus::Ingame *pIngame = nullptr;
    if (cl.gamestate == GS_INGAME && cl.pActiveMenu != nullptr)
    {
        pIngame = static_cast<D2Menus::Ingame *>(cl.pActiveMenu);
    }

    switch (action)
    {
    case BIND_SKILL_HOTKEY_0:
    case BIND_SKILL_HOTKEY_1:
    case BIND_SKILL_HOTKEY_2:
    case BIND_SKILL_HOTKEY_3:
    case BIND_SKILL_HOTKEY_4:
    case BIND_SKILL_HOTKEY_5:
    case BIND_SKILL_HOTKEY_6:
    case BIND_SKILL_HOTKEY_7:
    case BIND_SKILL_HOTKEY_8:
    case BIND_SKILL_HOTKEY_9:
    case BIND_SKILL_HOTKEY_A:
    case BIND_SKILL_HOTKEY_B:
    case BIND_SKILL_HOTKEY_C:
    case BIND_SKILL_HOTKEY_D:
    case BIND_SKILL_HOTKEY_E:
    case BIND_SKILL_HOTKEY_F:
    {
        // TODO: send skill select packet to server
        // int nSlot = action - BIND_SKILL_HOTKEY_0;
        break;
    }

    case BIND_TOGGLE_AUTOMAP:
        if (pIngame != nullptr)
        {
            Automap *pAutomap = pIngame->GetAutomap();
            if (pAutomap->IsVisible())
                pAutomap->Hide();
            else
                pAutomap->Show();
        }
        break;

    case BIND_TOGGLE_INVENTORY:
        if (pIngame != nullptr)
        {
            Inventory *pInv = pIngame->GetInventory();
            if (pInv->IsVisible())
                pInv->Hide();
            else
                pInv->Show();
        }
        break;

    case BIND_TOGGLE_CHARACTER:
        if (pIngame != nullptr)
        {
            CharacterScreen *pChar = pIngame->GetCharacterScreen();
            if (pChar->IsVisible())
                pChar->Hide();
            else
                pChar->Show();
        }
        break;

    case BIND_TOGGLE_SKILLTREE:
        if (pIngame != nullptr)
        {
            SkillTree *pSkills = pIngame->GetSkillTree();
            if (pSkills->IsVisible())
                pSkills->Hide();
            else
                pSkills->Show();
        }
        break;

    case BIND_TOGGLE_QUESTLOG:
        if (pIngame != nullptr)
        {
            QuestLog *pQuests = pIngame->GetQuestLog();
            if (pQuests->IsVisible())
                pQuests->Hide();
            else
                pQuests->Show();
        }
        break;

    case BIND_TOGGLE_PARTY:
        // TODO: party panel not yet implemented
        break;

    case BIND_SWAP_WEAPONS:
    {
        // From Ghidra: PlayWeaponSwapSoundIfNoPanel
        D2Packet packet;
        packet.nPacketType = D2CPACKET_SWAPWEAPONS;
        engine->NET_SendClientPacket(&packet);
        break;
    }

    case BIND_RUN_TOGGLE:
        if (gpInputBindings != nullptr)
        {
            gpInputBindings->ToggleRun();
        }
        break;

    case BIND_CLEAR_SCREEN:
        if (pIngame != nullptr)
        {
            pIngame->GetInventory()->Hide();
            pIngame->GetCharacterScreen()->Hide();
            pIngame->GetSkillTree()->Hide();
            pIngame->GetQuestLog()->Hide();
            pIngame->GetChatPanel()->Hide();
        }
        break;

    default:
        break;
    }
}
