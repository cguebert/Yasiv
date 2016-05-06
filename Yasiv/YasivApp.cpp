// Yasiv.cpp : définit le point d'entrée pour l'application.
//

#ifndef WINVER                  // Allow use of features specific to Windows XP or later.
#define WINVER 0x0501           // Change this to the appropriate value to target other versions of Windows.
#endif

#ifndef _WIN32_WINNT            // Allow use of features specific to Windows XP or later.
#define _WIN32_WINNT 0x0501     // Change this to the appropriate value to target other versions of Windows.
#endif

#ifndef _WIN32_WINDOWS          // Allow use of features specific to Windows 98 or later.
#define _WIN32_WINDOWS 0x0410   // Change this to the appropriate value to target Windows Me or later.
#endif

#ifndef _WIN32_IE               // Allow use of features specific to IE 6.0 or later.
#define _WIN32_IE 0x0600        // Change this to the appropriate value to target other versions of IE.
#endif

#define WIN32_LEAN_AND_MEAN     // Exclude rarely-used stuff from Windows headers

#include "YasivApp.h"
#include <stdio.h>
#include <shellapi.h>
#include <Shlwapi.h>

#include <iostream>
#include <fstream>

#include <string>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR pszCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pszCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    if(SUCCEEDED(hr))
    {
        YasivApp app;
        hr = app.Initialize(hInstance);  
        if(SUCCEEDED(hr))
        {
            BOOL fRet;
            MSG msg;

            // Main message loop:
            while ((fRet = GetMessage(&msg, NULL, 0, 0)) != 0)
            {
                if (fRet == -1)
                {
                    break;
                }
                else
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        }

        CoUninitialize();
    }

    return SUCCEEDED(hr);
}

DWORD WINAPI SleepProc(LPVOID lpParam)
{
	// Get single instance pointer
	YasivApp* pApp = (YasivApp*)lpParam;

	// Build event handle array
	HANDLE events[] = {pApp->m_hSingletonEvent, pApp->m_hSingletonSignal};

	BOOL bForever = TRUE;
	while(bForever)
	{
		// Goto sleep until one of the events signals, zero CPU overhead
		DWORD lResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);

		if(lResult == WAIT_OBJECT_0 + 0)		// 0 = event, another instance started
		{
			// Read the data
			std::wstring commandLine;
			// Lock file
			if(WAIT_OBJECT_0 == WaitForSingleObject(pApp->m_hSingletonMutex, INFINITE))
			{
				commandLine = pApp->m_pSingletonData;	// Copy data
				ReleaseMutex(pApp->m_hSingletonMutex);	// Unlock file
			}

			// Wake up the application with the data
			pApp->SendCommandLine(commandLine);
		//	pApp->ParseCommandLine(commandLine.c_str());
		}
		else if(lResult == WAIT_OBJECT_0 + 1)	// 1 = signal, time to exit the thread
			bForever = FALSE;	// Break the forever loop
	}

	// Set event to say thread is exiting
	SetEvent(pApp->m_hSingletonEvent);

	return 0;
}

YasivApp::YasivApp()
	: m_hSingletonEvent(NULL)
	, m_hSingletonSignal(NULL)
	, m_hSingletonMap(NULL)
	, m_hSingletonMutex(NULL)
	, m_pSingletonData(NULL)
	, m_hInst(NULL)
	, m_hWnd(NULL)
	, m_pIWICFactory(NULL)
	, m_hCommandLineMutex(NULL)
{
}

YasivApp::~YasivApp()
{
	for(auto pWindow : m_Windows)
		delete pWindow;

	SafeRelease(m_pIWICFactory);

	if(m_hSingletonMap)
	{
		// Unmap data from file
		UnmapViewOfFile(m_pSingletonData);

		// Close file
		CloseHandle(m_hSingletonMap);
	}

	// Clean up mutex
	if(m_hSingletonMutex)
		CloseHandle(m_hSingletonMutex);

	if(m_hSingletonEvent && m_hSingletonSignal)
	{
		// Set signal event to allow thread to exit
		if(PulseEvent(m_hSingletonSignal))
			WaitForSingleObject(m_hSingletonEvent, INFINITE);

		CloseHandle(m_hSingletonEvent);
		CloseHandle(m_hSingletonSignal);
	}

	if(m_hCommandLineMutex)
		CloseHandle(m_hCommandLineMutex);
}

