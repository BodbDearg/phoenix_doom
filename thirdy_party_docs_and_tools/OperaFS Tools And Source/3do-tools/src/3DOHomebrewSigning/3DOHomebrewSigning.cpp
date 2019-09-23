// 3DOHomebrewSigning.cpp : Defines the entry point for the console application.
//

#include <stdio.h>

#include "stdafx.h"

#include "3DODisk.h"

int main(int argc, char *argv[]) 
{
	bool	bGenRomTags	= false;
	
	printf("3DO homebrew encryption v0.6a\n");
	printf("\tBy Charles Doty\n\n");
	
	if (argc < 2)
	{
		printf("Usage: 3DOHomebrewSigning [genromtags] File.iso\n");
    exit(1);
	}

	C3DODisk	*p3DODisk	= new C3DODisk();

	// Set the generate rom tags flag 
	if (!strcmp("genromtags", argv[1]))
	{
		bGenRomTags	= true;
	}
		
	// Set the iso name, and parse the needed info
	if (false == p3DODisk->SetISO(argv[argc - 1], bGenRomTags))
	{
		// Some error happened
		return	1;
	}

	// Write changes
	p3DODisk->WriteChanges();
}
