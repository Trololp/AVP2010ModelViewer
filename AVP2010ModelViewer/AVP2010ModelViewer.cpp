// AVP2010ModelViewer 
// Trololp. 03.2021 
//
#include "stdafx.h"
#include "AVP2010ModelViewer.h"
#include "MainClass_Viewer_Screen.h"
#include "Debug.h"
#include <ShObjIdl.h>
#include <winuser.h>

extern DWORD g_loaded_anim_count;
extern DWORD g_loaded_mdl_count;
extern char* g_model_names[1000];
extern char* g_anim_names[2000];
extern DWORD g_selected_model_index;
extern DWORD g_selected_anim_index;

#define MAX_LOADSTRING 100

// Глобальные переменные:
extern wchar_t* g_path1;
extern HINSTANCE               g_hInst;
extern HWND                    g_hWnd;

WCHAR szTitle[MAX_LOADSTRING];// Текст строки заголовка
WCHAR szWindowClass[MAX_LOADSTRING];// имя класса главного окна

int screen_width = GetSystemMetrics(SM_CXSCREEN);
int screen_height = GetSystemMetrics(SM_CYSCREEN);

// Отправить объявления функций, включенных в этот модуль кода:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    SelectAnimModel(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: разместите код здесь.

    // Инициализация глобальных строк
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_AVP2010MODELVIEWER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Выполнить инициализацию приложения:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

	// Get folder with models
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED |
		COINIT_DISABLE_OLE1DDE);
	if (SUCCEEDED(hr))
	{
		IFileOpenDialog *pFileOpen;

		// Create the FileOpenDialog object.
		hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
			IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

		if (SUCCEEDED(hr))
		{
			FILEOPENDIALOGOPTIONS* pfos = new FILEOPENDIALOGOPTIONS;
			ZeroMemory(pfos, sizeof(pfos));
			pFileOpen->GetOptions(pfos);
			*pfos = *pfos |= FOS_PICKFOLDERS;
			pFileOpen->SetOptions(*pfos);
			// Show the Open dialog box.
			hr = pFileOpen->Show(NULL);

			// Get the file name from the dialog box.
			if (SUCCEEDED(hr))
			{
				IShellItem *pItem;
				hr = pFileOpen->GetResult(&pItem);
				if (SUCCEEDED(hr))
				{
					hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &g_path1);
					if (FAILED(hr))
					{
						MessageBoxW(NULL, L"Bad folder or else ...", L"File Path", MB_OK);
					}
					pItem->Release();
				}
			}

			pFileOpen->Release();
		}
		CoUninitialize();
	}

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_AVP2010MODELVIEWER));

	if (FAILED(InitDevice()))
	{
		CleanupDevice();
		return 0;
	}

	// Main message loop
	MSG msg = { 0 };
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			MovementFunc_();
			Render();
		}
	}

	CleanupDevice();

	return (int)msg.wParam;
}



//
//  ФУНКЦИЯ: MyRegisterClass()
//
//  НАЗНАЧЕНИЕ: регистрирует класс окна.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_AVP2010MODELVIEWER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_AVP2010MODELVIEWER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   ФУНКЦИЯ: InitInstance(HINSTANCE, int)
//
//   НАЗНАЧЕНИЕ: сохраняет обработку экземпляра и создает главное окно.
//
//   КОММЕНТАРИИ:
//
//        В данной функции дескриптор экземпляра сохраняется в глобальной переменной, а также
//        создается и выводится на экран главное окно программы.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   g_hInst = hInstance;

   RECT rc = { 0, 0, screen_width, screen_height };
   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
	   CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
	   nullptr, nullptr, hInstance, nullptr);
   g_hWnd = hWnd;

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  ФУНКЦИЯ: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  НАЗНАЧЕНИЕ:  обрабатывает сообщения в главном окне.
//
//  WM_COMMAND — обработать меню приложения
//  WM_PAINT — отрисовать главное окно
//  WM_DESTROY — отправить сообщение о выходе и вернуться
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Разобрать выбор в меню:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
			case IDM_SELECTANIMMODEL:
				DialogBox(g_hInst, MAKEINTRESOURCE(IDD_SELECTANIMMODEL), hWnd, SelectAnimModel);
				break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Добавьте сюда любой код прорисовки, использующий HDC...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
		WndProc_2(hWnd, message, wParam, lParam);
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Обработчик сообщений для окна "О программе".
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

// anim and model select

INT_PTR CALLBACK SelectAnimModel(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	HWND Combobox_model_h = GetDlgItem(hDlg, IDC_COMBO1);
	HWND Combobox_anim_h = GetDlgItem(hDlg, IDC_COMBO2);
	TCHAR temp[256];
	ZeroMemory(temp, 256);

	switch (message)
	{
	case WM_INITDIALOG:
		// Init comboboxes with strings !!!
		
		for (int i = 0; i < g_loaded_mdl_count; i++)
		{
			wsprintf(temp, L"%S", g_model_names[i]);

			SendMessage(Combobox_model_h, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)temp);
		}

		for (int i = 0; i < g_loaded_anim_count; i++)
		{
			wsprintf(temp, L"%S", g_anim_names[i]);

			SendMessage(Combobox_anim_h, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)temp);
		}

		SendMessage(Combobox_model_h, CB_SETCURSEL, g_selected_model_index, 0);
		SendMessage(Combobox_anim_h, CB_SETCURSEL, g_selected_anim_index, 0);

		return (INT_PTR)TRUE;
	case WM_DESTROY:
		EndDialog(hDlg, LOWORD(wParam));
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		


		if (LOWORD(wParam) == IDC_BUTTON2)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		if (LOWORD(wParam) == IDC_BUTTON1)
		{
			// Do select !!!
			if ((SendMessage(Combobox_model_h, CB_GETCURSEL, 0, 0) != CB_ERR) && (SendMessage(Combobox_anim_h, CB_GETCURSEL, 0, 0) != CB_ERR))
			{
				g_selected_model_index = SendMessage(Combobox_model_h, CB_GETCURSEL, 0, 0);
				g_selected_anim_index = SendMessage(Combobox_anim_h, CB_GETCURSEL, 0, 0);
			}

			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

