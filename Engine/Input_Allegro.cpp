#include "Diablo2.hpp"
#include "imgui.h"
#include "imgui_impl_allegro5.h"

// ImGui overlay toggle (engine-level, toggled by F12)
bool g_showImGuiOverlay = false;

#define MAX_INPUT_COMMANDS 512

// Forward declaration for Window_Allegro
namespace Window
{
	ALLEGRO_EVENT_QUEUE *GetEventQueue();
}

namespace IN
{
	static D2CommandQueue gProcessedCommands[MAX_INPUT_COMMANDS];
	static DWORD gdwNumProcessedCommands;

	static bool bTextEditingActive = false;

	/*
	 *	Maps Allegro key codes to D2InputButton values.
	 *	These values match the D2InputButton enum in D2Shared.hpp.
	 */
	static inline DWORD MapAllegroKey(int allegroKey)
	{
		switch (allegroKey)
		{
		// Arrow keys
		case ALLEGRO_KEY_RIGHT:    return 1073741903; // B_RIGHTARROW
		case ALLEGRO_KEY_LEFT:     return 1073741904; // B_LEFTARROW
		case ALLEGRO_KEY_DOWN:     return 1073741905; // B_DOWNARROW
		case ALLEGRO_KEY_UP:       return 1073741906; // B_UPARROW

		// Navigation
		case ALLEGRO_KEY_HOME:     return 1073741898; // B_HOME
		case ALLEGRO_KEY_END:      return 1073741901; // B_END
		case ALLEGRO_KEY_PGUP:   return 1073741899; // B_PAGEUP
		case ALLEGRO_KEY_PGDN: return 1073741902; // B_PAGEDOWN
		case ALLEGRO_KEY_INSERT:   return 1073741897; // B_INSERT
		case ALLEGRO_KEY_DELETE:   return '\177';     // B_DELETE
		case ALLEGRO_KEY_BACKSPACE: return '\b';      // B_BACKSPACE

		// Special keys
		case ALLEGRO_KEY_ESCAPE:   return 27;
		case ALLEGRO_KEY_TAB:      return '\t';
		case ALLEGRO_KEY_ENTER:    return '\r';
		case ALLEGRO_KEY_SPACE:    return ' ';

		// Function keys
		case ALLEGRO_KEY_F1:  return 1073741882;
		case ALLEGRO_KEY_F2:  return 1073741883;
		case ALLEGRO_KEY_F3:  return 1073741884;
		case ALLEGRO_KEY_F4:  return 1073741885;
		case ALLEGRO_KEY_F5:  return 1073741886;
		case ALLEGRO_KEY_F6:  return 1073741887;
		case ALLEGRO_KEY_F7:  return 1073741888;
		case ALLEGRO_KEY_F8:  return 1073741889;
		case ALLEGRO_KEY_F9:  return 1073741890;
		case ALLEGRO_KEY_F10: return 1073741891;
		case ALLEGRO_KEY_F11: return 1073741892;
		case ALLEGRO_KEY_F12: return 1073741893;

		// Modifier keys
		case ALLEGRO_KEY_LSHIFT:  return 1073742049;
		case ALLEGRO_KEY_RSHIFT:  return 1073742053;
		case ALLEGRO_KEY_LCTRL:   return 1073742048;
		case ALLEGRO_KEY_RCTRL:   return 1073742052;
		case ALLEGRO_KEY_ALT:     return 1073742050;
		case ALLEGRO_KEY_ALTGR:   return 1073742054;

		// Letters (lowercase ASCII)
		case ALLEGRO_KEY_A: return 'a';
		case ALLEGRO_KEY_B: return 'b';
		case ALLEGRO_KEY_C: return 'c';
		case ALLEGRO_KEY_D: return 'd';
		case ALLEGRO_KEY_E: return 'e';
		case ALLEGRO_KEY_F: return 'f';
		case ALLEGRO_KEY_G: return 'g';
		case ALLEGRO_KEY_H: return 'h';
		case ALLEGRO_KEY_I: return 'i';
		case ALLEGRO_KEY_J: return 'j';
		case ALLEGRO_KEY_K: return 'k';
		case ALLEGRO_KEY_L: return 'l';
		case ALLEGRO_KEY_M: return 'm';
		case ALLEGRO_KEY_N: return 'n';
		case ALLEGRO_KEY_O: return 'o';
		case ALLEGRO_KEY_P: return 'p';
		case ALLEGRO_KEY_Q: return 'q';
		case ALLEGRO_KEY_R: return 'r';
		case ALLEGRO_KEY_S: return 's';
		case ALLEGRO_KEY_T: return 't';
		case ALLEGRO_KEY_U: return 'u';
		case ALLEGRO_KEY_V: return 'v';
		case ALLEGRO_KEY_W: return 'w';
		case ALLEGRO_KEY_X: return 'x';
		case ALLEGRO_KEY_Y: return 'y';
		case ALLEGRO_KEY_Z: return 'z';

		// Numbers
		case ALLEGRO_KEY_0: return '0';
		case ALLEGRO_KEY_1: return '1';
		case ALLEGRO_KEY_2: return '2';
		case ALLEGRO_KEY_3: return '3';
		case ALLEGRO_KEY_4: return '4';
		case ALLEGRO_KEY_5: return '5';
		case ALLEGRO_KEY_6: return '6';
		case ALLEGRO_KEY_7: return '7';
		case ALLEGRO_KEY_8: return '8';
		case ALLEGRO_KEY_9: return '9';

		// Punctuation
		case ALLEGRO_KEY_TILDE:      return '`';
		case ALLEGRO_KEY_MINUS:      return '-';
		case ALLEGRO_KEY_EQUALS:     return '=';
		case ALLEGRO_KEY_OPENBRACE:  return '[';
		case ALLEGRO_KEY_CLOSEBRACE: return ']';
		case ALLEGRO_KEY_BACKSLASH:  return '\\';
		case ALLEGRO_KEY_SEMICOLON:  return ';';
		case ALLEGRO_KEY_QUOTE:      return '\'';
		case ALLEGRO_KEY_COMMA:      return ',';
		case ALLEGRO_KEY_FULLSTOP:   return '.';
		case ALLEGRO_KEY_SLASH:      return '/';

		default:
			return (DWORD)allegroKey;
		}
	}

