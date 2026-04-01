#include "D2SFile.hpp"
#include "D2SBitReader.hpp"
#include <fstream>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <vector>

//////////////////////////////////////////////////
// D2 Checksum (rotate-left + add, NOT standard CRC32)
// D2 zeros out bytes 12-15 (the checksum field), then for each byte:
//   checksum = rotl(checksum, 1) + byte

D2SFile::D2SFile()
	: playerItemCount(0), corpseCount(0), corpseItemCount(0), mercItemCount(0), hasGolem(false)
{
	memset(&header, 0, sizeof(header));
	memset(skills, 0, sizeof(skills));
}

D2SFile::~D2SFile() {}

bool D2SFile::Load(const char *path)
{
	filePath = path;
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file.is_open())
		return false;

	size_t fileSize = static_cast<size_t>(file.tellg());
	if (fileSize < D2S_HEADER_SIZE)
		return false;

	m_data.resize(fileSize);
	file.seekg(0, std::ios::beg);
	file.read(reinterpret_cast<char *>(m_data.data()), fileSize);
	return file.good();
}

void D2SFile::Parse()
{
	ParseHeader();
	if (!headerResult.ok)
		return;

	ValidateCRC();

	size_t offset = D2S_HEADER_SIZE;

	ParseQuests(offset);
	ParseWaypoints(offset);
	ParseNPC(offset);
	ParseStats(offset);
	ParseSkills(offset);

	// Player items
	ParseItems(offset, playerItems, playerItemCount, itemResult);

	// Corpse section: JM header with corpse count, then corpse metadata + items
	ParseCorpse(offset);

	// Merc/hireling section: jf marker + optional item list
	ParseMerc(offset);

	// Iron golem: kf marker + flag + optional item
	ParseGolem(offset);
}

//////////////////////////////////////////////////
// Header

void D2SFile::ParseHeader()
{
	if (m_data.size() < D2S_HEADER_SIZE)
	{
		headerResult = {false, "File too small for header"};
		return;
	}

	memcpy(&header, m_data.data(), sizeof(D2SHeader));

	if (header.dwMagic != D2S_MAGIC)
	{
		char buf[64];
		snprintf(buf, sizeof(buf), "Bad magic: 0x%08X (expected 0x%08X)", header.dwMagic, D2S_MAGIC);
		headerResult = {false, buf};
		return;
	}

	if (header.dwVersion != D2S_VERSION_110)
	{
		char buf[64];
		snprintf(buf, sizeof(buf), "Unsupported version: %u (expected %u)", header.dwVersion, D2S_VERSION_110);
		headerResult = {false, buf};
		return;
	}

	if (header.dwFileSize != m_data.size())
	{
		char buf[96];
		snprintf(buf, sizeof(buf), "Size mismatch: header says %u, actual %zu", header.dwFileSize, m_data.size());
		headerResult = {false, buf};
		return;
	}

	if (header.nCharClass >= NUM_CLASSES)
	{
		char buf[64];
		snprintf(buf, sizeof(buf), "Invalid class: %u", header.nCharClass);
		headerResult = {false, buf};
		return;
	}

	if (header.nCharLevel < 1 || header.nCharLevel > 99)
	{
		char buf[64];
		snprintf(buf, sizeof(buf), "Invalid level: %u", header.nCharLevel);
		headerResult = {false, buf};
		return;
	}

	// Validate character name: must have a null terminator and printable ASCII
	bool hasNull = false;
	for (int i = 0; i < 16; i++)
	{
		if (header.szCharacterName[i] == '\0')
		{
			hasNull = true;
			break;
		}
		char c = header.szCharacterName[i];
		if (c < 32 || c > 126)
		{
			headerResult = {false, "Non-ASCII character in name"};
			return;
		}
	}
	if (!hasNull)
	{
		headerResult = {false, "Character name not null-terminated"};
		return;
	}

	headerResult = {true};
}

//////////////////////////////////////////////////
// CRC32

uint32_t D2SFile::ComputeCRC32() const
{
	// D2's checksum: rotate-left by 1, then add each byte
	// Bytes 12-15 (the checksum field) are treated as zero
	uint32_t checksum = 0;
	for (size_t i = 0; i < m_data.size(); i++)
	{
		uint8_t byte = m_data[i];
		if (i >= 12 && i <= 15)
			byte = 0;

		// Rotate left by 1
		uint32_t carry = (checksum >> 31) & 1;
		checksum = (checksum << 1) | carry;

		// Add byte
		checksum += byte;
	}
	return checksum;
}

