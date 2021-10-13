#include "stdafx.h"
#include "MainClass_Viewer_Screen.h"
#include "Debug.h"
#include "Keyboard.h"
#include "Hash_tree.h"
#include "Textures.h"
#include "Models.h"
#include "DDSLoader.h"
#include <SpriteFont.h>
#include "Console.h"
#include "Drawable_info.h"
#include "ViewFrustrum.h"

#pragma comment(lib, "d3d11.lib")

using namespace DirectX;

//Extern Globals
extern int screen_width;
extern int screen_height;
extern hash_tree_node* hash_tree_texture;
extern hash_tree_node2* model_hashs;
extern hash_tree_node2* model_skeletons;
extern hash_tree_node2* model_skeleton_anims;
extern hash_tree_node2* model_HMPTs;
extern model_incremental_info g_mmi[1000];
extern anim_incremental_info g_anm[2000];
extern XMMATRIX g_axBoneMatrices[128];
extern DWORD g_loaded_anim_count;
extern DWORD g_loaded_mdl_count;
extern Command cmd_dump_anim;
extern Command cmd_dump_model;

// Globals
float ScreenNear = 0.1f;
float ScreenFar = 500.0f;
float vis_radius = 600.0f;
float g_max_FPS_value = 60.0f;
float g_dT = (float)1 / 60.0f;



avp_texture AVP_TEXTURES[1500]; // Textures
ID3D11Device*           g_pd3dDevice = nullptr;
ID3D11DeviceContext*    g_pImmediateContext = nullptr;
ID3D11Buffer*		    g_pVertexBuffer_generic_bbox = nullptr; // generic bboxes
ID3D11Buffer*			g_pVertexBuffer_lines = nullptr;
ID3D11Buffer*			g_dynamic_buffer_lines = nullptr; // Entities bboxes and stuff

HINSTANCE               g_hInst = nullptr;
HWND                    g_hWnd = nullptr;
D3D_DRIVER_TYPE         g_driverType = D3D_DRIVER_TYPE_REFERENCE;
D3D_FEATURE_LEVEL       g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device1*          g_pd3dDevice1 = nullptr;
ID3D11DeviceContext1*   g_pImmediateContext1 = nullptr;
IDXGISwapChain*         g_pSwapChain = nullptr;
IDXGISwapChain1*        g_pSwapChain1 = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11VertexShader*     g_pVertexShader_m = nullptr; // for model
ID3D11PixelShader*      g_pPixelShader_m = nullptr;
ID3D11InputLayout*      g_pVertexLayout_m = nullptr;
ID3D11VertexShader*     g_pVertexShader_s = nullptr; // for lines and bbox
ID3D11PixelShader*      g_pPixelShader_s = nullptr;
ID3D11InputLayout*      g_pVertexLayout_s = nullptr;

ID3D11Buffer*           g_pConstantBuffer = nullptr;
ID3D11RasterizerState*  g_Env_RS = nullptr;
ID3D11RasterizerState*  g_Wireframe_RS = nullptr;
ID3D11DepthStencilState*	g_pDSState_normal = nullptr;

ID3D11DepthStencilState*	g_pDSState_Zbuffer_disabled = nullptr;

ID3D11DepthStencilView* g_pDSV = nullptr;
ID3D11Texture2D*		g_pDepthStencilBuffer = nullptr;
ID3D11ShaderResourceView* g_pDefaultTexture = nullptr;
ID3D11SamplerState*     g_pSS = nullptr;
ID3D11BlendState*		g_pBlendStateNoBlend = nullptr;

XMMATRIX                g_World;
XMMATRIX                g_View;
XMMATRIX                g_Projection;
XMMATRIX				g_OrthoMatrix;
XMMATRIX				g_Matrix_Identity = XMMatrixIdentity(); // identity matrix

ViewFrustum				g_frustum; // frustrum class for blocking unseen points and objects

Console*				g_pConsole = nullptr;

DWORD					g_points_stuff;
DWORD					g_points_lines;
std::vector <bbox>		g_bboxes;
std::vector <line>		g_lines;

// Camera
float					pos_x = -5.0f;
float					pos_y = 0.0f;
float					pos_z = 0.0f;
float					g_alpha = 0.0f;
float					g_beta = 0.0f;
int						g_click_x;
int						g_click_y; // unused now
bool					LMB_click = 0; // unused now too


//Floor
int						g_lines_floor_width = 10; // amount of floor lines
float					g_lines_floor_distance = 1.0f; // distance between lines

