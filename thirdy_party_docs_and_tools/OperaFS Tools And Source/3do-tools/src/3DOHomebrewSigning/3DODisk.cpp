#include <stdio.h>
#include <assert.h>

#include "3DODisk.h"
#include "Utilities.h"

// Defines
static const int	s_iDiskLabelSize = 0x84;
static const int	s_iRomTagOffset = 0x800;
static const int	s_iRomTagSize = 32;
static const int	s_iUsedSectorsCount = 0x50;
static const int	s_iSignatureClear = 0x55;
static const int	s_iSignatureSize = 0x52000;
static const int	s_iDirEntrySize = 72;
static const int	s_iFilenameSize = 32;
static const int	s_iBannerStart = 0xE2;
static const int	s_iBannerLength = 0x25858;
static const char	s_szFiller[9] = "iamaduck";

static const BYTE	s_uRomTags[] =
{
	0x0F, 0x0D, 0x02, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x17, 0x6C,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0F, 0x07, 0x18, 0xE1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0xC3, 0x40,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0F, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAC, 0xBF, 0xF7, 0x92, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0F, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0xFF, 0x00, 0x00, 0x00, 0x78,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0F, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x45, 0x00, 0x00, 0x0B, 0x5C,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0F, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE2, 0x00, 0x02, 0x58, 0x58,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0F, 0x05, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

#define	WORDSwap(type) (((type >> 8) & 0x00FF) | ((type << 8)  & 0xFF00))
#define	DWORDSwap(type) (((type >> 24) & 0x000000FF) | ((type >> 8) & 0x0000FF00) | ((type << 8)  & 0x00FF0000) | ((type << 24) & 0xFF000000))

C3DODisk::C3DODisk() :
	m_pSignatures(NULL),
	m_iLaunchMeSize(-1),
	m_iSignatureSize(-1)
{
}

C3DODisk::~C3DODisk()
{
	if (m_pSignatures != NULL)
	{
		delete[] m_pSignatures;

		m_pSignatures = NULL;
	}
}

bool C3DODisk::SetISO(string _strFilename, bool _bGenRomTags)
{
	FILE	*Handle = fopen(_strFilename.c_str(), "r+b");

	if (NULL == Handle)
	{
		printf("Unable to load file %s\n", _strFilename.c_str());

		return	false;
	}

	// Save the iso name
	m_strIsoName = _strFilename;

	if (true == _bGenRomTags)
	{
		if (false == GenerateRomTags(Handle))
		{
			return	false;
		}
	}

	// Find signature file
	if (false == FindRomTagFiles(Handle))
	{
		return	false;
	}

	// Read rom tags
	if (false == ReadRomTags(Handle))
	{
		return	false;
	}

	// Read rom tags
	if (false == FixISO(Handle))
	{
		return	false;
	}

	// Calculate boot_code checksum
	if (false == CalculateBootCodeChecksum(Handle))
	{
		return	false;
	}

	// Calculate boot checksum
	if (false == CalculateBootChecksum(Handle))
	{
		return	false;
	}

	// Calculate os_code checksum
	if (false == CalculateOSCodeChecksum(Handle))
	{
		return	false;
	}

	// Calculate misc_code checksum
	if (false == CalculateMiscCodeChecksum(Handle))
	{
		return	false;
	}

	// Calculate bannerscreen checksums
	if (false == UpdateBannerChecksum(Handle))
	{
		return	false;
	}

	// Calculate signatures
	if (false == CalculateSignatures(Handle))
	{
		return	false;
	}

	fclose(Handle);

	return	true;
}

void C3DODisk::WriteChanges()
{
	FILE	*Handle = fopen(m_strIsoName.c_str(), "r+b");

	if (NULL == Handle)
	{
		printf("Unable to save to %s\n", m_strIsoName.c_str());

		return;
	}

	// Write boot checksum
	WriteBootChecksum(Handle);

	// Write signatures
	WriteSignatures(Handle);
}

bool C3DODisk::ReadRomTags(FILE *Handle)
{
	printf("\nSearching for rom tags...\n\n");

	fseek(Handle, s_iRomTagOffset, SEEK_SET);

	while (true)
	{
		sRomTag	RomTag;

		DWORD	Value;

		// Read tag type
		fread(&Value, sizeof(DWORD), 1, Handle);
		Value = DWORDSwap(Value);

		if (Value != 0x00000000 && (Value & 0xFF000000) != 0x0F000000)
		{
			printf("Rom tags corrupted, or not a 3DO image\n");

			return	false;
		}

		RomTag.TagType = (Value & 0x00FF0000) >> 16;

		// Read data
		fread(&Value, sizeof(DWORD), 1, Handle);

		fread(&Value, sizeof(DWORD), 1, Handle);
		RomTag.Offset = DWORDSwap(Value);

		fread(&Value, sizeof(DWORD), 1, Handle);
		RomTag.Length = DWORDSwap(Value);

		fread(&Value, sizeof(DWORD), 1, Handle);
		fread(&Value, sizeof(DWORD), 1, Handle);
		fread(&Value, sizeof(DWORD), 1, Handle);
		fread(&Value, sizeof(DWORD), 1, Handle);

		switch (RomTag.TagType)
		{
		case 2:
			printf("Found launchme tag:\n");
			printf("\tStarting sector %X\n", RomTag.Offset);
			printf("\tLength %X\n", RomTag.Length);

			m_iLaunchMeTagStart = ftell(Handle) - 32;

			break;

		case 5:
			// Sector is one off
			RomTag.Offset++;

			printf("Found signatures tag:\n");
			printf("\tStarting sector %X\n", m_iSignatureStart - 1);
			printf("\tLength %X\n", RomTag.Length);

			m_iSignaturesTagStart = ftell(Handle) - 32;

			break;

		case 7:
			// Sector is one off
			RomTag.Offset++;

			printf("Found os_code tag:\n");
			printf("\tStarting sector %X\n", RomTag.Offset);
			printf("\tLength %X\n", RomTag.Length);

			break;

		case 0x0C:
			printf("Found release data tag:\n");
			printf("\tEncoded date %X\n", RomTag.Offset);

			break;

		case 0x0D:
			// Sector is one off
			RomTag.Offset++;

			printf("Found boot_code tag:\n");
			printf("\tStarting sector %X\n", RomTag.Offset);
			printf("\tLength %X\n", RomTag.Length);

			break;

		case 0x10:
			// Sector is one off
			RomTag.Offset++;

			printf("Found misc_code tag:\n");
			printf("\tStarting sector %X\n", RomTag.Offset);
			printf("\tLength %X\n", RomTag.Length);

			break;

		case 0x14:
			// Sector is one off
			RomTag.Offset++;

			printf("Found bannerscreen tag:\n");
			printf("\tStarting sector %X\n", RomTag.Offset);
			printf("\tLength %X\n", RomTag.Length);

			m_iBannerTagStart = ftell(Handle) - 32;

			break;

		case 0:
			printf("Found end tag\n");

			break;

		default:
			printf("Unknown tag %X:\n", RomTag.TagType);
			printf("\tStarting sector %X\n", RomTag.Offset);
			printf("\tLength %X\n", RomTag.Length);
		}

		m_vRomTags.push_back(RomTag);

		if (0 == RomTag.TagType)
		{
			printf("\nFound %d tags\n\n", m_vRomTags.size());

			break;
		}
	}

	return	true;
}

bool C3DODisk::FixISO(FILE *Handle)
{
	DWORD	Value;

	printf("Fixing the ISO image...\n");

	// Read the sector count from the disk label
	fseek(Handle, s_iUsedSectorsCount, SEEK_SET);
	fread(&Value, sizeof(DWORD), 1, Handle);

	m_iSectorCount = DWORDSwap(Value);

	// Round sector count up to nearest 16.
	if ((m_iSectorCount & 0x0F) != 0)
	{
		m_iSectorCount -= m_iSectorCount & 0x0F;
		m_iSectorCount += 0x10;
	}

	// The signature size is the number of disk sector, plus the signature sector.
	m_iSignatureSize = m_iSectorCount + 2048;

	// Pad the image
	fseek(Handle, 0, SEEK_END);

	int	iLength = ftell(Handle);
	int	iNewLength = m_iSectorCount * 2048;

	if (iLength < iNewLength)
	{
		printf("Padding the image...\n");

		int	iWriteLength = iNewLength - iLength;

		for (int iLoop = 0; iLoop < iWriteLength; ++iLoop)
		{
			BYTE	Value = s_szFiller[iLoop % 8];

			fwrite(&Value, 1, 1, Handle);
		}
	}

	if (iLength != iNewLength)
	{
		// Update used sector count
		iNewLength /= 2048;
		iNewLength = DWORDSwap(iNewLength);

		printf("Updating the used sector count...\n");

		fseek(Handle, s_iUsedSectorsCount, SEEK_SET);
		fwrite(&iNewLength, sizeof(DWORD), 1, Handle);
	}

	// Update launchme tag
	printf("Writing launchme start...\n");

	Value = m_iLaunchMeStart - 1;
	Value = DWORDSwap(Value);

	fseek(Handle, m_iLaunchMeTagStart + 8, SEEK_SET);
	fwrite(&Value, sizeof(DWORD), 1, Handle);

	printf("Writing launchme size...\n");

	Value = m_iLaunchMeSize;
	Value = DWORDSwap(Value);

	fwrite(&Value, sizeof(DWORD), 1, Handle);

	// Update signatures tag
	printf("Writing signature start...\n");

	Value = m_iSignatureStart - 1;
	Value = DWORDSwap(Value);

	fseek(Handle, m_iSignaturesTagStart + 8, SEEK_SET);
	fwrite(&Value, sizeof(DWORD), 1, Handle);

	printf("Writing signature size...\n");

	Value = DWORDSwap(m_iSignatureSize);
	fwrite(&Value, sizeof(DWORD), 1, Handle);

	// Update bannerscreen tag
	printf("Writing bannerscreen start...\n");

	Value = s_iBannerStart;
	Value = DWORDSwap(Value);

	fseek(Handle, m_iBannerTagStart + 8, SEEK_SET);
	fwrite(&Value, sizeof(DWORD), 1, Handle);

	printf("Writing bannerscreen size...\n");

	Value = s_iBannerLength;
	Value = DWORDSwap(Value);

	fwrite(&Value, sizeof(DWORD), 1, Handle);

	return	true;
}

bool C3DODisk::CalculateBootCodeChecksum(FILE *Handle)
{
	int	iRomTagsSize = (int)m_vRomTags.size();

	printf("\nCalculating boot_code checksum...\n\n");

	m_iBootCodeSize = -1;

	// Find boot_code
	for (int iLoop = 0; iLoop < iRomTagsSize; ++iLoop)
	{
		if (0x0D == m_vRomTags[iLoop].TagType)
		{
			m_iBootCodeStart = m_vRomTags[iLoop].Offset;
			m_iBootCodeSize = m_vRomTags[iLoop].Length;

			break;
		}
	}

	if (-1 == m_iBootCodeSize)
	{
		printf("Unable to find a boot_code rom tag entry\n");

		return	false;
	}

	BYTE	*pBootCode = new BYTE[m_iBootCodeSize];

	if (NULL == pBootCode)
	{
		printf("Unable to allocated enough memory for the boot_code files\n");

		return	false;
	}

	// Read boot_code
	fseek(Handle, m_iBootCodeStart * 2048, SEEK_SET);
	fread(pBootCode, m_iBootCodeSize, 1, Handle);

	BYTE	uBootMD5[16];

	CalculateMD5(pBootCode, m_iBootCodeSize - 64, uBootMD5);

	// Boot MD5 value
	char	szBootMD5[64];

	szBootMD5[0] = 0;

	for (int iLoop = 0; iLoop < sizeof(uBootMD5); ++iLoop)
	{
		char	szHexDigits[4];

		sprintf(szHexDigits, "%02X", uBootMD5[iLoop]);

		strcat(szBootMD5, szHexDigits);
	}

	printf("boot_code MD5 value is %s\n\n", szBootMD5);

	CalculateRSA(szBootMD5, m_uBootRSA, true);

	printf("boot_code RDS value is ");

	for (int iLoop = 0; iLoop < sizeof(m_uBootRSA); ++iLoop)
	{
		printf("%02X", m_uBootRSA[iLoop]);
	}

	printf("\n\n");

	delete[] pBootCode;

	printf("Writing boot_code checksum...\n");

	fseek(Handle, m_iBootCodeStart * 2048 + m_iBootCodeSize - 64, SEEK_SET);
	fwrite(m_uBootRSA, 1, 64, Handle);

	return	true;
}

bool C3DODisk::CalculateOSCodeChecksum(FILE *Handle)
{
	int	iRomTagsSize = (int)m_vRomTags.size();

	printf("\nCalculating os_code checksum...\n\n");

	int	iOSCodeSize = -1;
	int	iOSCodeOffset;

	// Find os_code
	for (int iLoop = 0; iLoop < iRomTagsSize; ++iLoop)
	{
		if (7 == m_vRomTags[iLoop].TagType)
		{
			iOSCodeSize = m_vRomTags[iLoop].Length;
			iOSCodeOffset = m_vRomTags[iLoop].Offset * 2048;

			break;
		}
	}

	if (-1 == iOSCodeSize)
	{
		printf("Unable to find a os_code rom tag entry\n");

		return	false;
	}

	BYTE	*pOSCode = new BYTE[iOSCodeSize];

	if (NULL == pOSCode)
	{
		printf("Unable to allocated enough memory for the os_code files\n");

		return	false;
	}

	// Read os_code
	fseek(Handle, iOSCodeOffset, SEEK_SET);
	fread(pOSCode, iOSCodeSize, 1, Handle);

	BYTE	uOSMD5[16];

	CalculateMD5(pOSCode, iOSCodeSize - 64, uOSMD5);

	// OS MD5 value
	char	szOSMD5[64];

	szOSMD5[0] = 0;

	for (int iLoop = 0; iLoop < sizeof(uOSMD5); ++iLoop)
	{
		char	szHexDigits[4];

		sprintf(szHexDigits, "%02X", uOSMD5[iLoop]);

		strcat(szOSMD5, szHexDigits);
	}

	printf("os_code MD5 value is %s\n\n", szOSMD5);

	BYTE	uOSRSA[64];

	CalculateRSA(szOSMD5, uOSRSA, true);

	printf("os_code RDS value is ");

	for (int iLoop = 0; iLoop < sizeof(uOSRSA); ++iLoop)
	{
		printf("%02X", uOSRSA[iLoop]);
	}

	printf("\n\n");

	delete[] pOSCode;

	printf("Writing os_code checksum...\n\n");

	fseek(Handle, iOSCodeOffset + iOSCodeSize - 64, SEEK_SET);
	fwrite(uOSRSA, 1, 64, Handle);

	return	true;
}

bool C3DODisk::CalculateMiscCodeChecksum(FILE *Handle)
{
	int	iRomTagsSize = (int)m_vRomTags.size();

	printf("\nCalculating misc_code checksum...\n\n");

	int	iMiscCodeSize = -1;
	int	iMiscCodeOffset;

	// Find misc_code
	for (int iLoop = 0; iLoop < iRomTagsSize; ++iLoop)
	{
		if (0x10 == m_vRomTags[iLoop].TagType)
		{
			iMiscCodeSize = m_vRomTags[iLoop].Length;
			iMiscCodeOffset = m_vRomTags[iLoop].Offset * 2048;

			break;
		}
	}

	if (-1 == iMiscCodeSize)
	{
		printf("Unable to find a misc_code rom tag entry\n");

		return	false;
	}

	BYTE	*pMiscCode = new BYTE[iMiscCodeSize];

	if (NULL == pMiscCode)
	{
		printf("Unable to allocated enough memory for the misc_code files\n");

		return	false;
	}

	// Read misc_code
	fseek(Handle, iMiscCodeOffset, SEEK_SET);
	fread(pMiscCode, iMiscCodeSize, 1, Handle);

	BYTE	uMiscMD5[16];

	CalculateMD5(pMiscCode, iMiscCodeSize - 64, uMiscMD5);

	// Misc MD5 value
	char	szMiscMD5[64];

	szMiscMD5[0] = 0;

	for (int iLoop = 0; iLoop < sizeof(uMiscMD5); ++iLoop)
	{
		char	szHexDigits[4];

		sprintf(szHexDigits, "%02X", uMiscMD5[iLoop]);

		strcat(szMiscMD5, szHexDigits);
	}

	printf("misc_code MD5 value is %s\n\n", szMiscMD5);

	BYTE	uMiscRSA[64];

	CalculateRSA(szMiscMD5, uMiscRSA, true);

	printf("misc_code RDS value is ");

	for (int iLoop = 0; iLoop < sizeof(uMiscRSA); ++iLoop)
	{
		printf("%02X", uMiscRSA[iLoop]);
	}

	printf("\n\n");

	delete[] pMiscCode;

	printf("Writing misc_code checksum...\n\n");

	fseek(Handle, iMiscCodeOffset + iMiscCodeSize - 64, SEEK_SET);
	fwrite(uMiscRSA, 1, 64, Handle);

	return	true;
}

bool C3DODisk::CalculateBootChecksum(FILE *Handle)
{
	int	iRomTagsSize = (int)m_vRomTags.size();
	int iDataSize = s_iDiskLabelSize;

	printf("\nCollecting data for boot checksum...\n\n");

	// Add in the size of the rom tags
	iDataSize += iRomTagsSize * s_iRomTagSize;

	// Add in the size of the boot code
	iDataSize += m_iBootCodeSize;

	BYTE	*pBootCode = new BYTE[iDataSize];

	if (NULL == pBootCode)
	{
		printf("Unable to allocated enough memory for the boot checksum files\n");

		return	false;
	}

	int	iReadOffset = 0;

	// Read the disk label
	fseek(Handle, 0, SEEK_SET);
	fread(&pBootCode[iReadOffset], s_iDiskLabelSize, 1, Handle);
	iReadOffset += s_iDiskLabelSize;

	// Read the rom tags
	fseek(Handle, s_iRomTagOffset, SEEK_SET);
	fread(&pBootCode[iReadOffset], iRomTagsSize * s_iRomTagSize, 1, Handle);
	iReadOffset += iRomTagsSize * s_iRomTagSize;

	// Read boot code
	fseek(Handle, m_iBootCodeStart * 2048, SEEK_SET);
	fread(&pBootCode[iReadOffset], m_iBootCodeSize, 1, Handle);
	iReadOffset += m_iBootCodeSize;

	assert(iReadOffset == iDataSize);

	BYTE	uBootMD5[16];

	CalculateMD5(pBootCode, iDataSize, uBootMD5);

	// Boot MD5 value
	char	szBootMD5[64];

	szBootMD5[0] = 0;

	for (int iLoop = 0; iLoop < sizeof(uBootMD5); ++iLoop)
	{
		char	szHexDigits[4];

		sprintf(szHexDigits, "%02X", uBootMD5[iLoop]);

		strcat(szBootMD5, szHexDigits);
	}

	printf("Boot MD5 value is %s\n\n", szBootMD5);

	CalculateRSA(szBootMD5, m_uBootRSA);

	printf("Boot RDS value is ");

	for (int iLoop = 0; iLoop < sizeof(m_uBootRSA); ++iLoop)
	{
		printf("%02X", m_uBootRSA[iLoop]);
	}

	printf("\n\n");

	delete[] pBootCode;

	return	true;
}

bool C3DODisk::UpdateBannerChecksum(FILE *Handle)
{
	printf("Calculating the banner checksum...\n\n");

	int	iBannerSize = -1;
	int	iBannerOffset = -1;

	int	iRomTagsSize = (int)m_vRomTags.size();

	// Find banner screen
	for (int iLoop = 0; iLoop < iRomTagsSize; ++iLoop)
	{
		if (0x14 == m_vRomTags[iLoop].TagType)
		{
			iBannerSize = m_vRomTags[iLoop].Length;
			iBannerOffset = m_vRomTags[iLoop].Offset * 2048;

			break;
		}
	}

	if (-1 == iBannerSize)
	{
		printf("Unable to find a banner screen rom tag entry\n");

		return	false;
	}

	BYTE	*pBannerBuffer = new BYTE[iBannerSize - 64];

	if (NULL == pBannerBuffer)
	{
		printf("Unable to allocate memory for the banner screen\n");

		return	false;
	}

	fseek(Handle, iBannerOffset, SEEK_SET);

	fread(pBannerBuffer, 1, iBannerSize - 64, Handle);

	BYTE	uBannerMD5[16];

	CalculateMD5(pBannerBuffer, iBannerSize - 64, uBannerMD5);

	// Banner MD5 value
	char	szBannerMD5[64];

	szBannerMD5[0] = 0;

	for (int iLoop = 0; iLoop < sizeof(uBannerMD5); ++iLoop)
	{
		char	szHexDigits[4];

		sprintf(szHexDigits, "%02X", uBannerMD5[iLoop]);

		strcat(szBannerMD5, szHexDigits);
	}

	printf("Banner MD5 value is %s\n\n", szBannerMD5);

	// Banner RSA value
	BYTE	uBannerRSA[64];

	CalculateRSA(szBannerMD5, uBannerRSA);

	printf("Banner RDS value is ");

	for (int iLoop = 0; iLoop < sizeof(uBannerRSA); ++iLoop)
	{
		printf("%02X", uBannerRSA[iLoop]);
	}

	printf("\n\n");

	delete[] pBannerBuffer;

	printf("Writing the banner checksum...\n\n");

	fseek(Handle, iBannerOffset + iBannerSize - 64, SEEK_SET);

	fwrite(uBannerRSA, 1, 64, Handle);

	return	true;
}

bool C3DODisk::CalculateSignatures(FILE *Handle)
{
	printf("Filling in the signatures...\n\n");

	printf("Signtures space used: %X\n", m_iSignatureSize);

	m_pSignatures = new BYTE[s_iSignatureSize];

	if (NULL == m_pSignatures)
	{
		printf("Unable to allocate memory for the signatures\n");

		return	false;
	}

	// Clear the signature memory
	memset(m_pSignatures, s_iSignatureClear, s_iSignatureSize);

	if (false == FillSignatures(Handle))
	{
		return	false;
	}

	return	true;
}

bool C3DODisk::FillSignatures(FILE *Handle)
{
	// Calculate the start and end skip sectors.
	int	iStartSkip = (m_iSignatureStart)& 0xFFFFFFF0;
	int	iEndSkip = (m_iSignatureStart)+(s_iSignatureSize / 2048);

	if ((iEndSkip & 0xF) != 0)
	{
		iEndSkip &= 0xFFFFFFF0;
		iEndSkip += 0x10;
	}

	// Seek to the start of the cdrom
	fseek(Handle, 0, SEEK_SET);

	BYTE	*pSignatures = m_pSignatures;

	int	iSignatures = m_iSectorCount;

	for (int iLoop = 0; iLoop < iSignatures * 2048; iLoop += 16 * 2048)
	{
		BYTE	Buffer[16 * 2048];

		fread(Buffer, 2048, 16, Handle);

		int	iSector = iLoop / 2048;

		// Do not calculate MD5 for the disk label, sector 0xE0(?), or the signature sectors.
		// 0x80E0 is sometimes ignored
		if (0 == iSector || 0xE0 == iSector || 0x80F0 == iSector || (iSector >= iStartSkip && iSector < iEndSkip))
		{
			memset(pSignatures, 0, 16);
		}

		else
		{
			CalculateMD5(Buffer, 16 * 2048, pSignatures);
		}

		pSignatures += 16;
	}

	// Clear out the sector after the signatures
	memset(pSignatures, 0, 2048);

	// Advance to the RSA area.
	pSignatures += 2048 - 64;

	// Calculate MD5 on the signatures
	BYTE	uSignatureMD5[16];

	CalculateMD5(m_pSignatures, m_iSignatureSize - 64, uSignatureMD5);

	// Convert MD5 to a string
	char	szSignatureMD5[64];

	szSignatureMD5[0] = 0;

	for (int iLoop = 0; iLoop < sizeof(uSignatureMD5); ++iLoop)
	{
		char	szHexDigits[4];

		sprintf(szHexDigits, "%02X", uSignatureMD5[iLoop]);

		strcat(szSignatureMD5, szHexDigits);
	}

	printf("Signatures MD5 value is %s\n\n", szSignatureMD5);

	// Calcute RSA on the MD5.
	CalculateRSA(szSignatureMD5, pSignatures);

	printf("Signatures RDS value is ");

	for (int iLoop = 0; iLoop < 64; ++iLoop)
	{
		printf("%02X", pSignatures[iLoop]);
	}

	printf("\n\n");

	return	true;
}

void C3DODisk::WriteBootChecksum(FILE *Handle)
{
	int	iChecksumOffset = s_iRomTagOffset + (int)m_vRomTags.size() * 32;

	printf("Writing boot checksum...\n");

	fseek(Handle, iChecksumOffset, SEEK_SET);
	fwrite(m_uBootRSA, 1, 64, Handle);
}

void C3DODisk::WriteSignatures(FILE *Handle)
{
	printf("Writing signatures...\n");

	fseek(Handle, m_iSignatureStart * 2048, SEEK_SET);
	fwrite(m_pSignatures, 1, s_iSignatureSize, Handle);
}

bool C3DODisk::FindRomTagFiles(FILE *Handle)
{
	// Find the root directory
	printf("Finding the rom tag files...\n");

	// The last 32 bytes contain sector numbers for the root directory. (Multiple copies?)
	fseek(Handle, s_iDiskLabelSize - 32, SEEK_SET);

	DWORD	Value;

	// Read the starting sector number for the root directory;
	fread(&Value, sizeof(DWORD), 1, Handle);

	Value = DWORDSwap(Value);
	Value *= 2048;

	// Skip the header info
	fseek(Handle, Value, SEEK_SET);

	// Read the number of directory entries
	fseek(Handle, 12, SEEK_CUR);

	fread(&Value, sizeof(DWORD), 1, Handle);

	DWORD	NumEntries = DWORDSwap(Value) / s_iDirEntrySize;

	// Read Offset
	fread(&Value, sizeof(DWORD), 1, Handle);

	Value = DWORDSwap(Value) - 0x14;

	// Skip to first entry
	if (Value > 0)
	{
		fseek(Handle, Value, SEEK_CUR);
	}

	for (DWORD iLoop = 0; iLoop < NumEntries; ++iLoop)
	{
		char	EntryName[s_iFilenameSize];

		// Read the type
		fread(&Value, sizeof(DWORD), 1, Handle);

		Value = DWORDSwap(Value);

		// Skip flags and stuff
		fseek(Handle, 12, SEEK_CUR);

		// Get the file size
		fread(&Value, sizeof(DWORD), 1, Handle);

		int	iFileSize = DWORDSwap(Value);

		// Skip flags and stuff
		fseek(Handle, 12, SEEK_CUR);

		// Read the filename
		fread(EntryName, 1, s_iFilenameSize, Handle);

		// Found the signature file, get the starting sector number
		if (!_stricmp("launchme", EntryName))
		{
			// Skip the entry count
			fseek(Handle, 4, SEEK_CUR);

			// Get the starting address
			fread(&Value, sizeof(DWORD), 1, Handle);

			m_iLaunchMeStart = DWORDSwap(Value);
			m_iLaunchMeSize = iFileSize;

			printf("Found launchme at sector %X\n", m_iLaunchMeStart);
		}

		// Found the signature file, get the starting sector number
		else if (!_stricmp("signatures", EntryName))
		{
			// Skip the entry count
			fseek(Handle, 4, SEEK_CUR);

			// Get the starting address
			fread(&Value, sizeof(DWORD), 1, Handle);

			m_iSignatureStart = DWORDSwap(Value);
			m_iSignatureSize = iFileSize;

			printf("Found signatures at sector %X\n", m_iSignatureStart);
		}

		else
		{
			// Get the entry count
			fread(&Value, sizeof(DWORD), 1, Handle);

			Value = DWORDSwap(Value);

			if (Value > 0)
			{
				fseek(Handle, Value * 4, SEEK_CUR);
			}

			// Skip the rest
			fseek(Handle, 4, SEEK_CUR);
		}
	}

	return	true;
}

bool C3DODisk::GenerateRomTags(FILE *Handle)
{
	printf("Generating rom tags...\n\n");

	fseek(Handle, s_iRomTagOffset, SEEK_SET);

	fwrite(s_uRomTags, 1, sizeof(s_uRomTags), Handle);

	return	true;
}