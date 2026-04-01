#include "D2Common.hpp"

D2COMMONAPI D2DataTablesStrc *sgptDataTables;

static D2DataTablesStrc gDataTables{0};

////////////////////////////////////////
//
//	Functions

/*
 *	Compiles a .txt into a .bin file
 *	@author	eezstreet
 */
void BIN_Compile(const char *szTextName, D2TxtColumnStrc *pColumns, int *nNumberColumns)
{
}

/*
 *	Reads a .bin file
 *	@author	eezstreet
 */
bool BIN_Read(char *szBinName, void **pDestinationData, size_t *pFileSize)
{
	D2MPQArchive *pArchive = nullptr;
	fs_handle f;
	*pFileSize = engine->FS_Open(szBinName, &f, FS_READ, true);
	DWORD dwNumRecords = 0; // FIXME: use this

	if (f == INVALID_HANDLE)
	{
		return false; // couldn't find it...this is probably a bad thing
	}

	Log_ErrorAssertReturn(*pFileSize != 0, false);

	*pDestinationData = malloc(*pFileSize);
	engine->FS_Read(f, *pDestinationData, *pFileSize, 1);
	engine->FS_CloseFile(f);

	//	Somewhat of a hack here, but the first field in the BIN actually seems to be a DWORD
	//	that specifies how many records in the file.
	//	Needless to say, this will screw everything up if we don't account for it correctly.
	void *offset = (void *)(((BYTE *)*pDestinationData) + 4);
	*pFileSize -= sizeof(DWORD);
	memmove(*pDestinationData, offset, *pFileSize);

	return true;
}

/*
 *	Loads a specific data table
 *	@author	eezstreet
 */
static int DataTables_Load(const char *szDataTableName, void **pDestinationData,
						   D2TxtLinkStrc **pDestinationLink, size_t dwRowSize)
{
	char szPath[MAX_D2PATH]{0};
	size_t dwFileSize = 0;

	// Try and load the BIN file first
	if (!gpConfig->bTXT)
	{
		snprintf(szPath, MAX_D2PATH, "%s%s.bin", D2DATATABLES_DIR, szDataTableName);

		if (BIN_Read(szPath, pDestinationData, &dwFileSize))
		{ // found the BIN file
			int nRecords = (int)(dwFileSize / dwRowSize);
			size_t remainder = dwFileSize % dwRowSize;
			engine->Print(PRIORITY_MESSAGE,
						  "DataTables: %s -> %d bytes, %d records (row=%d, remainder=%d)",
						  szDataTableName, (int)dwFileSize, nRecords, (int)dwRowSize, (int)remainder);
			return nRecords;
		}
	}

	// If that doesn't work (or we have -txt mode enabled) we need to compile the BIN itself from .txt
	snprintf(szPath, MAX_D2PATH, "%s%s.txt", D2DATATABLES_DIR, szDataTableName);
	return 0;
}

/*
 *	Load all of the data tables
 *	@author	eezstreet
 */
