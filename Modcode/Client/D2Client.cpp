#include "D2Client.hpp"
#include "MapSelector.hpp"
#include "UI/Menus/Trademark.hpp"
#include "UI/Menus/Main.hpp"
#include "UI/Menus/TCPIP.hpp"
#include "UI/Menus/Ingame.hpp"
#include "Game/D2Game.hpp"
#include "Game/D2World.hpp"
#include "Game/D2Input.hpp"
#ifdef _DEBUG
#include "D2ClientDebug.hpp"
#endif

D2ModuleImportStrc *engine = nullptr;
D2GameConfigStrc *config = nullptr;
OpenD2ConfigStrc *openConfig = nullptr;
D2Client cl;

/*
 *	Initializes the client
 */
static void D2Client_InitializeClient(D2GameConfigStrc *pConfig, OpenD2ConfigStrc *pOpenConfig)
{
	config = pConfig;
	openConfig = pOpenConfig;

	memset(&cl, 0, sizeof(D2Client));

	cl.dwStartMS = engine->Milliseconds();

	cl.font16 = engine->graphics->LoadFont("data\\local\\font\\latin\\font16.dc6", "font16");
	cl.font30 = engine->graphics->LoadFont("data\\local\\font\\latin\\font30.dc6", "font30");
	cl.font42 = engine->graphics->LoadFont("data\\local\\font\\latin\\font42.dc6", "font42");
	cl.fontFormal12 = engine->graphics->LoadFont("data\\local\\font\\latin\\fontformal12.dc6",
												 "fontformal12");
	cl.fontExocet10 = engine->graphics->LoadFont("data\\local\\font\\latin\\fontexocet10.dc6",
												 "fontexocet10");
	cl.fontRidiculous = engine->graphics->LoadFont(
		"data\\local\\font\\latin\\fontridiculous.dc6",
		"fontridiculous");

	// Initialize input bindings
	gpInputBindings = new D2InputBindings();

	// Check if mapviewer mode is requested
	if (pOpenConfig->bMapViewer)
	{
		engine->Print(PRIORITY_MESSAGE, "Map Viewer mode activated");
		pOpenConfig->currentGameMode = OpenD2GameModes::MapPreviewer;

		// Create and initialize the map selector
		gpMapSelector = new MapSelector();
		gpMapSelector->ScanDirectory(pOpenConfig->szBasePath);

		// Skip trademark menu, go straight to loading
		cl.gamestate = GS_LOADING;
		cl.nLoadState = 0;
		cl.pActiveMenu = nullptr;
		cl.pLoadingMenu = nullptr;
		return;
	}

	// Set first menu to be trademark menu
	cl.gamestate = GS_TRADEMARK;
	cl.pActiveMenu = new D2Menus::Trademark();
}

/*
 *	Go back to the "main" menu, whereever it should be.
 *	The Trademark menu always goes to the main menu, but after that we depend on the charSelectContext to guide us.
 *	@author	eezstreet
 */
void D2Client_GoToContextMenu()
{
	switch (cl.charSelectContext)
	{
	case CSC_SINGLEPLAYER:
		cl.pActiveMenu = new D2Menus::Main();
		break;
	case CSC_TCPIP:
		cl.pActiveMenu = new D2Menus::TCPIP();
		break;
	}
}

/*
 *	Set up local game server
 *	@author	eezstreet
 */
void D2Client_SetupServerConnection()
{
	if (cl.szCurrentIPDestination[0] != '\0')
	{
		// If we have an IP, this means that we need to connect to that server. Not host one.
		if (!engine->NET_Connect(cl.szCurrentIPDestination, GAME_PORT))
		{
			// Error out!
			engine->Warning(__FILE__, __LINE__, "Failed to connect to server.");
			cl.gamestate = GS_MAINMENU;
			D2Client_GoToContextMenu();
		}
		cl.bLocalServer = false;
	}
	else
	{
		engine->NET_Listen(GAME_PORT);
		cl.bLocalServer = true;
	}
}

/*
 *	Handle window events
 */
static void D2Client_HandleWindowEvent(WindowEvent windowEvent)
{
	switch (windowEvent.event)
	{
	case D2WindowEventType::WINDOWEVENT_FOCUS_GAINED:
		engine->S_ResumeAudio();
		break;
	case D2WindowEventType::WINDOWEVENT_FOCUS_LOST:
		engine->S_PauseAudio();
		break;
	}
}

