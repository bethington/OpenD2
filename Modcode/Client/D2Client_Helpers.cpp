#include "D2Client.hpp"
#include "../Common/D2Common.hpp"

// Assorted helper functions

/*
 *	Determines if the character class is male or female.
 *	@author	eezstreet
 */
bool Client_classMale(int nCharClass)
{
	switch (nCharClass)
	{
	case D2CLASS_AMAZON:
	case D2CLASS_ASSASSIN:
	case D2CLASS_SORCERESS:
		return false;
	default:
		return true;
	}
}

/*
 *	Returns the localized name of a character class.
 *	@author	eezstreet
 */
char16_t* Client_className(int nCharClass)
{
	switch (nCharClass)
	{
		case D2CLASS_AMAZON:
		default:
			return engine->TBL_FindStringFromIndex(4011);
		case D2CLASS_ASSASSIN:
			return engine->TBL_FindStringFromIndex(4013);
		case D2CLASS_BARBARIAN:
			return engine->TBL_FindStringFromIndex(4007);
		case D2CLASS_DRUID:
			return engine->TBL_FindStringFromIndex(4012);
		case D2CLASS_NECROMANCER:
			return engine->TBL_FindStringFromIndex(4009);
		case D2CLASS_PALADIN:
			return engine->TBL_FindStringFromIndex(4008);
		case D2CLASS_SORCERESS:
			return engine->TBL_FindStringFromIndex(4010);
	}
}

/*
 *	Searches the item tables (weapons, armor, misc) for a matching 4-char code.
 *	Returns the TBL string for the item's base name, or nullptr if not found.
 */
char16_t* Client_getItemName(const char* szCode)
{
	if (sgptDataTables == nullptr || szCode == nullptr)
		return nullptr;

	DWORD code = *(DWORD*)szCode;

	// Search weapons
	if (sgptDataTables->pWeapons)
	{
		for (int i = 0; i < sgptDataTables->nWeaponsTxtRecordCount; i++)
		{
			if (sgptDataTables->pWeapons[i].dwCode == code)
				return engine->TBL_FindStringFromIndex(sgptDataTables->pWeapons[i].wNameStr);
		}
	}

	// Search armor
	if (sgptDataTables->pArmor)
	{
		for (int i = 0; i < sgptDataTables->nArmorTxtRecordCount; i++)
		{
			if (sgptDataTables->pArmor[i].dwCode == code)
				return engine->TBL_FindStringFromIndex(sgptDataTables->pArmor[i].wNameStr);
		}
	}

	// Search misc
	if (sgptDataTables->pMisc)
	{
		for (int i = 0; i < sgptDataTables->nMiscTxtRecordCount; i++)
		{
			if (sgptDataTables->pMisc[i].dwCode == code)
				return engine->TBL_FindStringFromIndex(sgptDataTables->pMisc[i].wNameStr);
		}
	}

	return nullptr;
}

/*
 *	Returns the localized name of a skill based on class and skill index (0-29).
 *	Uses the SkillDesc table's wStrName field for TBL lookup.
 */
char16_t* Client_getSkillName(int nCharClass, int nSkillIndex)
{
	if (sgptDataTables == nullptr || sgptDataTables->pSkillsTxt == nullptr ||
		sgptDataTables->pSkillDescTxt == nullptr)
		return nullptr;

	// Find the skill record matching this class and index
	// Skills are ordered: class skills come in groups of 30 per class
	// But the actual skill table has all skills. We need to search by class.
	int classSkillFound = 0;
	for (int i = 0; i < sgptDataTables->nSkillsTxtRecordCount; i++)
	{
		D2SkillsTxt* pSkill = &sgptDataTables->pSkillsTxt[i];
		if (pSkill->nCharClass == (BYTE)nCharClass)
		{
			if (classSkillFound == nSkillIndex)
			{
				// Look up the skill description for the name
				WORD descId = pSkill->wSkillDesc;
				if (descId < sgptDataTables->nSkillDescTxtRecordCount)
				{
					WORD nameStr = sgptDataTables->pSkillDescTxt[descId].wStrName;
					if (nameStr != 0)
						return engine->TBL_FindStringFromIndex(nameStr);
				}
				return nullptr;
			}
			classSkillFound++;
		}
	}

	return nullptr;
}