void D2SFile::ValidateCRC()
{
	uint32_t computed = ComputeCRC32();
	if (computed != header.dwCRC)
	{
		char buf[96];
		snprintf(buf, sizeof(buf), "CRC mismatch: stored 0x%08X, computed 0x%08X", header.dwCRC, computed);
		crcResult = {false, buf};
	}
	else
	{
		crcResult = {true};
	}
}

//////////////////////////////////////////////////
// Quests

void D2SFile::ParseQuests(size_t &offset)
{
	if (offset + 4 > m_data.size())
	{
		questResult = {false, "No quest data (EOF)"};
		return;
	}

	if (!MatchMarker(offset, MARKER_QUESTS, 4))
	{
		questResult = {false, "Quest marker 'Woo!' not found at expected offset"};
		return;
	}

	// Woo! (4) + version (4) + size (2) = 10 byte header, then 96*3 = 298 total
	size_t questSectionSize = 4 + 4 + 2 + (96 * NUM_DIFFICULTIES);
	if (offset + questSectionSize > m_data.size())
	{
		questResult = {false, "Quest section extends past EOF"};
		return;
	}

	// Validate sub-header
	uint32_t questVer;
	memcpy(&questVer, m_data.data() + offset + 4, 4);
	if (questVer != 6)
	{
		char buf[64];
		snprintf(buf, sizeof(buf), "Unexpected quest version: %u (expected 6)", questVer);
		questResult = {false, buf};
		return;
	}

	offset += questSectionSize;
	questResult = {true};
}

//////////////////////////////////////////////////
// Waypoints

void D2SFile::ParseWaypoints(size_t &offset)
{
	if (offset + 2 > m_data.size())
	{
		waypointResult = {false, "No waypoint data (EOF)"};
		return;
	}

	if (!MatchMarker(offset, MARKER_WAYPOINTS, 2))
	{
		waypointResult = {false, "Waypoint marker 'WS' not found at expected offset"};
		return;
	}

	// WS (2) + version (4) + size (2) = 8 byte header
	// Then 2 + 22 per difficulty = 24 * 3 = 72
	// Total: 80
	size_t waypointSectionSize = 2 + 4 + 2 + (24 * NUM_DIFFICULTIES);
	if (offset + waypointSectionSize > m_data.size())
	{
		waypointResult = {false, "Waypoint section extends past EOF"};
		return;
	}

	offset += waypointSectionSize;
	waypointResult = {true};
}

//////////////////////////////////////////////////
// NPC

void D2SFile::ParseNPC(size_t &offset)
{
	if (offset + 2 > m_data.size())
	{
		npcResult = {false, "No NPC data (EOF)"};
		return;
	}

	// NPC marker is 0x01 0x77
	if (offset + 2 <= m_data.size() && m_data[offset] == 0x01 && m_data[offset + 1] == 0x77)
	{
		// Read size from the next 2 bytes
		if (offset + 4 > m_data.size())
		{
			npcResult = {false, "NPC section header truncated"};
			return;
		}

		uint16_t npcSize;
		memcpy(&npcSize, m_data.data() + offset + 2, 2);

		// Total NPC section: marker(2) + size(2) + data(npcSize - 4 or the rest)
		// In practice the section is 52 bytes total
		size_t npcSectionSize = npcSize;
		if (offset + npcSectionSize > m_data.size())
		{
			npcResult = {false, "NPC section extends past EOF"};
			return;
		}

		offset += npcSectionSize;
		npcResult = {true};
	}
	else
	{
		// Some saves might not have NPC data - try to skip to stats marker
		npcResult = {false, "NPC marker 0x01 0x77 not found"};
	}
}

//////////////////////////////////////////////////
// Stats (bit-packed)