/*
 *	Pump input
 */
static void D2Client_HandleInput()
{
	engine->In_PumpEvents(openConfig);

	// All of the input data now lives in the OpenD2 config (yeah...) and we can iterate over it.
	for (DWORD i = 0; i < openConfig->dwNumPendingCommands; i++)
	{
		D2CommandQueue *pCmd = &openConfig->pCmds[i];

		// MapSelector intercepts input when active
		if (gpMapSelector != nullptr && gpMapSelector->IsActive())
		{
			if (pCmd->cmdType == IN_KEYDOWN)
			{
				if (gpMapSelector->HandleKeyDown(pCmd->cmdData.button.buttonID))
					continue;
			}
			else if (pCmd->cmdType == IN_MOUSEMOVE)
			{
				cl.dwMouseX = pCmd->cmdData.motion.x;
				cl.dwMouseY = pCmd->cmdData.motion.y;
				gpMapSelector->HandleMouseMove(cl.dwMouseX, cl.dwMouseY);
				continue;
			}
			else if (pCmd->cmdType == IN_MOUSEDOWN)
			{
				// Mouse position already tracked via MOUSEMOVE
				if (gpMapSelector->HandleMouseDown(cl.dwMouseX, cl.dwMouseY))
					continue;
			}
			else if (pCmd->cmdType == IN_MOUSEWHEEL)
			{
				gpMapSelector->HandleMouseWheel(pCmd->cmdData.motion.y);
				continue;
			}
			else if (pCmd->cmdType == IN_QUIT)
			{
				cl.bKillGame = true;
				return;
			}
			continue;
		}

		// Handle all of the different event types
		switch (pCmd->cmdType)
		{
		case IN_WINDOW:
			D2Client_HandleWindowEvent(pCmd->cmdData.window);
			break;
		case IN_MOUSEMOVE:
			cl.dwMouseX = pCmd->cmdData.motion.x;
			cl.dwMouseY = pCmd->cmdData.motion.y;
			break;

		case IN_MOUSEDOWN:
			if (pCmd->cmdData.button.buttonID == B_MOUSE1)
			{
				cl.bMouseDown = true;
			}
			break;

		case IN_MOUSEUP:
			if (pCmd->cmdData.button.buttonID == B_MOUSE1)
			{
				if (cl.bMouseDown)
				{
					cl.bMouseDown = false;
					cl.bMouseClicked = true;
				}
			}
			break;

		case IN_KEYDOWN:
			if (cl.pActiveMenu != nullptr)
			{
				cl.pActiveMenu->HandleKeyDown(pCmd->cmdData.button.buttonID);
			}
			break;

		case IN_KEYUP:
			// FIXME: handle binds also
#ifdef _DEBUG
			if (openConfig->currentGameMode == OpenD2GameModes::MapPreviewer)
			{
				if (Debug::HandleKeyInput(pCmd->cmdData.button.buttonID))
				{
					break;
				}
			}
#endif
			// Process keybinds when in-game
			if (cl.gamestate == GS_INGAME && gpInputBindings != nullptr)
			{
				D2BindAction action = gpInputBindings->ProcessKeyUp(pCmd->cmdData.button.buttonID, 0);
				if (action != BIND_NONE)
				{
					D2Input_HandleBindAction(action);
					break;
				}
			}

			if (cl.pActiveMenu != nullptr)
			{
				cl.pActiveMenu->HandleKeyUp(pCmd->cmdData.button.buttonID);
			}
			break;

		case IN_TEXTEDITING:
			// only menus need to work with text editing
			if (cl.pActiveMenu != nullptr)
			{
				cl.pActiveMenu->HandleTextEditing(pCmd->cmdData.text.text,
												  pCmd->cmdData.text.start, pCmd->cmdData.text.length);
			}
			break;

		case IN_TEXTINPUT:
			if (cl.pActiveMenu != nullptr)
			{
				cl.pActiveMenu->HandleTextInput(pCmd->cmdData.text.text);
			}
			break;

		case IN_MOUSEWHEEL:
			break;

		case IN_QUIT:
			cl.bKillGame = true;
			break;
		}
	}

	if (cl.pActiveMenu != nullptr)
	{
		if (cl.bMouseClicked)
		{
			if (cl.pActiveMenu->HandleMouseClicked(cl.dwMouseX, cl.dwMouseY))
			{
				return;
			}
		}
		else if (cl.bMouseDown)
		{
			if (cl.pActiveMenu->HandleMouseDown(cl.dwMouseX, cl.dwMouseY))
			{
				return;
			}
		}
	}

	// handle worldspace clicking here
}