HRESULT YasivApp::Initialize(HINSTANCE hInstance)
{
	m_hCommandLineMutex = CreateMutex(NULL, FALSE, NULL);
    HRESULT hr = S_OK;

	// Ensure that only one instance of this application can be run concurrently
	//  (from the MFC singleton example by Justin Hallet)
	std::wstring szGUID = L"a6a436a3-8f07-44c3-8396-7648b1ca4284";

	// Create mutex, global scope
	std::wstring szMutexName = szGUID + L"-Data-Mapping-Mutex";
	m_hSingletonMutex = CreateMutex(NULL, FALSE, szMutexName.c_str());	// Mutex for access to the shared data

	// Create file mapping
	std::wstring szFileName = szGUID + L"-Data-Mapping-File";
	m_hSingletonMap = CreateFileMapping(INVALID_HANDLE_VALUE,
										NULL,
										PAGE_READWRITE,
										0,
										sizeof(WCHAR)*MAX_SINGLETON_DATA,
										szFileName.c_str());

	if(GetLastError() == ERROR_ALREADY_EXISTS) 
	{
		CloseHandle(m_hSingletonMap);	// Close handle
		m_hSingletonMap = OpenFileMapping(FILE_MAP_WRITE, FALSE, szFileName.c_str());	// Open existing file mapping
	}

	// Set up data mapping
	m_pSingletonData = (LPWSTR)MapViewOfFile(m_hSingletonMap, FILE_MAP_WRITE, 0, 0, sizeof(WCHAR)*MAX_SINGLETON_DATA);

	// Lock file
	if(WAIT_OBJECT_0 == WaitForSingleObject(m_hSingletonMutex, INFINITE))
	{
		RtlZeroMemory(m_pSingletonData, sizeof(WCHAR)*MAX_SINGLETON_DATA);	// Clear data
		ReleaseMutex(m_hSingletonMutex);	// Unlock file
	}
	std::wstring szEventName = szGUID + L"-Event";
	m_hSingletonEvent = CreateEvent(NULL, FALSE, FALSE, szEventName.c_str());	// Named event for the loading of modified data
	if(m_hSingletonEvent)
	{
		if(ERROR_ALREADY_EXISTS == GetLastError())
		{	// If another instance already exists, send it the arguments
			// Lock file
			if(WAIT_OBJECT_0 == WaitForSingleObject(m_hSingletonMutex, INFINITE))
			{
				LPCWSTR pszCommandLine = GetCommandLine();
				// Check data length, prevent buffer over run
				if(wcslen(pszCommandLine) < MAX_SINGLETON_DATA)
					wcscpy_s(m_pSingletonData, MAX_SINGLETON_DATA, pszCommandLine); // Copy data
				ReleaseMutex(m_hSingletonMutex);	// Unlock file
			}
			PulseEvent(m_hSingletonEvent);	// Send a signal to the other application
			CloseHandle(m_hSingletonEvent);
			return E_FAIL;
		}
		else
		{
			m_hSingletonSignal = CreateEvent(NULL, FALSE, FALSE, NULL);	// Signal for the thread destruction
			if(m_hSingletonSignal)	
				CreateThread(NULL, 0, SleepProc, this, 0, NULL);	// Create thread
		}
	}

    // Create WIC factory
    hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_pIWICFactory));

    // Register window class
    WNDCLASSEX wcex;
    if (SUCCEEDED(hr))
    {
        wcex.cbSize        = sizeof(WNDCLASSEX);
        wcex.style         = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc   = YasivApp::s_WndProc;
        wcex.cbClsExtra    = 0;
        wcex.cbWndExtra    = sizeof(LONG_PTR);
        wcex.hInstance     = hInstance;
        wcex.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_YASIV));;
        wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wcex.hbrBackground = NULL;
        wcex.lpszMenuName  = NULL;
        wcex.lpszClassName = L"Yasiv";
        wcex.hIconSm	   = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

        m_hInst = hInstance;

        hr = RegisterClassEx(&wcex) ? S_OK : E_FAIL;

		// Do we want to open a file now ?
		ParseCommandLine(GetCommandLineW());
    }

    return hr;
}