void D2SFile::ParseStats(size_t &offset)
{
	if (offset + 2 > m_data.size())
	{
		statResult = {false, "No stat data (EOF)"};
		return;
	}

	if (!MatchMarker(offset, MARKER_STATS, 2))
	{
		// Try to find it nearby
		size_t found = FindMarker(offset, MARKER_STATS, 2);
		if (found == SIZE_MAX)
		{
			statResult = {false, "Stats marker 'gf' not found"};
			return;
		}
		offset = found;
	}

	offset += 2; // skip "gf"

	D2SBitReader reader(m_data.data() + offset, m_data.size() - offset);
	stats.clear();

	try
	{
		while (true)
		{
			uint32_t statId = reader.ReadBits(9);
			if (statId == D2S_STAT_END)
				break;

			int bitWidth = GetCharStatBitWidth(static_cast<uint16_t>(statId));
			if (bitWidth < 0)
			{
				// Unknown stat ID - this is expected for PD2 custom stats
				// Try common default widths based on known patterns
				// For now, we can't continue parsing safely
				char buf[96];
				snprintf(buf, sizeof(buf), "Unknown stat ID %u at bit %zu (parsed %zu stats OK before this)",
						 statId, reader.GetBitPosition() - 9, stats.size());
				statResult = {false, buf};

				// Still advance offset past what we did read
				reader.AlignToByte();
				offset += reader.GetCurrentByteOffset();
				return;
			}

			uint32_t value = reader.ReadBits(bitWidth);

			D2SStat stat;
			stat.id = static_cast<uint16_t>(statId);
			stat.value = value;
			stats.push_back(stat);
		}
	}
	catch (const std::runtime_error &e)
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "Stat parsing error: %s (parsed %zu stats)", e.what(), stats.size());
		statResult = {false, buf};
		reader.AlignToByte();
		offset += reader.GetCurrentByteOffset();
		return;
	}

	reader.AlignToByte();
	offset += reader.GetCurrentByteOffset();
	statResult = {true};
}

//////////////////////////////////////////////////
// Skills

void D2SFile::ParseSkills(size_t &offset)
{
	if (offset + 2 > m_data.size())
	{
		skillResult = {false, "No skill data (EOF)"};
		return;
	}

	if (!MatchMarker(offset, MARKER_SKILLS, 2))
	{
		size_t found = FindMarker(offset, MARKER_SKILLS, 2);
		if (found == SIZE_MAX)
		{
			skillResult = {false, "Skills marker 'if' not found"};
			return;
		}
		offset = found;
	}

	// if (2) + skill data (30)
	if (offset + 2 + NUM_SKILLS > m_data.size())
	{
		skillResult = {false, "Skill section extends past EOF"};
		return;
	}

	offset += 2; // skip "if"
	memcpy(skills, m_data.data() + offset, NUM_SKILLS);
	offset += NUM_SKILLS;

	skillResult = {true};
}

//////////////////////////////////////////////////
// Items

void D2SFile::ParseItems(size_t &offset, std::vector<D2SItem> &items,
						 uint16_t &itemCount, D2SSectionResult &result)
{
	if (offset + 4 > m_data.size())
	{
		result = {false, "No item data (EOF)"};
		return;
	}

	if (!MatchMarker(offset, MARKER_ITEMS, 2))
	{
		size_t found = FindMarker(offset, MARKER_ITEMS, 2);
		if (found == SIZE_MAX)
		{
			result = {false, "Items marker 'JM' not found"};
			return;
		}
		offset = found;
	}

	offset += 2; // skip "JM"

	// Read item count (2 bytes LE)
	if (offset + 2 > m_data.size())
	{
		result = {false, "Item count truncated"};
		return;
	}
	memcpy(&itemCount, m_data.data() + offset, 2);
	offset += 2;

	items.clear();

	for (uint16_t i = 0; i < itemCount; i++)
	{
		D2SItem item;
		if (!ParseSingleItem(offset, item))
		{
			char buf[128];
			snprintf(buf, sizeof(buf), "Failed parsing item %u/%u at offset 0x%zX", i + 1, itemCount, offset);
			result = {false, buf};
			return;
		}
		items.push_back(item);

		// Parse socketed items (gems/runes/jewels inside this item)
		for (uint8_t s = 0; s < item.nGemCount; s++)
		{
			D2SItem socketedItem;
			if (!ParseSingleItem(offset, socketedItem))
			{
				char buf[128];
				snprintf(buf, sizeof(buf), "Failed parsing socketed item %u/%u of item %u at offset 0x%zX",
						 s + 1, item.nGemCount, i + 1, offset);
				result = {false, buf};
				return;
			}
			items.back().socketedItems.push_back(socketedItem);
		}
	}

	result = {true};
}