std::unique_ptr<DirectX::SpriteFont> m_font;
XMVECTOR				g_Model_counter_font_pos = { screen_width - 200.0f, 200.0f };
XMVECTOR				g_DataFontPos = { screen_width - 200.0f, 300.0f };
std::unique_ptr<DirectX::SpriteBatch> m_spriteBatch;
char                    g_NotifyNoModel[100] = "No model !!!";
char                    g_NotifyOnPause[100] = "On pause !!!";
XMVECTOR				m_NotifyNoModelTextPos = { (float)(screen_width / 2), 50.0f };
char                    g_model_counter[256] = { 0 };
XMVECTOR				m_model_name_pos = { (float)(screen_width / 2), 100.0f };
XMVECTOR				g_anim_name_pos = { (float)(screen_width / 2), 150.0f };
char					g_anim_info[250] = { 0 };

model_info*				g_default_model;

wchar_t* g_cwd;
wchar_t* g_path1 = nullptr; // path for [Models]
char* g_model_names[1000] = { 0 }; // this for dialog box
char* g_anim_names[2000] = { 0 };

DWORD g_selected_model_index = 0;
DWORD g_selected_anim_index = 0;

float g_current_anim_time = 0.0f;
DWORD g_anim_setup_bones = 1;

// States
bool g_bDrawEmptyModelNotification = false;
bool g_animate = true;
bool g_show_node_pts = false;
bool g_show_HSSB = false;
bool g_show_skl = true;
bool g_show_HMPT = true;

int g_framecount = 0;


