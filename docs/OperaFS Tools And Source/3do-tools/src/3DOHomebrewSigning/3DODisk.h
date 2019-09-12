#pragma once

#include <windows.h>

#include <vector>
#include <string>

using namespace std;
#define	VOLUME_SYNC_BYTE_LEN	5
#define	VOLUME_COM_LEN			32
#define	VOLUME_ID_LEN			32
#define ROOT_HIGHEST_AVATAR		7

struct sRomTag
{
	DWORD	TagType;
	DWORD	Offset;
	DWORD	Length;
};

class C3DODisk
{
	public:
		// Constructor
		C3DODisk();
		
		// Destructor
		~C3DODisk();

		// Sets the iso name to use
		// Finds the needs information from the iso
		bool SetISO(string _strFilename, bool _bGenRomTags);
		
		// Write changes
		void WriteChanges();
		
	protected:
		// Filename of the iso image
		string		m_strIsoName;

		// Rom tags
		vector<sRomTag>	m_vRomTags;

		// launchme starting sector and size
		int		m_iLaunchMeStart;
		int		m_iLaunchMeSize;

		// signatures starting sector and size
		int		m_iSignatureStart;
		int		m_iSignatureSize;

		// boot_code starting sector and size
		int		m_iBootCodeStart;
		int		m_iBootCodeSize;

		// Signature buffer
		BYTE	*m_pSignatures;
		
		// Boot RSA value
		BYTE	m_uBootRSA[64];
		
		// Sector count
		int		m_iSectorCount;
		
		// Start of the rom_tags entries
		int		m_iLaunchMeTagStart;
		int		m_iSignaturesTagStart;
		int		m_iBannerTagStart;
		
		// Read the rom tags
		bool ReadRomTags(FILE *Handle);

		// Fix ISO
		bool FixISO(FILE *Handle);

		// Calculate boot_code checksum
		bool CalculateBootCodeChecksum(FILE *Handle);
		
		// Calculate os_code checksum
		bool CalculateOSCodeChecksum(FILE *Handle);
		
		// Calculate misc_code checksum
		bool CalculateMiscCodeChecksum(FILE *Handle);
		
		// Calculate boot checksum
		bool CalculateBootChecksum(FILE *Handle);

		// Update bannerscreen checksum
		bool UpdateBannerChecksum(FILE *Handle);

		// Calculate signatures
		bool CalculateSignatures(FILE *Handle);

		// Fill in the signatures
		bool FillSignatures(FILE *Handle);

		// Write boot checksum
		void WriteBootChecksum(FILE *Handle);

		// Write signatures
		void WriteSignatures(FILE *Handle);

		// Find signature file
		bool FindRomTagFiles(FILE *Handle);

		// Generate rom tags
		bool GenerateRomTags(FILE *Handle);
};