bool D2SFile::ParseSingleItem(size_t &offset, D2SItem &item)
{
	memset(&item, 0, sizeof(D2SItem) - sizeof(std::vector<D2SItem>));
	item.nBaseFlags = -1;

	if (offset + 2 > m_data.size())
		return false;

	// Verify JM marker
	if (!MatchMarker(offset, MARKER_ITEMS, 2))
		return false;

	size_t itemJmOffset = offset; // remember where this item's JM was
	offset += 2;				  // skip "JM"

	// Items are bit-packed from here
	D2SBitReader reader(m_data.data() + offset, m_data.size() - offset);

	try
	{
		// === Flag bits (after JM marker) ===
		reader.Skip(4);								  // bits 0-3: unknown
		item.bIdentified = reader.ReadBits(1) != 0;	  // bit 4
		reader.Skip(6);								  // bits 5-10: unknown
		item.bSocketed = reader.ReadBits(1) != 0;	  // bit 11
		reader.Skip(4);								  // bits 12-15: unknown
		item.bEar = reader.ReadBits(1) != 0;		  // bit 16
		reader.Skip(4);								  // bits 17-20: unknown
		item.bSimple = reader.ReadBits(1) != 0;		  // bit 21
		item.bEthereal = reader.ReadBits(1) != 0;	  // bit 22
		reader.Skip(1);								  // bit 23: unknown (often 1)
		item.bPersonalized = reader.ReadBits(1) != 0; // bit 24
		reader.Skip(1);								  // bit 25: unknown
		item.bRuneword = reader.ReadBits(1) != 0;	  // bit 26
		reader.Skip(15);							  // bits 27-41: version/unknown

		// === Location data ===
		item.nParent = static_cast<uint8_t>(reader.ReadBits(3));  // bits 42-44
		item.nBodyLoc = static_cast<uint8_t>(reader.ReadBits(4)); // bits 45-48
		item.nColumn = static_cast<uint8_t>(reader.ReadBits(4));  // bits 49-52
		item.nRow = static_cast<uint8_t>(reader.ReadBits(3));	  // bits 53-55 (was 4 before, corrected to 3)
		reader.Skip(1);											  // bit 56: unknown
		item.nStorage = static_cast<uint8_t>(reader.ReadBits(3)); // bits 57-59

		// === Item code (4 × 8 bits) ===
		char c0 = static_cast<char>(reader.ReadBits(8)); // bits 60-67
		char c1 = static_cast<char>(reader.ReadBits(8)); // bits 68-75
		char c2 = static_cast<char>(reader.ReadBits(8)); // bits 76-83
		char c3 = static_cast<char>(reader.ReadBits(8)); // bits 84-91 (should be ' ' = 32)

		item.szCode[0] = c0;
		item.szCode[1] = c1;
		item.szCode[2] = c2;
		item.szCode[3] = c3;
		item.szCode[4] = '\0';

		// Look up item base type (4-char code, 3-char items have ' ' as 4th)
		item.nBaseFlags = GetItemBaseFlags(item.szCode);

		if (item.nBaseFlags < 0)
		{
			fprintf(stderr, "  [DEBUG] Unknown item code '%.4s' (0x%02X 0x%02X 0x%02X 0x%02X) at bit %zu\n",
					item.szCode, (uint8_t)c0, (uint8_t)c1, (uint8_t)c2, (uint8_t)c3, reader.GetBitPosition() - 32);
		}

		// === Ear variant ===
		if (item.bEar)
		{
			reader.Skip(3); // ear class
			reader.Skip(7); // ear level
			// Read name (7-bit chars until 0)
			while (reader.ReadBits(7) != 0)
			{
				// consume name characters
			}
			reader.AlignToByte();
			offset += reader.GetCurrentByteOffset();
			return true;
		}

		// === Simple items ===
		if (item.bSimple)
		{
			reader.Skip(1); // unknown bit for simple items
			reader.AlignToByte();
			offset += reader.GetCurrentByteOffset();
			return true;
		}

		// === Extended items ===
		item.nGemCount = static_cast<uint8_t>(reader.ReadBits(3)); // socketed gem/rune count
		item.dwId = reader.ReadBits(32);						   // unique ID
		item.nILevel = static_cast<uint8_t>(reader.ReadBits(7));   // item level
		item.nQuality = static_cast<uint8_t>(reader.ReadBits(4));  // quality

		// Picture field (optional, for rings/amulets/charms)
		if (reader.ReadBits(1))
			reader.Skip(3);

		// Class-specific auto-mod (optional)
		if (reader.ReadBits(1))
			reader.Skip(11);

		// === Quality-specific data ===
		switch (item.nQuality)
		{
		case QUALITY_LOW: // 1
			reader.Skip(3);
			break;
		case QUALITY_NORMAL: // 2
			// no extra data
			break;
		case QUALITY_SUPERIOR: // 3
			reader.Skip(3);
			break;
		case QUALITY_MAGIC:	 // 4
			reader.Skip(11); // prefix
			reader.Skip(11); // suffix
			break;
		case QUALITY_SET:	 // 5
			reader.Skip(12); // set ID
			break;
		case QUALITY_RARE:	  // 6
		case QUALITY_CRAFTED: // 8
			reader.Skip(8);	  // rare name 1
			reader.Skip(8);	  // rare name 2
			// 6 optional prefix/suffix pairs
			for (int p = 0; p < 6; p++)
			{
				if (reader.ReadBits(1))
					reader.Skip(11);
			}
			break;
		case QUALITY_UNIQUE: // 7
			reader.Skip(12); // unique ID
			break;
		default:
			break;
		}

		// Runeword data
		if (item.bRuneword)
			reader.Skip(16); // 12-bit rune word ID + 4 unknown

		// Personalized name
		if (item.bPersonalized)
		{
			while (reader.ReadBits(7) != 0)
			{
				// consume name characters
			}
		}

		// Tome extra field
		int baseFlags = item.nBaseFlags;
		if (baseFlags < 0)
			baseFlags = 0; // unknown item: assume no special fields

		if (baseFlags & IBASE_TOME)
			reader.Skip(5);

		// Realm data / timestamp (1 bit flag + 96 bits if set)
		if (reader.ReadBits(1))
			reader.Skip(96);

		// === Item-specific data ===

		// Defense (armor)
		if (baseFlags & IBASE_DEFENSE)
		{
			item.nDefense = static_cast<uint16_t>(reader.ReadBits(11));
			// Defense is stored as value + 10
			if (item.nDefense >= 10)
				item.nDefense -= 10;
		}

		// Durability (armor and weapons)
		if (baseFlags & (IBASE_DEFENSE | IBASE_DURABILITY))
		{
			item.nMaxDur = static_cast<uint8_t>(reader.ReadBits(8));
			if (item.nMaxDur > 0) // 0 = indestructible
				item.nCurDur = static_cast<uint16_t>(reader.ReadBits(9));
		}

		// Quantity (stackable items)
		if (baseFlags & IBASE_QUANTITY)
			item.nQuantity = static_cast<uint16_t>(reader.ReadBits(9));

		// Socket count
		if (item.bSocketed)
			item.nTotalSockets = static_cast<uint8_t>(reader.ReadBits(4));

		// Set item bonus flags
		uint8_t setFlags = 0;
		if (item.nQuality == QUALITY_SET)
			setFlags = static_cast<uint8_t>(reader.ReadBits(5));

		// === Stat lists ===

		if (!ParseItemStats(reader, item.szCode))
			goto fallback;

		// Set bonus stat lists (up to 5)
		for (int b = 0; b < 5; b++)
		{
			if (setFlags & (1 << b))
			{
				if (!ParseItemStats(reader, item.szCode))
					goto fallback;
			}
		}

		// Runeword stat list
		if (item.bRuneword)
		{
			if (!ParseItemStats(reader, item.szCode))
				goto fallback;
		}

		reader.AlignToByte();
		offset += reader.GetCurrentByteOffset();
		return true;

	fallback:
		// Stat parsing failed (likely PD2 custom stat).
		// Scan for next JM marker starting just past this item's JM.
		// Using the item's JM offset (not the reader's position) ensures
		// consistent recovery regardless of how many bits stats consumed.
		{
			size_t search = itemJmOffset + 2;
			while (search + 1 < m_data.size())
			{
				if (m_data[search] == 'J' && m_data[search + 1] == 'M')
				{
					offset = search;
					return true;
				}
				search++;
			}
			offset = m_data.size();
			return true;
		}
	}
	catch (const std::runtime_error &)
	{
		return false;
	}
}

