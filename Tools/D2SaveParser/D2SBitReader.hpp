#pragma once
#include <cstdint>
#include <cstddef>
#include <stdexcept>

// Lightweight bit reader for D2S stat and item parsing.
// Reads bits LSB-first (little-endian bit order) which is how D2 stores bit-packed data.
class D2SBitReader
{
public:
	D2SBitReader() : m_data(nullptr), m_size(0), m_bitPos(0) {}

	D2SBitReader(const uint8_t *data, size_t sizeBytes)
		: m_data(data), m_size(sizeBytes), m_bitPos(0) {}

	void SetData(const uint8_t *data, size_t sizeBytes)
	{
		m_data = data;
		m_size = sizeBytes;
		m_bitPos = 0;
	}

	// Read up to 32 bits, LSB-first
	uint32_t ReadBits(int numBits)
	{
		if (numBits <= 0 || numBits > 32)
			throw std::runtime_error("ReadBits: invalid bit count");

		uint32_t result = 0;
		for (int i = 0; i < numBits; i++)
		{
			size_t byteIndex = m_bitPos / 8;
			int bitIndex = m_bitPos % 8;

			if (byteIndex >= m_size)
				throw std::runtime_error("ReadBits: read past end of data");

			if (m_data[byteIndex] & (1 << bitIndex))
				result |= (1u << i);

			m_bitPos++;
		}
		return result;
	}

	// Read bits and sign-extend
	int32_t ReadBitsSigned(int numBits)
	{
		uint32_t raw = ReadBits(numBits);
		// Sign extend: if the top bit is set, fill upper bits with 1s
		if (numBits < 32 && (raw & (1u << (numBits - 1))))
			raw |= ~((1u << numBits) - 1);
		return static_cast<int32_t>(raw);
	}

	// Skip bits
	void Skip(int numBits)
	{
		m_bitPos += numBits;
	}

	// Get current bit position
	size_t GetBitPosition() const { return m_bitPos; }

	// Set absolute bit position
	void SetBitPosition(size_t pos) { m_bitPos = pos; }

	// Get remaining bits
	size_t GetRemainingBits() const
	{
		size_t totalBits = m_size * 8;
		return (m_bitPos < totalBits) ? (totalBits - m_bitPos) : 0;
	}

	// Align to next byte boundary
	void AlignToByte()
	{
		if (m_bitPos % 8 != 0)
			m_bitPos = ((m_bitPos / 8) + 1) * 8;
	}

	// Get raw data pointer at current byte offset
	const uint8_t *GetCurrentBytePtr() const
	{
		return m_data + (m_bitPos / 8);
	}

	size_t GetCurrentByteOffset() const
	{
		return m_bitPos / 8;
	}

	// Peek bits at an absolute bit position without advancing
	uint32_t PeekBits(size_t absPos, int numBits) const
	{
		if (numBits <= 0 || numBits > 32)
			return 0;
		uint32_t result = 0;
		for (int i = 0; i < numBits; i++)
		{
			size_t byteIndex = (absPos + i) / 8;
			int bitIndex = (absPos + i) % 8;
			if (byteIndex >= m_size)
				return result;
			if (m_data[byteIndex] & (1 << bitIndex))
				result |= (1u << i);
		}
		return result;
	}

private:
	const uint8_t *m_data;
	size_t m_size;
	size_t m_bitPos;
};
