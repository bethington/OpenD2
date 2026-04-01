#include "FileSystem_MPQ.hpp"
#include "FileSystem.hpp"
#include "Logging.hpp"
#include "MPQ.hpp"
#include "Platform.hpp"

/*
 *	The MPQ file system is an extension of the original filesystem.
 *	Here, we need to load all of the MPQs that the game *needs* to use in order to run.
 *	Mods however can add additional MPQs to the search path.
 */
namespace FSMPQ
{

	struct MPQSearchPath
	{
		char szName[MAX_D2PATH];
		char szPath[MAX_D2PATH];
		D2MPQArchive *pArchive;
		MPQSearchPath *pNext;
	};

	// gpMPQSearchPaths is a stack, not a list.
	// The first element of gpMPQSearchPaths is what gets searched for assets first (if no name is specified)
	static MPQSearchPath *gpMPQSearchPaths;

	/*
	 *	Initializes the MPQ filesystem.
	 *	First loads the base game MPQs (from basepath), then loads any
	 *	additional MPQs found in the modpath directory on top.
	 *	Since the search path is a stack (last added = first searched),
	 *	mod MPQs automatically take priority over base game MPQs.
	 */
	void Init()
	{
		// Base game MPQs - loaded from basepath
		AddSearchPath("D2DATA", "d2data.mpq");
		AddSearchPath("D2CHAR", "d2char.mpq");
		AddSearchPath("D2SFX", "d2sfx.mpq");
		AddSearchPath("D2SPEECH", "d2speech.mpq");
		AddSearchPath("D2MUSIC", "d2music.mpq");
		AddSearchPath("D2VIDEO", "d2video.mpq");
#ifdef ASIA_LOCALIZATION
		AddSearchPath("D2DELTA", "d2delta.mpq");
		AddSearchPath("D2KFIXUP", "d2kfixup.mpq");
#endif
		AddSearchPath("D2EXP", "d2exp.mpq");
		AddSearchPath("D2EXPANSION", "d2XVideo.mpq");
		AddSearchPath("D2EXPANSION", "d2XTalk.mpq");
		AddSearchPath("D2EXPANSION", "d2XMusic.mpq");
		AddSearchPath("PATCH_D2", "patch_d2.mpq");

		// If a modpath is configured, scan it for additional MPQs.
		// These are loaded last and therefore searched first.
		const char *szModPath = FS::GetModPath();
		if (szModPath[0] != '\0')
		{
			LoadDirectoryMPQs(szModPath);
		}
	}

	/*
	 *	Shuts down the MPQ filesystem
	 */
	void Shutdown()
	{
		MPQSearchPath *pCurrent = gpMPQSearchPaths;
		MPQSearchPath *pPrev = nullptr;

		while (pCurrent != nullptr)
		{
			pPrev = pCurrent;
			pCurrent = pPrev->pNext;

			pPrev->pNext = nullptr;
			if (pPrev->pArchive != nullptr)
			{
				MPQ::CloseMPQ(pPrev->pArchive);
			}
			free(pPrev->pArchive);
			free(pPrev);
			pPrev = nullptr;
		}

		MPQ::Cleanup();
	}

	/*
	 *	Adds a single MPQ to the search path.
	 *	@return	A pointer to the D2MPQArchive that got loaded
	 */
	D2MPQArchive *AddSearchPath(char *szMPQName, char *szMPQPath)
	{
		if (szMPQName == nullptr || szMPQPath == nullptr)
		{
			return nullptr;
		}

		MPQSearchPath *pNew = (MPQSearchPath *)malloc(sizeof(MPQSearchPath));
		Log_ErrorAssertReturn(pNew != nullptr, nullptr);

		pNew->pArchive = (D2MPQArchive *)malloc(sizeof(D2MPQArchive));
		if (pNew->pArchive == nullptr)
		{ // couldn't allocate memory
			free(pNew->pArchive);
			free(pNew);
			Log_ErrorAssertReturn(!"Ran out of memory when adding MPQ search path.", nullptr);
		}

		D2Lib::strncpyz(pNew->szName, szMPQName, MAX_D2PATH);
		D2Lib::strncpyz(pNew->szPath, szMPQPath, MAX_D2PATH);
		MPQ::OpenMPQ(szMPQPath, szMPQName, pNew->pArchive);
		if (pNew->pArchive == nullptr)
		{ // couldn't open MPQ
			free(pNew->pArchive);
			free(pNew);
			return nullptr;
		}

		pNew->pNext = gpMPQSearchPaths;
		gpMPQSearchPaths = pNew;

		return pNew->pArchive;
	}

