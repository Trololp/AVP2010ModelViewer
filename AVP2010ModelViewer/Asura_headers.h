#pragma once
#include "MainClass_Viewer_Screen.h"

struct asura16b_hdr
{
	DWORD magic;
	DWORD chunk_size;
	DWORD type1;
	DWORD type2;
};

struct asura24b_hdr
{
	DWORD magic;
	DWORD chunk_size;
	DWORD type1;
	DWORD type2;
	DWORD amount1;
	DWORD amount2;
};