void DataTables_Init()
{
	sgptDataTables = &gDataTables;

	//////////////
	//
	//	Levels

	// levels.bin and leveldef.bin are special in that they are created from levels.txt
	// Therefore, they will share the same record count.
	// (and therefore, the count doesn't need to be stored in sgptDataTables)
	sgptDataTables->nLevelsTxtRecordCount =
		DataTables_Load("levels", (void **)&sgptDataTables->pLevelsTxt, nullptr, sizeof(D2LevelsTxt));
	DataTables_Load("leveldefs", (void **)&sgptDataTables->pLevelDefBin, nullptr, sizeof(D2LevelDefBin));

	sgptDataTables->nLvlTypesTxtRecordCount =
		DataTables_Load("lvltypes", (void **)&sgptDataTables->pLvlTypesTxt, nullptr, sizeof(D2LvlTypesTxt));
	sgptDataTables->nLvlSubTxtRecordCount =
		DataTables_Load("lvlsub", (void **)&sgptDataTables->pLvlSubTxt, nullptr, sizeof(D2LvlSubTxt));
	sgptDataTables->nLvlWarpTxtRecordCount =
		DataTables_Load("lvlwarp", (void **)&sgptDataTables->pLvlWarpTxt, nullptr, sizeof(D2LvlWarpTxt));
	sgptDataTables->nLvlMazeTxtRecordCount =
		DataTables_Load("lvlmaze", (void **)&sgptDataTables->pLvlMazeTxt, nullptr, sizeof(D2LvlMazeTxt));
	sgptDataTables->nLvlPrestTxtRecordCount =
		DataTables_Load("lvlprest", (void **)&sgptDataTables->pLvlPrestTxt, nullptr, sizeof(D2LvlPrestTxt));

	//////////////
	//
	//	Items

	sgptDataTables->nItemsTxtRecordCount =
		DataTables_Load("weapons", (void **)&sgptDataTables->pWeapons, nullptr, sizeof(D2ItemsTxt));
	sgptDataTables->nArmorTxtRecordCount =
		DataTables_Load("armor", (void **)&sgptDataTables->pArmor, nullptr, sizeof(D2ItemsTxt));
	sgptDataTables->nMiscTxtRecordCount =
		DataTables_Load("misc", (void **)&sgptDataTables->pMisc, nullptr, sizeof(D2ItemsTxt));

	sgptDataTables->nItemStatCostTxtRecordCount =
		DataTables_Load("itemstatcost", (void **)&sgptDataTables->pItemStatCostTxt, nullptr, sizeof(D2ItemStatCostTxt));

	sgptDataTables->nItemTypesTxtRecordCount =
		DataTables_Load("itemtypes", (void **)&sgptDataTables->pItemTypesTxt, nullptr, sizeof(D2ItemTypesTxt));

	//////////////
	//
	//	Unique and Set Items

	sgptDataTables->nUniqueItemsTxtRecordCount =
		DataTables_Load("uniqueitems", (void **)&sgptDataTables->pUniqueItemsTxt, nullptr, sizeof(D2UniqueItemsTxt));

	sgptDataTables->nSetItemsTxtRecordCount =
		DataTables_Load("setitems", (void **)&sgptDataTables->pSetItemsTxt, nullptr, sizeof(D2SetItemsTxt));

	sgptDataTables->nSetsTxtRecordCount =
		DataTables_Load("sets", (void **)&sgptDataTables->pSetsTxt, nullptr, sizeof(D2SetsTxt));

	//////////////
	//
	//	Skills

	sgptDataTables->nSkillsTxtRecordCount =
		DataTables_Load("skills", (void **)&sgptDataTables->pSkillsTxt, nullptr, sizeof(D2SkillsTxt));

	sgptDataTables->nSkillDescTxtRecordCount =
		DataTables_Load("skilldesc", (void **)&sgptDataTables->pSkillDescTxt, nullptr, sizeof(D2SkillDescTxt));

	//////////////
	//
	//	Character Stats

	sgptDataTables->nCharStatsTxtRecordCount =
		DataTables_Load("charstats", (void **)&sgptDataTables->pCharStatsTxt, nullptr, sizeof(D2CharStatsTxt));

	//////////////
	//
	//	Properties

	sgptDataTables->nPropertiesTxtRecordCount =
		DataTables_Load("properties", (void **)&sgptDataTables->pPropertiesTxt, nullptr, sizeof(D2PropertiesTxt));
}

/*
 *	Delete all of the data tables
 *	@author	eezstreet
 */
void DataTables_Free()
{
	free(sgptDataTables->pLevelsTxt);
	free(sgptDataTables->pLevelDefBin);
	free(sgptDataTables->pLvlTypesTxt);
	free(sgptDataTables->pLvlSubTxt);
	free(sgptDataTables->pLvlWarpTxt);
	free(sgptDataTables->pLvlMazeTxt);
	free(sgptDataTables->pLvlPrestTxt);
	free(sgptDataTables->pWeapons);
	free(sgptDataTables->pArmor);
	free(sgptDataTables->pMisc);
	free(sgptDataTables->pItemStatCostTxt);
	free(sgptDataTables->pItemTypesTxt);
	free(sgptDataTables->pUniqueItemsTxt);
	free(sgptDataTables->pSetItemsTxt);
	free(sgptDataTables->pSetsTxt);
	free(sgptDataTables->pSkillsTxt);
	free(sgptDataTables->pSkillDescTxt);
	free(sgptDataTables->pCharStatsTxt);
	free(sgptDataTables->pPropertiesTxt);
}