/*
 *	For unique/set quality items, returns the specific unique or set item name.
 *	Falls back to the base item name if no unique/set match is found.
 */
char16_t* Client_getUniqueItemName(const char* szCode, BYTE nQuality)
{
	if (sgptDataTables == nullptr || szCode == nullptr)
		return Client_getItemName(szCode);

	DWORD code = *(DWORD*)szCode;

	// For unique items (quality 7), search uniqueitems.bin
	if (nQuality == 7 && sgptDataTables->pUniqueItemsTxt)
	{
		for (int i = 0; i < sgptDataTables->nUniqueItemsTxtRecordCount; i++)
		{
			D2UniqueItemsTxt* pUniq = &sgptDataTables->pUniqueItemsTxt[i];
			if (pUniq->dwBaseItemCode == code)
			{
				char16_t* name = engine->TBL_FindStringFromIndex(pUniq->wTblIndex);
				if (name)
					return name;
			}
		}
	}

	// For set items (quality 5), search setitems.bin
	if (nQuality == 5 && sgptDataTables->pSetItemsTxt)
	{
		for (int i = 0; i < sgptDataTables->nSetItemsTxtRecordCount; i++)
		{
			D2SetItemsTxt* pSet = &sgptDataTables->pSetItemsTxt[i];
			if (pSet->szItemCode == code)
			{
				char16_t* name = engine->TBL_FindStringFromIndex(pSet->wStringId);
				if (name)
					return name;
			}
		}
	}

	// Fall back to base item name
	return Client_getItemName(szCode);
}

/*
 *	Returns the inventory sprite filename (without path/extension) for an item.
 *	For unique (7) and set (5) items, returns the unique/set invfile if available.
 *	Falls back to the normal szInvFile from the base item entry.
 */
const char* Client_getItemInvFile(const char* szCode, BYTE nQuality)
{
	if (sgptDataTables == nullptr || szCode == nullptr)
		return nullptr;

	DWORD code = *(DWORD*)szCode;
	D2ItemsTxt* pBaseItem = nullptr;

	// Find the base item record
	if (sgptDataTables->pWeapons)
	{
		for (int i = 0; i < sgptDataTables->nWeaponsTxtRecordCount; i++)
		{
			if (sgptDataTables->pWeapons[i].dwCode == code)
			{ pBaseItem = &sgptDataTables->pWeapons[i]; break; }
		}
	}
	if (!pBaseItem && sgptDataTables->pArmor)
	{
		for (int i = 0; i < sgptDataTables->nArmorTxtRecordCount; i++)
		{
			if (sgptDataTables->pArmor[i].dwCode == code)
			{ pBaseItem = &sgptDataTables->pArmor[i]; break; }
		}
	}
	if (!pBaseItem && sgptDataTables->pMisc)
	{
		for (int i = 0; i < sgptDataTables->nMiscTxtRecordCount; i++)
		{
			if (sgptDataTables->pMisc[i].dwCode == code)
			{ pBaseItem = &sgptDataTables->pMisc[i]; break; }
		}
	}

	if (!pBaseItem)
		return nullptr;

	// For unique quality, try unique invfile first
	if (nQuality == 7 && pBaseItem->szUniqueInvFile[0] != '\0')
		return pBaseItem->szUniqueInvFile;

	// For set quality, try set invfile first
	if (nQuality == 5 && pBaseItem->szSetInvFile[0] != '\0')
		return pBaseItem->szSetInvFile;

	// Fall back to normal invfile
	if (pBaseItem->szInvFile[0] != '\0')
		return pBaseItem->szInvFile;

	return nullptr;
}