/*
 *	Derives the starting town level ID and difficulty from the save header.
 *	Reads cl.currentSave.header.nTowns[] to find the active difficulty (bit 7 set)
 *	and the current act (bits 0-2).
 */
static int D2Client_GetStartingTownLevel(BYTE *pOutDifficulty = nullptr, BYTE *pOutAct = nullptr)
{
	static const int townLevels[MAX_ACTS] = {
		D2LEVEL_ACT1_TOWN,
		D2LEVEL_ACT2_TOWN,
		D2LEVEL_ACT3_TOWN,
		D2LEVEL_ACT4_TOWN,
		D2LEVEL_ACT5_TOWN};

	BYTE nDifficulty = D2DIFF_NORMAL;
	BYTE nAct = 0;

	// Find the active difficulty (bit 7 set in nTowns), search from Hell down
	for (int i = D2DIFF_MAX - 1; i >= 0; i--)
	{
		if (cl.currentSave.header.nTowns[i] & 0x80)
		{
			nDifficulty = (BYTE)i;
			nAct = cl.currentSave.header.nTowns[i] & 0x07;
			break;
		}
	}

	if (nAct >= MAX_ACTS)
		nAct = 0;

	if (pOutDifficulty)
		*pOutDifficulty = nDifficulty;
	if (pOutAct)
		*pOutAct = nAct;

	return townLevels[nAct];
}

/*
 *	Load stuff between main menu and regular game
 *	@author	eezstreet
 */
static void D2Client_LoadData()
{
	if (cl.nLoadState == 0)
	{ // load D2Common
		D2Common_Init(engine, config, openConfig);
		cl.nLoadState++;
	}
	else if (cl.nLoadState == 1)
	{ // load interface

		// Prime the server for init.
		if (cl.bLocalServer)
		{
			cl.bClientReadyForServer = true;
		}
		cl.nLoadState++;
	}
	else if (cl.nLoadState == 2)
	{ // D2Game is loaded on this step if we are running a local game. The client need not do anything here.
		cl.nLoadState++;
	}
	else if (cl.nLoadState == 3)
	{ // START HERE if in an inter-act loading.
	  // Create the level
#ifdef _DEBUG
		if (openConfig->currentGameMode == OpenD2GameModes::MapPreviewer)
		{
			Debug::LoadWorld();
		}
		else
#endif
		{
			// Normal game: load the world for the character's last town
			if (gpWorld == nullptr)
			{
				gpWorld = new D2ClientWorld();
			}
			gpWorld->LoadLevel(D2Client_GetStartingTownLevel());
		}
		cl.nLoadState++;
	}
	else if (cl.nLoadState == 4)
	{ // ??
		cl.nLoadState++;
	}
	else if (cl.nLoadState == 5)
	{ // ??
		cl.nLoadState++;
	}
	else if (cl.nLoadState == 6)
	{ // ??
		cl.nLoadState++;
	}
	else if (cl.nLoadState == 7)
	{ // ??
		cl.nLoadState++;
	}
	else if (cl.nLoadState == 8)
	{ // ??
		cl.nLoadState++;
	}
	else if (cl.nLoadState == 9)
	{ // go ingame
		cl.nLoadState = 0;
		cl.gamestate = GS_INGAME;

		// Clean up the loading menu
		if (cl.pLoadingMenu != nullptr)
		{
			delete cl.pLoadingMenu;
			cl.pLoadingMenu = nullptr;
		}

		// Create the client game state from save header
		if (gpGame == nullptr)
		{
			BYTE nSaveDifficulty = D2DIFF_NORMAL;
			BYTE nSaveAct = 0;
			int nTownLevel = D2Client_GetStartingTownLevel(&nSaveDifficulty, &nSaveAct);
			bool bExpansion = (cl.currentSave.header.nCharStatus & 0x20) != 0;

			gpGame = new D2ClientGame();
			gpGame->Initialize(0, nSaveAct, (WORD)nTownLevel,
							   cl.currentSave.header.dwSeed,
							   bExpansion, nSaveDifficulty);

			// In single-player (local server), create the local player directly
			// since the server stub doesn't send D2SPACKET_ASSIGNPLAYER
			if (cl.bLocalServer)
			{
				D2UnitStrc *pPlayer = gpGame->AddPlayer(
					1, cl.currentSave.header.nCharClass,
					cl.currentSave.header.szCharacterName,
					30, 30); // Town center position

				gpGame->Initialize(1, nSaveAct, (WORD)nTownLevel,
								   cl.currentSave.header.dwSeed,
								   bExpansion, nSaveDifficulty);
			}
		}

		// Switch to the in-game menu
		if (cl.pActiveMenu != nullptr)
		{
			delete cl.pActiveMenu;
		}
		cl.pActiveMenu = new D2Menus::Ingame();
	}
}