bool D2SFile::ParseItemStats(D2SBitReader &reader, const char *dbgCode)
{
	int nStatCount = 0;

	while (true)
	{
		size_t statStart = reader.GetBitPosition();
		uint32_t statId = reader.ReadBits(9);
		if (statId == D2S_STAT_END)
			break;

		if (statId >= NUM_SAVE_BITS || g_SaveBits[statId] == 0)
		{
			fprintf(stderr, "  [DEBUG] '%.4s' Unknown stat ID %u at bit %zu (stat #%d)\n",
					dbgCode ? dbgCode : "????", statId, statStart, nStatCount);
			return false;
		}

		uint32_t paramBits = g_SaveParamBits[statId];
		uint32_t valBits = g_SaveBits[statId];

		reader.Skip(paramBits + valBits);
		nStatCount++;

		// Ghidra-confirmed follow stats for damage pairs.
		// Auto-detect per occurrence: peek ahead to see if follow interpretation
		// yields a valid continuation (valid stat ID or 0x1FF terminator).
		uint32_t followCount = g_FollowStats[statId];
		if (followCount > 0)
		{
			size_t afterVal = reader.GetBitPosition();
			uint32_t totalFollowBits = 0;
			bool followOk = true;

			for (uint32_t f = 0; f < followCount && followOk; f++)
			{
				uint32_t fid = statId + 1 + f;
				if (fid >= NUM_SAVE_BITS || g_SaveBits[fid] == 0)
				{
					followOk = false;
					break;
				}
				totalFollowBits += g_SaveParamBits[fid] + g_SaveBits[fid];
			}

			if (followOk)
			{
				// Peek at 9-bit stat ID after follow data
				uint32_t nextFollow = reader.PeekBits(afterVal + totalFollowBits, 9);
				bool followValid = (nextFollow == D2S_STAT_END) ||
								   (nextFollow < NUM_SAVE_BITS && g_SaveBits[nextFollow] > 0);

				// Peek at 9-bit stat ID without follow (current position)
				uint32_t nextNoFollow = reader.PeekBits(afterVal, 9);
				bool noFollowValid = (nextNoFollow == D2S_STAT_END) ||
									 (nextNoFollow < NUM_SAVE_BITS && g_SaveBits[nextNoFollow] > 0);

				if (followValid && !noFollowValid)
				{
					// Only follow gives valid continuation
					reader.Skip(totalFollowBits);
					nStatCount += followCount;
				}
				else if (followValid && noFollowValid)
				{
					// Both valid - prefer follow (matches Ghidra-confirmed encoding)
					reader.Skip(totalFollowBits);
					nStatCount += followCount;
				}
				// else: don't consume follow bits
			}
		}
	}
	return true;
}

