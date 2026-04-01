#include "ChatPanel.hpp"
#include <string.h>

ChatPanel::ChatPanel()
    : m_nMsgCount(0)
{
    m_bVisible = false;

    // Message history lines (bottom of screen, above HUD)
    for (int i = 0; i < CHAT_MAX_MESSAGES; i++)
    {
        m_msgLines[i] = engine->renderer->AllocateObject(1);
        m_msgLines[i]->AttachFontResource(cl.font16);
        m_msgLines[i]->SetDrawCoords(10, 360 + i * 16, 0, 0);
        m_msgLines[i]->SetTextColor(TextColor_White);
        m_messages[i][0] = 0;
    }

    // Input prompt label
    m_inputLabel = engine->renderer->AllocateObject(1);
    m_inputLabel->AttachFontResource(cl.font16);
    m_inputLabel->SetDrawCoords(10, 500, 0, 0);
    m_inputLabel->SetTextColor(TextColor_Gold);
    m_inputLabel->SetText(u"Chat:");

    // Input line
    m_inputLine = engine->renderer->AllocateObject(1);
    m_inputLine->AttachFontResource(cl.font16);
    m_inputLine->SetDrawCoords(60, 500, 0, 0);
    m_inputLine->SetTextColor(TextColor_White);
    m_inputLine->SetText(u"");

    // Welcome message
    AddMessage(u"Chat panel ready. Press Enter to type.", TextColor_BrightGreen);
}

ChatPanel::~ChatPanel()
{
    for (int i = 0; i < CHAT_MAX_MESSAGES; i++)
        engine->renderer->Remove(m_msgLines[i]);
    engine->renderer->Remove(m_inputLabel);
    engine->renderer->Remove(m_inputLine);
}

void ChatPanel::AddMessage(const char16_t *msg, int color)
{
    // Shift messages up
    if (m_nMsgCount >= CHAT_MAX_MESSAGES)
    {
        for (int i = 0; i < CHAT_MAX_MESSAGES - 1; i++)
        {
            D2Lib::qstrncpyz(m_messages[i], m_messages[i + 1], CHAT_MSG_LEN);
            // Preserve color from source line
            m_msgLines[i]->SetText(m_messages[i]);
        }
        m_nMsgCount = CHAT_MAX_MESSAGES - 1;
    }

    D2Lib::qstrncpyz(m_messages[m_nMsgCount], msg, CHAT_MSG_LEN);
    m_msgLines[m_nMsgCount]->SetText(m_messages[m_nMsgCount]);
    m_msgLines[m_nMsgCount]->SetTextColor(color);
    m_nMsgCount++;
}

bool ChatPanel::HandleTextInput(char *szText)
{
    if (!m_bVisible)
        return false;
    // Text input is handled by the text editing system
    return false;
}

bool ChatPanel::HandleKeyDown(DWORD dwKey)
{
    if (!m_bVisible)
        return false;
    return false;
}

void ChatPanel::Draw()
{
    if (!m_bVisible)
        return;

    for (int i = 0; i < CHAT_MAX_MESSAGES; i++)
        m_msgLines[i]->Draw();

    m_inputLabel->Draw();
    m_inputLine->Draw();

    DrawWidgets();
}

void ChatPanel::Tick(DWORD dwDeltaMs)
{
    if (!m_bVisible)
        return;
    D2Panel::Tick(dwDeltaMs);
}