void YasivApp::ParseCommandLine(LPCWSTR pszCommandLine)
{
	LPWSTR *szArglist;
	int nArgs;

	szArglist = CommandLineToArgvW(pszCommandLine, &nArgs);
	if(nArgs > 1)
	{
		for(int i=1; i<nArgs; i++)
		{
			if(!wcscmp(PathFindExtension(szArglist[i]), L".yasiv"))
			{
				ImportLayout(szArglist[i]);
			}
			else
			{
				YasivWindow* pWindow = new YasivWindow(this, m_pIWICFactory);
				m_Windows.push_back(pWindow);
				pWindow->OpenImage(szArglist[i]);
				SetForegroundWindow(pWindow->GetWnd());
			}
		}
	}
		
	if(m_Windows.empty())
	{
		YasivWindow* pWindow = new YasivWindow(this, m_pIWICFactory);
		m_Windows.push_back(pWindow);
		pWindow->OpenImage();
		SetForegroundWindow(pWindow->GetWnd());
	}
	LocalFree(szArglist);
}

void YasivApp::SendCommandLine(std::wstring szCommandLine)
{
	if(m_Windows.empty())
		return;

	if(WAIT_OBJECT_0 == WaitForSingleObject(m_hCommandLineMutex, INFINITE))
	{
		m_szCommandLine = szCommandLine;
		PostMessage(m_Windows.front()->GetWnd(), WM_USER, 0, 1);
		ReleaseMutex(m_hCommandLineMutex);
	}
}

void YasivApp::LoadCommandLine()
{
	if(WAIT_OBJECT_0 == WaitForSingleObject(m_hCommandLineMutex, INFINITE))
	{
		ParseCommandLine(m_szCommandLine.c_str());
		m_szCommandLine.clear();
		ReleaseMutex(m_hCommandLineMutex);
	}
}

LRESULT CALLBACK YasivApp::s_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    YasivWindow *pWindow;
    LRESULT lRet = 0;

    if(uMsg == WM_NCCREATE)
    {
        LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParam;
        pWindow = reinterpret_cast<YasivWindow*> (pcs->lpCreateParams);

        SetWindowLongPtr(hWnd, GWLP_USERDATA, PtrToUlong(pWindow));
        lRet = DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    else
    {
        pWindow = reinterpret_cast<YasivWindow*> (GetWindowLongPtr(hWnd, GWLP_USERDATA));
        if(pWindow)
            lRet = pWindow->WndProc(hWnd, uMsg, wParam, lParam);
        else
            lRet = DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return lRet;
}

void YasivApp::OpenNewWindow(bool OpenImage)
{
	YasivWindow* pWindow = new YasivWindow(this, m_pIWICFactory);
	m_Windows.push_back(pWindow);
	if(OpenImage)
		pWindow->OpenImage();
}

void YasivApp::CloseWindow(YasivWindow* pWindow)
{
	DestroyWindow(pWindow->GetWnd());
	m_Windows.erase(std::find(m_Windows.begin(), m_Windows.end(), pWindow));
	delete pWindow;
	if(m_Windows.empty())
		PostQuitMessage(0); 
}

void YasivApp::AllWindowsToTop()
{
	for(YasivWindow *pWindow : m_Windows)
		SetForegroundWindow(pWindow->GetWnd());
}

void YasivApp::AllWindowsSetTransparent(bool transparent)
{
	for (YasivWindow *pWindow : m_Windows)
		pWindow->SetDrawTransparent(transparent);
}

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
	std::vector<RECT>* pRects = reinterpret_cast<std::vector<RECT>*>(dwData);
	MONITORINFO mi;
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(hMonitor, &mi);
	pRects->push_back(mi.rcWork);
	pRects->push_back(mi.rcMonitor);

	return TRUE;
}

