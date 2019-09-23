#pragma once

#include <windows.h>

// Calculate MD5 checksum
void CalculateMD5(BYTE *_pBuffer, int _iDataSize, BYTE *_pMD5);

// Calculate RSA
void CalculateRSA(char *pMessage, BYTE *_pRSA, bool _bUseKey2 = false);