	/*
	 *	Scans a directory for *.mpq files and adds each one to the search path.
	 *	MPQ files already loaded (by filename, case-insensitive) are skipped.
	 *	This allows a mod folder to override base game data by layering
	 *	its MPQs on top of the base game's MPQs.
	 */
	void LoadDirectoryMPQs(const char *szDirectory)
	{
		char szFiles[MAX_FILE_LIST_SIZE][MAX_D2PATH_ABSOLUTE]{0};
		int nFiles = 0;

		// Scan the directory for .mpq files
		Sys::ListFilesInDirectory((char *)szDirectory, "*.mpq", "", &nFiles, &szFiles);
		if (nFiles <= 0)
		{
			Log::Print(PRIORITY_MESSAGE, "FSMPQ: No mod MPQs found in '%s'", szDirectory);
			return;
		}

		Log::Print(PRIORITY_MESSAGE, "FSMPQ: Found %d mod MPQ(s) in '%s'", nFiles, szDirectory);

		for (int i = 0; i < nFiles; i++)
		{
			// szFiles[i] is "/filename.mpq" (originalPath="" + "/" + cFileName)
			// Extract just the filename
			char *szFileName = szFiles[i];
			if (szFileName[0] == '/' || szFileName[0] == '\\')
				szFileName++;

			// Skip if this MPQ filename matches one already loaded in the base set
			bool bAlreadyLoaded = false;
			MPQSearchPath *pCheck = gpMPQSearchPaths;
			while (pCheck != nullptr)
			{
				if (!D2Lib::stricmp(pCheck->szPath, szFileName))
				{
					bAlreadyLoaded = true;
					break;
				}
				pCheck = pCheck->pNext;
			}

			if (bAlreadyLoaded)
			{
				Log::Print(PRIORITY_MESSAGE, "FSMPQ: Skipping '%s' (already loaded as base)", szFileName);
				continue;
			}

			// Derive a search-path name from the filename (uppercase, no extension)
			char szName[MAX_D2PATH]{0};
			D2Lib::strncpyz(szName, szFileName, MAX_D2PATH);
			char *pDot = strrchr(szName, '.');
			if (pDot)
				*pDot = '\0';
			for (char *p = szName; *p; p++)
			{
				if (*p >= 'a' && *p <= 'z')
					*p -= 32;
			}

			// Pass just the filename — FS::Open will find it in the modpath
			Log::Print(PRIORITY_MESSAGE, "FSMPQ: Loading mod MPQ '%s' as '%s'", szFileName, szName);
			AddSearchPath(szName, szFileName);
		}
	}

	/*
	 *	Look for a file from an archive
	 *	We can either explicitly say which MPQ we want to look from, or pass in nullptr to look in all of them.
	 *	Returns the handle to a file. pArchiveOut gets filled in with data about the MPQ archive.
	 *	FIXME: In -direct mode, fs_handle should be a valid file, but pArchiveOut should be invalid.
	 *	We need to route all of the existing MPQ_Read calls through an ReadFile function to make it work.
	 *	@author	eezstreet
	 */
	fs_handle FindFile(const char *szFileName, const char *szMPQName, D2MPQArchive **pArchiveOut)
	{
		MPQSearchPath *pCurrent = gpMPQSearchPaths;
		fs_handle f;

		while (pCurrent != nullptr)
		{
			if (szMPQName == nullptr || !D2Lib::stricmp(szMPQName, pCurrent->szName))
			{
				f = MPQ::FetchHandle(pCurrent->pArchive, szFileName);
				if (f != (fs_handle)-1)
				{
					if (pArchiveOut != nullptr)
					{
						*pArchiveOut = pCurrent->pArchive;
					}
					return f;
				}
			}
			pCurrent = pCurrent->pNext;
		}
		return INVALID_HANDLE; // invalid handle
	}
}