void YasivApp::PrepareSnapData(YasivWindow* pCurrentWindow)
{
	m_SnapRects.clear();

	// Get monitors info
	EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&m_SnapRects));

	// Get other windows geometry
	for (auto pWindow : m_Windows)
	{
		if (pWindow == pCurrentWindow)
			continue;

		RECT rect;
		if (GetWindowRect(pWindow->GetWnd(), &rect))
			m_SnapRects.push_back(rect);
	}
}

int YasivApp::SnapX(int x, bool* snaped)
{
	if(snaped) *snaped = true;

	for (auto rect : m_SnapRects)
	{
		if (abs(x - rect.left) < snapDistance)
			return rect.left;
		if (abs(x - rect.right) < snapDistance)
			return rect.right;
	}

	if(snaped) *snaped = false;
	return x;
}

int YasivApp::SnapY(int y, bool* snaped)
{
	if(snaped) *snaped = true;

	for (auto rect : m_SnapRects)
	{
		if (abs(y - rect.top) < snapDistance)
			return rect.top;
		if (abs(y - rect.bottom) < snapDistance)
			return rect.bottom;
	}

	if(snaped) *snaped = false;
	return y;
}

void YasivApp::ImportLayout(LPCWSTR pszFileName)
{
	WCHAR szFileName[MAX_PATH];
	if(!pszFileName)
	{
		pszFileName = szFileName;
		szFileName[0] = L'\0';

		OPENFILENAME ofn;
		RtlZeroMemory(&ofn, sizeof(ofn));

		ofn.lStructSize     = sizeof(ofn);
		ofn.lpstrFilter     = L"Yasiv Layout\0"					L"*.yasiv\0"
							  L"All Files\0"                    L"*.*\0"
							  L"\0";
		ofn.lpstrFile       = szFileName;
		ofn.nMaxFile        = MAX_PATH;
		ofn.lpstrTitle      = L"Import Layout";
		ofn.Flags			= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

		if(!GetOpenFileName(&ofn))
			return;
	}

	// Close previously opened windows
	for (auto pWindow : m_Windows)
	{
		DestroyWindow(pWindow->GetWnd());
		delete pWindow;
	}
	m_Windows.clear();

	std::wifstream file(pszFileName);

	unsigned int nb = m_Windows.size();
	file >> nb;
	for(unsigned int i=0; i<nb; i++)
	{
		YasivWindow* pWindow = new YasivWindow(this, m_pIWICFactory);
		pWindow->Import(file);
		m_Windows.push_back(pWindow);
	}
}

void YasivApp::ExportLayout(LPCWSTR pszFileName)
{
	std::wofstream file;

	if(pszFileName)
	{
		file.open(pszFileName);
	}
	else
	{
		WCHAR szFileName[MAX_PATH];
		szFileName[0] = L'\0';

		OPENFILENAME ofn;
		RtlZeroMemory(&ofn, sizeof(ofn));

		ofn.lStructSize     = sizeof(ofn);
		ofn.lpstrFilter     = L"Yasiv Layout\0"					L"*.yasiv\0"
							  L"All Files\0"                    L"*.*\0"
							  L"\0";
		ofn.lpstrFile       = szFileName;
		ofn.nMaxFile        = MAX_PATH;
		ofn.lpstrTitle      = L"Export Layout";
		ofn.Flags			= OFN_OVERWRITEPROMPT;

		if(!GetSaveFileName(&ofn))
			return;

		std::wstring fileName = szFileName;
		if(fileName.rfind(L".yasiv") == std::string::npos)
			fileName.append(L".yasiv");

		file.open(fileName.c_str());
	}

	int nb = m_Windows.size();
	file << nb << std::endl;
	for(auto pWindow : m_Windows)
		pWindow->Export(file);
}