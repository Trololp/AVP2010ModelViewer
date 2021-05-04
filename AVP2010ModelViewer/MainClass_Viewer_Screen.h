#pragma once
#include "AVP2010ModelViewer.h"
#include <Windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>

// C runtime
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <vector>
#include <algorithm>

using namespace DirectX;
#pragma comment(lib, "HACKS.lib")
#pragma warning(disable : 4996)

//Macro
#define SFPS(pos) SetFilePointer(f, pos, 0, FILE_BEGIN)
#define SFPC(pos) SetFilePointer(f, pos, 0, FILE_CURRENT)
#define READ(v) ReadFile(f, &(v), sizeof(v), &a, NULL)
#define READP(p, n) ReadFile(f, p, n, &a, NULL)

//Defines
#define NULL_ROT { 0.0f, 0.0f, 0.0f, 1.0f }

//Structures

struct simple_vertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
};

struct model_vtx
{
	XMFLOAT4 Pos;
	UINT8 BlendIndices[4];
	SHORT BlendWeight[4];
	SHORT Tangent[4];
	SHORT Binormal[4];
	SHORT Normal[4];
	XMFLOAT2 TexCoord1;
	float TextCoord2;
};

typedef struct
{
	DWORD id = 0;
	DWORD hash = 0;
	ID3D11ShaderResourceView* pSRView = nullptr;
	char* path = nullptr;
} avp_texture;

struct Material
{
	DWORD mat_hash;
	DWORD mat_num;
	DWORD texture_diff_id = 0;
	DWORD texture_norm_id = 0;
	DWORD texture_diff2_id = 0;
	DWORD texture_alpha_id = 0;
	bool is_alpha;
};

struct bbox
{
	XMFLOAT3 p1;
	XMFLOAT3 p2;
	XMFLOAT4 Color;
	XMFLOAT4 rot = NULL_ROT;  //quaternion rotation
};

struct asura_bbox
{
	float x1;
	float x2;
	float y1;
	float y2;
	float z1;
	float z2;
};

struct line
{
	XMFLOAT3 p1;
	XMFLOAT3 p2;
	XMFLOAT4 Color;
};

struct ConstantBuffer
{
	XMMATRIX mWorld;
	XMMATRIX mView;
	XMMATRIX mProjection;
	XMMATRIX mAnimBonesMatrix[128];
};

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
//HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT InitDevice();
void CleanupDevice();
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void Render();
LRESULT CALLBACK WndProc_2(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void MovementFunc_();

