#pragma once
#include "../D2Panel.hpp"

#define CHAT_MAX_MESSAGES 8
#define CHAT_MSG_LEN 128

class ChatPanel : public D2Panel
{
private:
	IRenderObject *m_msgLines[CHAT_MAX_MESSAGES];
	IRenderObject *m_inputLine;
	IRenderObject *m_inputLabel;
	char16_t m_messages[CHAT_MAX_MESSAGES][CHAT_MSG_LEN];
	int m_nMsgCount;

public:
	ChatPanel();
	~ChatPanel();

	void AddMessage(const char16_t *msg, int color = TextColor_White);
	void Draw() override;
	void Tick(DWORD dwDeltaMs) override;
	bool HandleTextInput(char *szText) override;
	bool HandleKeyDown(DWORD dwKey) override;
};