	/*
	 *	Maps Allegro keyboard modifiers to D2 modifiers
	 */
	static inline DWORD MapAllegroModifiers(unsigned int alMod)
	{
		DWORD dwModifiers = 0;
		if (alMod & ALLEGRO_KEYMOD_SHIFT)
			dwModifiers |= KEYMOD_SHIFT;
		if (alMod & ALLEGRO_KEYMOD_CTRL)
			dwModifiers |= KEYMOD_CTRL;
		if (alMod & ALLEGRO_KEYMOD_ALT)
			dwModifiers |= KEYMOD_ALT;
		return dwModifiers;
	}

	/*
	 *	Maps Allegro mouse button to D2InputButton
	 */
	static inline D2InputButton MapAllegroMouseButton(unsigned int alButton)
	{
		switch (alButton)
		{
		case 1:  return B_MOUSE1; // left
		case 2:  return B_MOUSE2; // right
		case 3:  return B_MOUSE3; // middle
		case 4:  return B_MOUSE4;
		case 5:  return B_MOUSE5;
		default: return (D2InputButton)alButton;
		}
	}

	/*
	 *	Start/stop text editing mode
	 */
	void StartTextEditing()
	{
		bTextEditingActive = true;
	}

	void StopTextEditing()
	{
		bTextEditingActive = false;
	}