//////////////////////////////////////////////////
// Corpse section: JM header with corpse count
// Format: JM <corpse_count_u16> [corpse_count × 12 bytes metadata] [corpse_count × item_lists]

void D2SFile::ParseCorpse(size_t &offset)
{
	if (offset + 4 > m_data.size())
	{
		corpseResult = {true};
		return;
	}

	// Corpse section starts with JM header
	if (!MatchMarker(offset, MARKER_ITEMS, 2))
	{
		corpseResult = {false, "Corpse section missing JM marker"};
		return;
	}

	offset += 2; // skip "JM"

	if (offset + 2 > m_data.size())
	{
		corpseResult = {false, "Corpse count truncated"};
		return;
	}

	memcpy(&corpseCount, m_data.data() + offset, 2);
	offset += 2;

	if (corpseCount == 0)
	{
		corpseResult = {true};
		return;
	}

	// Skip corpse metadata (12 bytes per corpse)
	size_t metadataSize = static_cast<size_t>(corpseCount) * 12;
	if (offset + metadataSize > m_data.size())
	{
		corpseResult = {false, "Corpse metadata truncated"};
		return;
	}
	offset += metadataSize;

	// Parse item lists for each corpse
	for (uint16_t c = 0; c < corpseCount; c++)
	{
		ParseItems(offset, corpseItems, corpseItemCount, corpseResult);
		if (!corpseResult.ok)
			return;
	}

	corpseResult = {true};
}

//////////////////////////////////////////////////
// Merc/hireling section: jf marker + optional item list
// Format: jf [JM <item_count> <items>]  (only if merc exists)