HRESULT CompileShaderFromFile(const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;

	// Disable optimizations to further improve shader debugging
	dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ID3DBlob* pErrorBlob = nullptr;
	hr = D3DCompileFromFile(szFileName, nullptr, nullptr, szEntryPoint, szShaderModel,
		dwShaderFlags, 0, ppBlobOut, &pErrorBlob);
	if (FAILED(hr))
	{
		if (pErrorBlob)
		{
			OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
			dbgprint("ShaderCompiler", "Error: %s\n", reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
			pErrorBlob->Release();
		}
		return hr;
	}
	if (pErrorBlob) pErrorBlob->Release();

	return S_OK;
}

//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
	dbgprint("Init", "Executable Started !\n");

	HRESULT hr = S_OK;

	RECT rc;
	GetClientRect(g_hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	DXGI_SWAP_CHAIN_DESC sd = { 0 };

	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.RefreshRate.Numerator = (int)g_max_FPS_value;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 1;
	sd.OutputWindow = g_hWnd;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Windowed = TRUE;
	sd.Flags = 0;


	if (D3D11CreateDeviceAndSwapChain(0, D3D_DRIVER_TYPE_HARDWARE, 0, 0, featureLevels, 3, D3D11_SDK_VERSION,
		&sd, &g_pSwapChain, &g_pd3dDevice, 0, &g_pImmediateContext) < 0
		|| !g_pd3dDevice
		|| !g_pImmediateContext
		|| !g_pSwapChain)
	{
		CleanupDevice(); // can be bugged
		dbgprint("Init", "Failed to initialise D3D11.\n");
		MessageBoxA(g_hWnd, "Failed to initialise D3D11.", "Viewer", 0x10u);
		exit(1);
	}


	// Obtain DXGI factory from device (since we used nullptr for pAdapter above)
	IDXGIFactory1* dxgiFactory = nullptr;
	{
		IDXGIDevice* dxgiDevice = nullptr;
		hr = g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
		if (SUCCEEDED(hr))
		{
			IDXGIAdapter* adapter = nullptr;
			hr = dxgiDevice->GetAdapter(&adapter);
			if (SUCCEEDED(hr))
			{
				hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
				adapter->Release();
			}
			dxgiDevice->Release();
		}
	}
	if (FAILED(hr))
		return hr;

	dxgiFactory->Release();

	if (FAILED(hr))
		return hr;

	// Create a render target view
	ID3D11Texture2D* pBackBuffer = nullptr;
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
	if (FAILED(hr))
		return hr;

	hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr))
		return hr;



	D3D11_TEXTURE2D_DESC DSBuffdesc;
	ZeroMemory(&DSBuffdesc, sizeof(DSBuffdesc));
	DSBuffdesc.Width = width;
	DSBuffdesc.Height = height;
	DSBuffdesc.MipLevels = 1;
	DSBuffdesc.ArraySize = 1;
	DSBuffdesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DSBuffdesc.SampleDesc.Count = 1;
	DSBuffdesc.SampleDesc.Quality = 0;
	DSBuffdesc.Usage = D3D11_USAGE_DEFAULT;
	DSBuffdesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	DSBuffdesc.CPUAccessFlags = 0;
	DSBuffdesc.MiscFlags = 0;

	hr = g_pd3dDevice->CreateTexture2D(&DSBuffdesc, NULL, &g_pDepthStencilBuffer);
	if (FAILED(hr))
	{
		dbgprint("Init", "Error create depth buffer %x\n", hr);
		return hr;
	}

	

	// Compile the vertex shader
	ID3DBlob* pVSBlob2 = nullptr;
	hr = CompileShaderFromFile(L"Shaders_line.fx", "VS", "vs_4_0", &pVSBlob2);
	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		dbgprint("Init", "VS (line) compile failed %x \n", hr);
		return hr;
	}

	// Create the vertex shader
	hr = g_pd3dDevice->CreateVertexShader(pVSBlob2->GetBufferPointer(), pVSBlob2->GetBufferSize(), nullptr, &g_pVertexShader_s);
	if (FAILED(hr))
	{
		dbgprint("Init", "VS (line) create failed %x \n", hr);
		pVSBlob2->Release();
		return hr;
	}

	// Define the input layout
	D3D11_INPUT_ELEMENT_DESC layout2[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0, D3D11_INPUT_PER_VERTEX_DATA,  0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements2 = ARRAYSIZE(layout2);

	// Create the input layout
	hr = g_pd3dDevice->CreateInputLayout(layout2, numElements2, pVSBlob2->GetBufferPointer(),
		pVSBlob2->GetBufferSize(), &g_pVertexLayout_s);
	pVSBlob2->Release();
	if (FAILED(hr))
	{
		dbgprint("Init", "CreateInputLayout (line) failed : %d\n", hr);
		return hr;
	}

	// Compile the model shader
	ID3DBlob* pPSBlob2 = nullptr;
	hr = CompileShaderFromFile(L"Shaders_line.fx", "PS", "ps_4_0", &pPSBlob2);
	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		dbgprint("Init", "PS (line) create failed %x \n", hr);
		return hr;
	}

	// Create the pixel shader
	hr = g_pd3dDevice->CreatePixelShader(pPSBlob2->GetBufferPointer(), pPSBlob2->GetBufferSize(), nullptr, &g_pPixelShader_s);
	pPSBlob2->Release();
	if (FAILED(hr))
		return hr;

	// Compile the vertex shader
	ID3DBlob* pVSBlob3 = nullptr;
	hr = CompileShaderFromFile(L"Shaders_model.hlsl", "VS", "vs_4_0", &pVSBlob3);
	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		dbgprint("Init", "VS (model) compile failed %x \n", hr);
		return hr;
	}

	// Create the vertex shader
	hr = g_pd3dDevice->CreateVertexShader(pVSBlob3->GetBufferPointer(), pVSBlob3->GetBufferSize(), nullptr, &g_pVertexShader_m);
	if (FAILED(hr))
	{
		dbgprint("Init", "VS (model) create failed %x \n", hr);
		pVSBlob3->Release();
		return hr;
	}

	// Define the input layout
	D3D11_INPUT_ELEMENT_DESC layout3[] =
	{
		{ "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT,      0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BLENDWEIGHT",  0, DXGI_FORMAT_R16G16B16A16_SNORM, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT",      0, DXGI_FORMAT_R16G16B16A16_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BINORMAL",     0, DXGI_FORMAT_R16G16B16A16_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",       0, DXGI_FORMAT_R16G16B16A16_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",     0, DXGI_FORMAT_R16G16B16A16_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",     1, DXGI_FORMAT_R32_FLOAT,          0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements3 = ARRAYSIZE(layout3);

	// Create the input layout
	hr = g_pd3dDevice->CreateInputLayout(layout3, numElements3, pVSBlob3->GetBufferPointer(),
		pVSBlob3->GetBufferSize(), &g_pVertexLayout_m);
	pVSBlob3->Release();
	if (FAILED(hr))
	{
		dbgprint("Init", "CreateInputLayout (model) failed : %d\n", hr);
		return hr;
	}

	// Compile the pixel shader
	ID3DBlob* pPSBlob3 = nullptr;
	hr = CompileShaderFromFile(L"Shaders_model.hlsl", "PS", "ps_4_0", &pPSBlob3);
	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		dbgprint("Init", "PS (model) create failed %x \n", hr);
		return hr;
	}

	// Create the pixel shader
	hr = g_pd3dDevice->CreatePixelShader(pPSBlob3->GetBufferPointer(), pPSBlob3->GetBufferSize(), nullptr, &g_pPixelShader_m);
	pPSBlob3->Release();
	if (FAILED(hr))
		return hr;

	D3D11_DEPTH_STENCIL_DESC DSDesc;
	DSDesc.DepthEnable = TRUE;
	DSDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	DSDesc.DepthFunc = D3D11_COMPARISON_LESS;
	DSDesc.StencilEnable = TRUE;
	DSDesc.StencilReadMask = 0xFF;
	DSDesc.StencilWriteMask = 0xFF;
	DSDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	DSDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	DSDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	DSDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	DSDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	DSDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	DSDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	DSDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	hr = g_pd3dDevice->CreateDepthStencilState(&DSDesc, &g_pDSState_normal);
	if (FAILED(hr))
	{
		dbgprint("Init", "CreateDepthStencilState failed : %d\n", hr);
		return hr;
	}

	DSDesc.DepthEnable = FALSE;
	DSDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	DSDesc.DepthFunc = D3D11_COMPARISON_LESS;
	DSDesc.StencilEnable = FALSE;
	DSDesc.StencilReadMask = 0xFF;
	DSDesc.StencilWriteMask = 0xFF;
	DSDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	DSDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	DSDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	DSDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	DSDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	DSDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	DSDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	DSDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	hr = g_pd3dDevice->CreateDepthStencilState(&DSDesc, &g_pDSState_Zbuffer_disabled);
	if (FAILED(hr))
	{
		dbgprint("Init", "CreateDepthStencilState Zbuffer disabled failed : %d\n", hr);
		return hr;
	}

	g_pImmediateContext->OMSetDepthStencilState(g_pDSState_normal, 1);
	D3D11_DEPTH_STENCIL_VIEW_DESC DSVdesc;
	ZeroMemory(&DSVdesc, sizeof(DSVdesc));

	DSVdesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DSVdesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	DSVdesc.Texture2D.MipSlice = 0;

	hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencilBuffer, &DSVdesc, &g_pDSV);
	if (FAILED(hr))
	{
		dbgprint("Init", "CreateDepthStencilView failed : %d\n", hr);
		return hr;
	}

	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDSV);


	D3D11_RASTERIZER_DESC RDesc;
	ZeroMemory(&RDesc, sizeof(RDesc));

	RDesc.FillMode = D3D11_FILL_SOLID;
	RDesc.CullMode = D3D11_CULL_BACK;
	RDesc.DepthBias = 0;
	RDesc.DepthBiasClamp = 0;
	RDesc.SlopeScaledDepthBias = 0;
	RDesc.DepthClipEnable = 1;
	RDesc.ScissorEnable = 0;
	RDesc.MultisampleEnable = 0;
	RDesc.AntialiasedLineEnable = 0;
	RDesc.FrontCounterClockwise = 1;
	hr = g_pd3dDevice->CreateRasterizerState(&RDesc, &g_Env_RS);
	if (FAILED(hr))
	{
		dbgprint("Init", "CreateRasterizerState failed : %d\n", hr);
		return hr;
	}
	g_pImmediateContext->RSSetState(g_Env_RS);

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pImmediateContext->RSSetViewports(1, &vp);

	D3D11_BLEND_DESC BlendState;
	ZeroMemory(&BlendState, sizeof(D3D11_BLEND_DESC));
	BlendState.RenderTarget[0].BlendEnable = FALSE;
	BlendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = g_pd3dDevice->CreateBlendState(&BlendState, &g_pBlendStateNoBlend);
	if (FAILED(hr))
	{
		dbgprint("Init", "CreateBlendState Failed %d\n", hr);
	}


	//g_bboxes.push_back({ XMFLOAT3(1.0f,1.0f,1.0f),XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f,1.0f,1.0f,1.0f) });

	if (!g_path1)
	{
		dbgprint("Init", "No path was chosen\n");
		exit(0);
	}

	g_cwd = _wgetcwd(0, 0);
	wchar_t Resource_str[260] = { 0 };


	model_hashs = new hash_tree_node2;
	model_skeletons = new hash_tree_node2;
	model_skeleton_anims = new hash_tree_node2;
	model_HMPTs = new hash_tree_node2;
	hash_tree_texture = new hash_tree_node;
	hash_tree_texture->Hash = 0;
	hash_tree_texture->bigger = nullptr;
	hash_tree_texture->smaller = nullptr;
	hash_tree_texture->texture = nullptr;

	if (g_path1)
	{
		lstrcpyW(Resource_str, g_path1);
		lstrcatW(Resource_str, L"\\graphics\\");

		if (!Read_textures(Resource_str))
		{
			dbgprint("Init", "Read Textures 2 Failed\n");
		}

		lstrcpyW(Resource_str, g_path1);
		lstrcatW(Resource_str, L"\\MARE_chunk\\*.MARE");

		if (!Read_materials(Resource_str))
		{
			dbgprint("Init", "Read Materials 2 Failed\n");
		}

		lstrcpyW(Resource_str, g_path1);
		lstrcatW(Resource_str, L"\\*.RSCF");

		if (!Read_models(Resource_str))
		{
			dbgprint("Init", "Read Models failed \n");
		}

		lstrcpyW(Resource_str, g_path1);
		lstrcatW(Resource_str, L"\\HSKN_chunk\\*.HSKN");

		if (!Read_HSKNs(Resource_str))
		{
			dbgprint("Init", "Read skeleton failed \n");
		}

		Update_Incremental_MMI_HSKN();
		Make_static_skeleton_lines();

		lstrcpyW(Resource_str, g_path1);
		lstrcatW(Resource_str, L"\\HANM_chunk\\*.HANM");

		if (!Read_HANMs(Resource_str))
		{
			dbgprint("Init", "Read anim failed \n");
		}

		lstrcpyW(Resource_str, g_path1);
		lstrcatW(Resource_str, L"\\HSBB_chunk\\*.HSBB");

		if (!Read_HSBBs(Resource_str))
		{
			dbgprint("Init", "Read HSBB failed \n");
		}
		/*
		lstrcpyW(Resource_str, g_path1);
		lstrcatW(Resource_str, L"\\HMPT_chunk\\*.HMPT");

		if (!Read_HMPTs(Resource_str))
		{
			dbgprint("Init", "Read HMPT failed \n");
		}
		Update_Incremental_MMI_HMPT();

		dbgprint("HMPT", "Update_Incremental_MMI_HMPT passed !\n");
		*/
	}

	wsprintf(Resource_str, L"%ws\\default.RSCF", g_cwd);

	Read_model(Resource_str, 1);
	hash_tree_node2* hn2_p = search_by_hash2(model_hashs, 0x18405); // Default mdl (is a cup model)

	if (!hn2_p)
	{
		dbgprint("Init", "Default model not loaded \n");
		exit(0);
	}

	for (UINT i = 0; i < g_loaded_mdl_count; i++)
	{
		g_model_names[i] = g_mmi[i].model_name;
	}

	for (UINT i = 0; i < g_loaded_anim_count; i++)
	{
		g_anim_names[i] = g_anm[i].anim_name;
	}

	g_default_model = (model_info*)hn2_p->ptr;

	wsprintf(Resource_str, L"%ws\\default.dds", g_cwd);

	CreateDDSTextureFromFile(g_pd3dDevice, Resource_str, nullptr, &g_pDefaultTexture);
	if (!g_pDefaultTexture)
	{
		dbgprint("Init", "Default texture not loaded \n");
		exit(0);
	}

	for (int i = -g_lines_floor_width / 2; i <= g_lines_floor_width / 2; i++)
	{
		float d = g_lines_floor_distance;
		XMFLOAT3 pos1 = { (float)(g_lines_floor_width / 2)*d, 0, ((float)i)*d };
		XMFLOAT3 pos2 = { -(float)(g_lines_floor_width / 2)*d, 0, ((float)i)*d };
		XMFLOAT3 pos3 = { ((float)i)*d, 0,  (float)(g_lines_floor_width / 2)*d };
		XMFLOAT3 pos4 = { ((float)i)*d, 0, -(float)(g_lines_floor_width / 2)*d };


		g_lines.push_back({ pos1, pos2,{ 1.0f, 1.0f, 1.0f, 1.0f } });
		g_lines.push_back({ pos3, pos4,{ 1.0f, 1.0f, 1.0f, 1.0f } });
	}

	if (!bbox_to_vertex(g_bboxes, &g_points_stuff))
	{
		dbgprint("Init", "BBoxes failed !\n");
	}// bboxes

	if (!line_to_vertex(g_lines, &g_points_lines))
	{
		dbgprint("Init", "lines failed !\n");
	}// lines

	D3D11_BUFFER_DESC bd = {};
	// Create the constant buffer
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(ConstantBuffer);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bd.MiscFlags = 0;
	bd.StructureByteStride = 0;
	hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pConstantBuffer);
	if (FAILED(hr))
		return hr;

	// Initialize the world matrix
	g_World = XMMatrixIdentity();

	// Initialize the view matrix
	XMVECTOR Eye = XMVectorSet(0.0f, 1.0f, -5.0f, 0.0f);
	XMVECTOR At = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR Up = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);
	g_View = XMMatrixLookAtLH(Eye, At, Up);

	// Initialize the projection matrix
	g_Projection = XMMatrixPerspectiveFovRH(XM_PIDIV2, width / (FLOAT)height, ScreenNear, ScreenFar);

	D3D11_RASTERIZER_DESC desc;
	ZeroMemory(&desc, sizeof(D3D11_RASTERIZER_DESC));
	desc.FillMode = D3D11_FILL_WIREFRAME;
	desc.CullMode = D3D11_CULL_BACK;
	g_pd3dDevice->CreateRasterizerState(&desc, &g_Wireframe_RS);

	g_OrthoMatrix = XMMatrixOrthographicLH((float)width, (float)height, ScreenNear, ScreenFar);

	D3D11_SAMPLER_DESC SD;
	ZeroMemory(&SD, sizeof(SD));

	// Create a texture sampler state description.
	SD.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SD.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	SD.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	SD.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	SD.MipLODBias = 0.0f;
	SD.MaxAnisotropy = 1;
	SD.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	SD.BorderColor[0] = 0;
	SD.BorderColor[1] = 0;
	SD.BorderColor[2] = 0;
	SD.BorderColor[3] = 0;
	SD.MinLOD = 0;
	SD.MaxLOD = D3D11_FLOAT32_MAX;

	// Create the texture sampler state.
	hr = g_pd3dDevice->CreateSamplerState(&SD, &g_pSS);
	if (FAILED(hr))
	{
		dbgprint("Init", "CreateSamplerState failed %x \n", hr);
		return hr;
	}

	m_font = std::make_unique<SpriteFont>(g_pd3dDevice, L"font.spitefont");
	m_spriteBatch = std::make_unique<SpriteBatch>(g_pImmediateContext);

	g_pConsole = new Console(5, 25.0f, Colors::Red, 0.5f);

	g_pConsole->Add_command(&cmd_dump_anim);
	g_pConsole->Add_command(&cmd_dump_model);

	return S_OK;
}