/*
 *	Ping the server, telling it that we're still alive.
 *	@author	eezstreet
 */
static void D2Client_PingServer()
{
	DWORD dwTicks;
	D2Packet packet;

	if (cl.bLocalServer || !cl.bValidatedSave)
	{ // We don't need to do this in a local server, or if we haven't started the handshake yet.
		return;
	}

	dwTicks = engine->Milliseconds();

	if (dwTicks - cl.dwLastPingPacket >= 5000)
	{ // Send one packet every 5 seconds
		packet.nPacketType = D2CPACKET_PING;
		packet.packetData.Ping.dwTickCount = dwTicks;
		packet.packetData.Ping.dwUnknown = 0;
		engine->NET_SendClientPacket(&packet);
		cl.dwLastPingPacket = dwTicks;
	}
}

/*
 *	Runs a single frame on the client.
 */
static void D2Client_RunClientFrame()
{
	const DWORD currentMs = engine->Milliseconds();
	DWORD deltaMs = currentMs - cl.dwMS;

	cl.dwMS = currentMs;

	// Pipe in input events
	D2Client_HandleInput();

	// Handle menus
	if (cl.pActiveMenu != nullptr && cl.gamestate != GS_LOADING)
	{
		// Tick on the current menu.
		cl.pActiveMenu->Tick(deltaMs);

		// Draw the active menu.
		if (cl.pActiveMenu != nullptr)
		{ // it can become null between now and then
			cl.pActiveMenu->Draw();
		}
	}
	else if (cl.pLoadingMenu != nullptr && cl.nLoadState != 0)
	{
		cl.pLoadingMenu->Draw();
	}
	// MapSelector UI takes over rendering when active
	if (gpMapSelector != nullptr && gpMapSelector->IsActive())
	{
		gpMapSelector->Draw();
		// Don't call Present() here - MapSelector::Draw() calls it
		goto frame_end;
	}
	else if (gpMapSelector != nullptr && gpMapSelector->HasSelection())
	{
		// User made a selection - transition to map preview
		engine->Print(PRIORITY_MESSAGE, "Selected map: %s", gpMapSelector->GetSelectedPath());

		// Activate MapPreviewer mode with the selected DS1
		openConfig->currentGameMode = OpenD2GameModes::MapPreviewer;
		cl.gamestate = GS_LOADING;
		cl.nLoadState = 3; // Skip to level creation step

		delete gpMapSelector;
		gpMapSelector = nullptr;
	}
	else if (gpMapSelector != nullptr && !gpMapSelector->IsActive() && !gpMapSelector->HasSelection())
	{
		// User pressed Escape - exit
		delete gpMapSelector;
		gpMapSelector = nullptr;
		cl.bKillGame = true;
		goto frame_end;
	}

#ifdef _DEBUG
	if (openConfig->currentGameMode == OpenD2GameModes::MapPreviewer)
	{
		Debug::DrawWorld();
	}
#endif

	engine->renderer->Present();

frame_end:

	// Load stuff, if we need to
	if (cl.gamestate == GS_LOADING)
	{
		D2Client_LoadData();
	}

	// Ping the server
	if (cl.gamestate == GS_LOADING || cl.gamestate == GS_INGAME)
	{
		D2Client_PingServer();
	}

	// Clear out data
	cl.bMouseClicked = false;
}

/*
 *	This gets called every frame. We return the next module to run after this one.
 */
static OpenD2Modules D2Client_RunModuleFrame(D2GameConfigStrc *pConfig, OpenD2ConfigStrc *pOpenConfig)
{
	if (config == nullptr && openConfig == nullptr && pConfig != nullptr && pOpenConfig != nullptr)
	{
		// now is our chance! initialize!
		D2Client_InitializeClient(pConfig, pOpenConfig);
	}

	D2Client_RunClientFrame();

	if (cl.bKillGame)
	{
		return MODULE_NONE;
	}

	if (cl.bLocalServer && cl.bClientReadyForServer)
	{ // If we're running a local server, we need to run that next (it will *always* run the client in the next step)
		return MODULE_SERVER;
	}

	return MODULE_CLIENT;
}