void D2SFile::ParseMerc(size_t &offset)
{
	if (offset + 2 > m_data.size())
	{
		mercResult = {true};
		return;
	}

	if (!MatchMarker(offset, MARKER_MERC, 2))
	{
		// Search forward a short distance
		size_t found = FindMarker(offset, MARKER_MERC, 2);
		if (found == SIZE_MAX || found > offset + 16)
		{
			mercResult = {true};
			return;
		}
		offset = found;
	}

	offset += 2; // skip "jf"

	// If merc exists (has a type), parse merc item list
	if (header.mercData.wMercType != 0)
	{
		ParseItems(offset, mercItems, mercItemCount, mercResult);
	}
	else
	{
		mercResult = {true};
	}
}

//////////////////////////////////////////////////
// Iron Golem (expansion only)

void D2SFile::ParseGolem(size_t &offset)
{
	if (offset + 2 > m_data.size())
	{
		golemResult = {true}; // optional
		return;
	}

	if (!MatchMarker(offset, MARKER_GOLEM, 2))
	{
		size_t found = FindMarker(offset, MARKER_GOLEM, 2);
		if (found == SIZE_MAX)
		{
			golemResult = {true}; // no golem
			return;
		}
		offset = found;
	}

	offset += 2; // skip "kf"

	if (offset + 1 > m_data.size())
	{
		golemResult = {false, "Golem flag truncated"};
		return;
	}

	hasGolem = (m_data[offset] != 0);
	offset += 1;

	if (hasGolem)
	{
		// Parse the golem item
		D2SItem golemItem;
		if (!ParseSingleItem(offset, golemItem))
		{
			golemResult = {false, "Failed parsing golem item"};
			return;
		}
	}

	golemResult = {true};
}

//////////////////////////////////////////////////
// Utility

bool D2SFile::MatchMarker(size_t offset, const uint8_t *marker, size_t markerLen) const
{
	if (offset + markerLen > m_data.size())
		return false;
	return memcmp(m_data.data() + offset, marker, markerLen) == 0;
}

size_t D2SFile::FindMarker(size_t startOffset, const uint8_t *marker, size_t markerLen) const
{
	if (markerLen == 0 || startOffset + markerLen > m_data.size())
		return SIZE_MAX;

	for (size_t i = startOffset; i + markerLen <= m_data.size(); i++)
	{
		if (memcmp(m_data.data() + i, marker, markerLen) == 0)
			return i;
	}
	return SIZE_MAX;
}

const char *D2SFile::GetCharClassName() const
{
	if (header.nCharClass < NUM_CLASSES)
		return ClassNames[header.nCharClass];
	return "Unknown";
}

std::string D2SFile::GetStatusString() const
{
	std::string s;
	if (header.nCharStatus & STATUS_HARDCORE)
		s += "HC";
	if (header.nCharStatus & STATUS_DEAD)
	{
		if (!s.empty())
			s += "+";
		s += "Dead";
	}
	if (header.nCharStatus & STATUS_EXPANSION)
	{
		if (!s.empty())
			s += "+";
		s += "Exp";
	}
	if (header.nCharStatus & STATUS_LADDER)
	{
		if (!s.empty())
			s += "+";
		s += "Ldr";
	}
	if (s.empty())
		s = "SC";
	return s;
}

bool D2SFile::AllSectionsOK() const
{
	return headerResult.ok && crcResult.ok && questResult.ok &&
		   waypointResult.ok && npcResult.ok && statResult.ok &&
		   skillResult.ok && itemResult.ok && corpseResult.ok &&
		   mercResult.ok && golemResult.ok;
}

//////////////////////////////////////////////////
// Writer: write raw data to file with recomputed CRC

bool D2SFile::Write(const char *outPath)
{
	if (m_data.empty())
		return false;

	// Make a copy so we can update the CRC without modifying our state
	std::vector<uint8_t> out = m_data;

	// Zero out the CRC field (bytes 12-15) before computing
	out[12] = out[13] = out[14] = out[15] = 0;

	// Compute D2's rotate-left checksum
	uint32_t checksum = 0;
	for (size_t i = 0; i < out.size(); i++)
	{
		uint32_t carry = (checksum >> 31) & 1;
		checksum = (checksum << 1) | carry;
		checksum += out[i];
	}

	// Write the checksum back into bytes 12-15
	memcpy(&out[12], &checksum, 4);

	std::ofstream file(outPath, std::ios::binary);
	if (!file.is_open())
		return false;

	file.write(reinterpret_cast<const char *>(out.data()), out.size());
	return file.good();
}