//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
	if (g_pImmediateContext) g_pImmediateContext->ClearState();

	if (g_pConstantBuffer) g_pConstantBuffer->Release();
	if (g_pVertexLayout_m) g_pVertexLayout_m->Release();
	if (g_pVertexShader_m) g_pVertexShader_m->Release();
	if (g_pPixelShader_m) g_pPixelShader_m->Release();
	if (g_pRenderTargetView) g_pRenderTargetView->Release();
	if (g_pSwapChain1) g_pSwapChain1->Release();
	if (g_pSwapChain) g_pSwapChain->Release();
	if (g_pImmediateContext1) g_pImmediateContext1->Release();
	if (g_pImmediateContext) g_pImmediateContext->Release();
	if (g_pd3dDevice1) g_pd3dDevice1->Release();
	if (g_pd3dDevice) g_pd3dDevice->Release();

	std::vector<bbox>().swap(g_bboxes);
	std::vector<line>().swap(g_lines);
	delete_mat_list();
	delete_nodes(hash_tree_texture);
	Delete_models();
	m_font.reset();
	m_spriteBatch.reset();
}

LRESULT CALLBACK WndProc_2(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	switch (message)
	{
	case WM_CHAR:
		KeyChar(wParam, lParam);
		break;
	case WM_KEYDOWN:
		KeyDown(wParam, lParam);
		break;
	case WM_KEYUP:
		KeyUp(wParam, lParam);
		break;

	case WM_MOUSEMOVE:
		MouseLookUp(wParam, lParam);
		break;

	case WM_RBUTTONDOWN:
		MouseRMB(1, wParam, lParam);
		break;

	case WM_RBUTTONUP:
		MouseRMB(0, wParam, lParam);
		break;

	case WM_LBUTTONDOWN:
		LMB_click = true;
		g_click_x = LOWORD(lParam);
		g_click_y = HIWORD(lParam);
		break;

	default:
		return 0;
	}

	return 0;
}