	/*
	 *	Pumps all events from Allegro event queue
	 */
	void PumpEvents(OpenD2ConfigStrc *pOpenConfig)
	{
		ALLEGRO_EVENT ev;
		ALLEGRO_EVENT_QUEUE *queue = Window::GetEventQueue();

		gdwNumProcessedCommands = 0;

		if (!queue)
		{
			pOpenConfig->pCmds = gProcessedCommands;
			pOpenConfig->dwNumPendingCommands = 0;
			return;
		}

		while (al_get_next_event(queue, &ev))
		{
			// Pass all events to ImGui (if context is ready)
			if (ImGui::GetCurrentContext())
				ImGui_ImplAllegro5_ProcessEvent(&ev);

			if (gdwNumProcessedCommands >= MAX_INPUT_COMMANDS)
				break;

			// F12 toggles ImGui overlay (engine-level, gated by editor config)
			if (ev.type == ALLEGRO_EVENT_KEY_DOWN && ev.keyboard.keycode == ALLEGRO_KEY_F12)
			{
				if (pOpenConfig->bEditorEnabled)
					g_showImGuiOverlay = !g_showImGuiOverlay;
				continue;
			}

			// If ImGui wants input, don't pass to game
			if (g_showImGuiOverlay && ImGui::GetCurrentContext())
			{
				ImGuiIO &io = ImGui::GetIO();
				if (io.WantCaptureMouse &&
					(ev.type == ALLEGRO_EVENT_MOUSE_AXES ||
					 ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN ||
					 ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_UP))
					continue;
				if (io.WantCaptureKeyboard &&
					(ev.type == ALLEGRO_EVENT_KEY_DOWN ||
					 ev.type == ALLEGRO_EVENT_KEY_UP ||
					 ev.type == ALLEGRO_EVENT_KEY_CHAR))
					continue;
			}

			switch (ev.type)
			{
			case ALLEGRO_EVENT_DISPLAY_CLOSE:
				gProcessedCommands[gdwNumProcessedCommands].cmdType = IN_QUIT;
				gdwNumProcessedCommands++;
				break;

			case ALLEGRO_EVENT_DISPLAY_SWITCH_IN:
				gProcessedCommands[gdwNumProcessedCommands].cmdType = IN_WINDOW;
				gProcessedCommands[gdwNumProcessedCommands].cmdData.window.event = WINDOWEVENT_FOCUS_GAINED;
				gdwNumProcessedCommands++;
				break;

			case ALLEGRO_EVENT_DISPLAY_SWITCH_OUT:
				gProcessedCommands[gdwNumProcessedCommands].cmdType = IN_WINDOW;
				gProcessedCommands[gdwNumProcessedCommands].cmdData.window.event = WINDOWEVENT_FOCUS_LOST;
				gdwNumProcessedCommands++;
				break;

			case ALLEGRO_EVENT_DISPLAY_RESIZE:
				gProcessedCommands[gdwNumProcessedCommands].cmdType = IN_WINDOW;
				gProcessedCommands[gdwNumProcessedCommands].cmdData.window.event = WINDOWEVENT_RESIZED;
				gdwNumProcessedCommands++;
				al_acknowledge_resize(ev.display.source);
				break;

			case ALLEGRO_EVENT_MOUSE_AXES:
				gProcessedCommands[gdwNumProcessedCommands].cmdType = IN_MOUSEMOVE;
				gProcessedCommands[gdwNumProcessedCommands].cmdData.motion.x = ev.mouse.x;
				gProcessedCommands[gdwNumProcessedCommands].cmdData.motion.y = ev.mouse.y;
				gdwNumProcessedCommands++;
				// Handle mouse wheel as separate event
				if (ev.mouse.dz != 0 || ev.mouse.dw != 0)
				{
					if (gdwNumProcessedCommands < MAX_INPUT_COMMANDS)
					{
						gProcessedCommands[gdwNumProcessedCommands].cmdType = IN_MOUSEWHEEL;
						gProcessedCommands[gdwNumProcessedCommands].cmdData.motion.x = ev.mouse.dw;
						gProcessedCommands[gdwNumProcessedCommands].cmdData.motion.y = ev.mouse.dz;
						gdwNumProcessedCommands++;
					}
				}
				break;

			case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
				gProcessedCommands[gdwNumProcessedCommands].cmdType = IN_MOUSEDOWN;
				gProcessedCommands[gdwNumProcessedCommands].cmdData.button.buttonID = MapAllegroMouseButton(ev.mouse.button);
				gProcessedCommands[gdwNumProcessedCommands].cmdData.button.mod = 0;
				gdwNumProcessedCommands++;
				break;

			case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
				gProcessedCommands[gdwNumProcessedCommands].cmdType = IN_MOUSEUP;
				gProcessedCommands[gdwNumProcessedCommands].cmdData.button.buttonID = MapAllegroMouseButton(ev.mouse.button);
				gProcessedCommands[gdwNumProcessedCommands].cmdData.button.mod = 0;
				gdwNumProcessedCommands++;
				break;

			case ALLEGRO_EVENT_KEY_DOWN:
				gProcessedCommands[gdwNumProcessedCommands].cmdType = IN_KEYDOWN;
				gProcessedCommands[gdwNumProcessedCommands].cmdData.button.buttonID = MapAllegroKey(ev.keyboard.keycode);
				gProcessedCommands[gdwNumProcessedCommands].cmdData.button.mod = 0; // set below
				{
					ALLEGRO_KEYBOARD_STATE kbState;
					al_get_keyboard_state(&kbState);
					DWORD mod = 0;
					if (al_key_down(&kbState, ALLEGRO_KEY_LSHIFT) || al_key_down(&kbState, ALLEGRO_KEY_RSHIFT))
						mod |= KEYMOD_SHIFT;
					if (al_key_down(&kbState, ALLEGRO_KEY_LCTRL) || al_key_down(&kbState, ALLEGRO_KEY_RCTRL))
						mod |= KEYMOD_CTRL;
					if (al_key_down(&kbState, ALLEGRO_KEY_ALT) || al_key_down(&kbState, ALLEGRO_KEY_ALTGR))
						mod |= KEYMOD_ALT;
					gProcessedCommands[gdwNumProcessedCommands].cmdData.button.mod = mod;
				}
				gdwNumProcessedCommands++;
				break;

			case ALLEGRO_EVENT_KEY_UP:
				gProcessedCommands[gdwNumProcessedCommands].cmdType = IN_KEYUP;
				gProcessedCommands[gdwNumProcessedCommands].cmdData.button.buttonID = MapAllegroKey(ev.keyboard.keycode);
				gProcessedCommands[gdwNumProcessedCommands].cmdData.button.mod = 0;
				gdwNumProcessedCommands++;
				break;

			case ALLEGRO_EVENT_KEY_CHAR:
				// Allegro fires KEY_CHAR with unicode character data
				// Allegro text input event — fires when text editing is active
				if (bTextEditingActive && ev.keyboard.unichar > 0 && ev.keyboard.unichar < 0x10000)
				{
					if (gdwNumProcessedCommands < MAX_INPUT_COMMANDS)
					{
						gProcessedCommands[gdwNumProcessedCommands].cmdType = IN_TEXTINPUT;
						gProcessedCommands[gdwNumProcessedCommands].cmdData.text.length = 0;
						gProcessedCommands[gdwNumProcessedCommands].cmdData.text.start = 0;
						// Convert unicode char to UTF-8
						char utf8[8] = {0};
						if (ev.keyboard.unichar < 0x80)
						{
							utf8[0] = (char)ev.keyboard.unichar;
						}
						else if (ev.keyboard.unichar < 0x800)
						{
							utf8[0] = (char)(0xC0 | (ev.keyboard.unichar >> 6));
							utf8[1] = (char)(0x80 | (ev.keyboard.unichar & 0x3F));
						}
						else
						{
							utf8[0] = (char)(0xE0 | (ev.keyboard.unichar >> 12));
							utf8[1] = (char)(0x80 | ((ev.keyboard.unichar >> 6) & 0x3F));
							utf8[2] = (char)(0x80 | (ev.keyboard.unichar & 0x3F));
						}
						D2Lib::strncpyz(gProcessedCommands[gdwNumProcessedCommands].cmdData.text.text, utf8, 32);
						gdwNumProcessedCommands++;
					}
				}
				break;
			}
		}

		pOpenConfig->pCmds = gProcessedCommands;
		pOpenConfig->dwNumPendingCommands = gdwNumProcessedCommands;
	}
}