/*
 *	This gets called whenever the module is cleaned up.
 */
static void D2Client_Shutdown()
{
	// Clean up map selector
	if (gpMapSelector != nullptr)
	{
		delete gpMapSelector;
		gpMapSelector = nullptr;
	}

	if (cl.pActiveMenu != nullptr)
	{
		delete cl.pActiveMenu;
		cl.pActiveMenu = nullptr;
	}

	// Clean up world
	if (gpWorld != nullptr)
	{
		delete gpWorld;
		gpWorld = nullptr;
	}

	// Clean up game state
	if (gpGame != nullptr)
	{
		delete gpGame;
		gpGame = nullptr;
	}

	// Clean up input bindings
	if (gpInputBindings != nullptr)
	{
		delete gpInputBindings;
		gpInputBindings = nullptr;
	}
}

/*
 *	This gets called whenever we receive a packet.
 *	@author	eezstreet
 */
static bool D2Client_HandlePacket(D2Packet *pPacket)
{
	switch (pPacket->nPacketType)
	{
	// Handshake / connection packets
	case D2SPACKET_COMPRESSIONINFO:
		ClientPacket::ProcessCompressionPacket(pPacket);
		break;
	case D2SPACKET_SAVESTATUS:
		ClientPacket::ProcessSavegameStatusPacket(pPacket);
		break;
	case D2SPACKET_GAMEFLAGS:
		ClientPacket::ProcessServerMetaPacket(pPacket);
		break;
	case D2SPACKET_PONG:
		ClientPacket::ProcessPongPacket(pPacket);
		break;

	// In-game packets (from Ghidra: GAME/SCmd.cpp dispatch table)
	case D2SPACKET_ASSIGNPLAYER:
		ClientPacket::ProcessAssignPlayer(pPacket);
		break;
	case D2SPACKET_PLAYERJOINED:
		ClientPacket::ProcessPlayerJoined(pPacket);
		break;
	case D2SPACKET_PLAYERLEFT:
		ClientPacket::ProcessPlayerLeft(pPacket);
		break;
	case D2SPACKET_ASSIGNNPC:
		ClientPacket::ProcessAssignNPC(pPacket);
		break;
	case D2SPACKET_REMOVEOBJECT:
		ClientPacket::ProcessRemoveObject(pPacket);
		break;
	case D2SPACKET_PLAYERSTOP:
		ClientPacket::ProcessPlayerStop(pPacket);
		break;
	case D2SPACKET_PLAYERMOVECOORD:
		ClientPacket::ProcessPlayerMoveCoord(pPacket);
		break;
	case D2SPACKET_NPCMOVECOORD:
		ClientPacket::ProcessNPCMoveCoord(pPacket);
		break;
	case D2SPACKET_NPCSTOP:
		ClientPacket::ProcessNPCStop(pPacket);
		break;
	case D2SPACKET_NPCSTATE:
		ClientPacket::ProcessNPCState(pPacket);
		break;
	case D2SPACKET_CHAT:
		ClientPacket::ProcessChat(pPacket);
		break;
	case D2SPACKET_LIFEMANA:
		ClientPacket::ProcessLifeMana(pPacket);
		break;
	case D2SPACKET_LOADACT:
		ClientPacket::ProcessLoadAct(pPacket);
		break;
	}
	return true;
}

/*
 *	GetModuleAPI allows us to exchange a series of function pointers with the engine.
 */
static D2ModuleExportStrc gExports{0};
extern "C"
{
	D2EXPORT D2ModuleExportStrc *GetModuleAPI(D2ModuleImportStrc *pImports)
	{
		if (pImports == nullptr)
		{
			return nullptr;
		}

		if (pImports->nApiVersion != D2CLIENTAPI_VERSION)
		{ // not the right API version
			return nullptr;
		}

		engine = pImports;

		gExports.nApiVersion = D2CLIENTAPI_VERSION;
		gExports.RunModuleFrame = D2Client_RunModuleFrame;
		gExports.CleanupModule = D2Client_Shutdown;
		gExports.HandlePacket = D2Client_HandlePacket;

		return &gExports;
	}
}