//---------------------------------------------------
// Render a frame
//--------------------------------------------------------------------------------------
void Render()
{
	// Constant FPS is 60

	g_pImmediateContext->OMSetDepthStencilState(g_pDSState_normal, 1);
	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDSV);
	g_pImmediateContext->RSSetState(g_Env_RS);
	g_pImmediateContext->OMSetBlendState(g_pBlendStateNoBlend, 0, 0xFFFFFFFF);

	g_frustum.ConsturctFrustum(ScreenFar / 16, g_Projection, g_View);

	//
	// Clear the back buffer
	//
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, Colors::MidnightBlue);
	g_pImmediateContext->ClearDepthStencilView(g_pDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
	//
	// Update variables
	//

	model_incremental_info* Current_mmi = &(g_mmi[g_selected_model_index]);
	model_info* mi = Current_mmi->mi;
	anim_incremental_info* Current_anim = &(g_anm[g_selected_anim_index]);

	if (g_anim_setup_bones || g_show_HSSB)
	{
		Skeleton_setup_bone_anim_matrices();
		struct1 ke = { Current_mmi->skl, Current_mmi->dynamic_skeleton_buffer, Current_mmi->dynamic_skeleton_points };
		Call_1(&ke);
		g_anim_setup_bones = 0;
		if (g_show_HSSB)
			g_animate = false;
	}

	// Animation
	if (Current_anim)
	{
		if (g_animate)
			Current_anim->current_anim_time += g_dT;


		if (Current_anim->current_anim_time >= Current_anim->total_time)
			Current_anim->current_anim_time -= Current_anim->total_time;

		if (!g_show_HSSB)
		{
			Model_anim_update_init(Current_mmi->skl, Current_anim->anim, Current_anim->current_anim_time);
			struct1 ke = { Current_mmi->skl, Current_mmi->dynamic_skeleton_buffer, Current_mmi->dynamic_skeleton_points };
			Call_1(&ke);
		}
	}
	


	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = g_pImmediateContext->Map(g_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(hr))
	{
		dbgprint("Draw", "Failed to update constant buffer %x \n", hr);
		CleanupDevice();
		exit(0);
	}

	ConstantBuffer* pCb = (ConstantBuffer*)mappedResource.pData;
	pCb->mWorld = XMMatrixTranspose(g_World);
	pCb->mView = XMMatrixTranspose(g_View);
	pCb->mProjection = XMMatrixTranspose(g_Projection);

	memcpy(&(pCb->mAnimBonesMatrix), g_axBoneMatrices, 128 * sizeof(XMMATRIX));

	g_pImmediateContext->Unmap(g_pConstantBuffer, 0);

	/*
	g_pImmediateContext->VSSetShader(g_pVertexShader_s, nullptr, 0);
	g_pImmediateContext->PSSetShader(g_pPixelShader_s, nullptr, 0);
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout_s);
	//g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	g_pImmediateContext->IASetIndexBuffer(NULL, DXGI_FORMAT_R16_UINT, 0);
	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	UINT stride = sizeof(simple_vertex);
	UINT offset = 0;

	if (Show_info())
	{
		if (Current_mmi->skl && g_show_skl)
		{
			g_pImmediateContext->IASetVertexBuffers(0, 1, &(Current_mmi->dynamic_skeleton_buffer), &stride, &offset);
			g_pImmediateContext->Draw(Current_mmi->points_skeleton, 0);
		}
	}*/

	g_pImmediateContext->VSSetShader(g_pVertexShader_m, nullptr, 0);
	g_pImmediateContext->PSSetShader(g_pPixelShader_m, nullptr, 0);
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout_m);
	UINT stride = sizeof(model_vtx);
	UINT offset = 0;

	if (mi->vertex_layout_triangle_list)
	{
		g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}
	else
	{
		g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	}

	g_pImmediateContext->PSSetSamplers(0, 1, &g_pSS);

	if (mi->empty_model)
	{
		// Draw text this is empty model !!!!
		g_bDrawEmptyModelNotification = true;
		mi = g_default_model;
	}
	else
	{
		g_bDrawEmptyModelNotification = false;
	}
	

	g_pImmediateContext->IASetVertexBuffers(0, 1, &(mi->pVertexBuff), &stride, &offset);
	g_pImmediateContext->IASetIndexBuffer(mi->pIndexBuff, DXGI_FORMAT_R16_UINT, 0);
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	int start_index = 0;
	list_node* ln;
	for (unsigned int i = 0; i < mi->mmi_count; i++)
	{
		model_mat_info* mmi = (mi->mmi + i);
		if ((ln = search_mat_by_hash(mmi->mat_hash)))
		{
			if (AVP_TEXTURES[ln->material->texture_diff_id].pSRView)
			{
				g_pImmediateContext->PSSetShaderResources(0, 1, &(AVP_TEXTURES[ln->material->texture_diff_id].pSRView));
			}
			else
			{
				g_pImmediateContext->PSSetShaderResources(0, 1, &g_pDefaultTexture);
			}
		}
		else
		{
			g_pImmediateContext->PSSetShaderResources(0, 1, &g_pDefaultTexture);
		}
		g_pImmediateContext->DrawIndexed(mmi->points_count, start_index, 0);
		start_index += mmi->points_count;
	}

	g_pImmediateContext->VSSetShader(g_pVertexShader_s, nullptr, 0);
	g_pImmediateContext->PSSetShader(g_pPixelShader_s, nullptr, 0);
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout_s);
	//g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	g_pImmediateContext->IASetIndexBuffer(NULL, DXGI_FORMAT_R16_UINT, 0);
	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	stride = sizeof(simple_vertex);
	offset = 0;
	
	if (true) // this is flat plane !!! for now
	{
		g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer_lines, &stride, &offset);
		g_pImmediateContext->Draw(g_points_lines, 0);

		g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer_generic_bbox, &stride, &offset);
		g_pImmediateContext->Draw(g_points_stuff, 0);// Draw stuff (bboxes)
	}

	g_pImmediateContext->OMSetDepthStencilState(g_pDSState_Zbuffer_disabled, 1);

	if (Show_info())
	{
		if (Current_mmi->skl && g_show_skl)
		{
			g_pImmediateContext->IASetVertexBuffers(0, 1, &(Current_mmi->dynamic_skeleton_buffer), &stride, &offset);
			g_pImmediateContext->Draw(Current_mmi->points_skeleton, 0);
		}

		if (Current_mmi->static_HSBB_buffer && g_show_HSSB)
		{
			g_pImmediateContext->IASetVertexBuffers(0, 1, &(Current_mmi->static_HSBB_buffer), &stride, &offset);
			g_pImmediateContext->Draw(Current_mmi->points_HSBB, 0);
		}
	}

	g_pImmediateContext->OMSetDepthStencilState(g_pDSState_normal, 1);

	m_spriteBatch->Begin();
	XMVECTOR origin;

	// Console Draw func
	g_pConsole->Draw();

	if (Show_info())
	{
		if (g_bDrawEmptyModelNotification)
		{
			origin = m_font->MeasureString((const char*)g_NotifyNoModel) / 2.f;
			m_font->DrawString(m_spriteBatch.get(), (const char*)g_NotifyNoModel, m_NotifyNoModelTextPos, Colors::White, 0.f, origin);
		}

		if (!g_animate)
		{
			origin = m_font->MeasureString((const char*)g_NotifyOnPause) / 2.f;
			m_font->DrawString(m_spriteBatch.get(), (const char*)g_NotifyOnPause, m_NotifyNoModelTextPos, Colors::White, 0.f, origin);
		}
		
		if (Current_mmi->skl && g_show_node_pts)
		{
			for (int i = 0; i < Current_mmi->skl->amount_bones; i++)
			{
				skeleton_bone* pSklNode = &(Current_mmi->skl->pBones[i]);
				DWORD index = pSklNode->parent_bone_id;
				XMVECTOR pos = pSklNode->bone_model_matrix.r[3];
				pos = XMVector3Transform(pos, g_axBoneMatrices[index]);

				if (!(const char*)(pSklNode->name))
					continue;

				if (g_frustum.CheckPoint({pos.m128_f32[0], pos.m128_f32[1], pos.m128_f32[2]}))
				{
					XMVECTOR cam_pos = { pos_y, pos_z, pos_x };
					
					float d = XMVector3Length(pos - cam_pos).m128_f32[0];
					if (d <= 3.0f)
					{
						XMVECTOR rel_pos = XMVector3Transform(pos, g_View); // x, y, z in camera coords
						if ((abs(rel_pos.m128_f32[0] / rel_pos.m128_f32[2]) <= 1.0f && abs(rel_pos.m128_f32[1] / rel_pos.m128_f32[2]) <= 1.0f) && pSklNode->name)
						{
							XMVECTOR text_pos = { screen_height*(-rel_pos.m128_f32[0] / rel_pos.m128_f32[2] + 1.77f) / 2, screen_height*(+rel_pos.m128_f32[1] / rel_pos.m128_f32[2] + 1.0f) / 2 };
							XMVECTOR tilt = { 2.0f, 2.0f };
							XMVECTOR origin = m_font->MeasureString((const char*)(pSklNode->name));
							m_font->DrawString(m_spriteBatch.get(), (const char*)(pSklNode->name), text_pos, { 0.0f, 0.0f, 0.0f, 1.0f }, 0.0f, origin, { (0.4f + d / 60, 0.3f + d / 60) });
							m_font->DrawString(m_spriteBatch.get(), (const char*)(pSklNode->name), text_pos - tilt, { 1.0f, 1.0f, 1.0f, 1.0f }, 0.0f, origin, { (0.4f + d / 60, 0.3f + d / 60) });
						}
					}
				}
			}
		}
		origin = m_font->MeasureString((const char*)(mi->mdl_name)) / 2.f;
		m_font->DrawString(m_spriteBatch.get(), (const char*)(mi->mdl_name), m_model_name_pos, Colors::White, 0.f, origin);

		if (Current_mmi->skl)
		{
			sprintf(g_model_counter, "Model: %d/%d\nBones: %d \n",
				g_selected_model_index, g_loaded_mdl_count, Current_mmi->skl->amount_bones);

			if (Current_anim)
			{
				origin = m_font->MeasureString((const char*)(Current_anim->anim_name)) / 2.f;
				m_font->DrawString(m_spriteBatch.get(), (const char*)(Current_anim->anim_name), g_anim_name_pos, Colors::White, 0.f, origin);

				sprintf(g_anim_info, "Anim: %d/%d\n\nBones required: %d \nTotal time: %f \nType: %08x",
					g_selected_anim_index, g_loaded_anim_count, Current_anim->bones_count, Current_anim->total_time, Current_anim->anim->type);


				origin = m_font->MeasureString((const char*)(g_anim_info)) / 2.f;
				m_font->DrawString(m_spriteBatch.get(), (const char*)(g_anim_info), g_DataFontPos, Colors::White, 0.f, origin, { (0.5f, 0.5f) });

			}

		}
		else
		{
			sprintf(g_model_counter, "Model: %d/%d\n",
				g_selected_model_index, g_loaded_mdl_count);
		}
		origin = m_font->MeasureString((const char*)(g_model_counter)) / 2.f;
		m_font->DrawString(m_spriteBatch.get(), (const char*)(g_model_counter), g_Model_counter_font_pos, Colors::White, 0.f, origin, { (0.5f, 0.5f) });

		
	}

	m_spriteBatch->End();

	

	g_pSwapChain->Present(1, 0);
	g_framecount++;
}

void MovementFunc_()
{
	MovementFunc();
	return;
}
