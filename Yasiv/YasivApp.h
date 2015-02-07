#pragma once

#include <list>

#include "resource.h"
#include "YasivImage.h"
#include "YasivWindow.h"

class YasivWindow;
class SingleInstanceData;

class YasivApp
{
public:
    YasivApp();
    ~YasivApp();

    HRESULT Initialize(HINSTANCE hInstance);
	HINSTANCE const GetInstance() { return m_hInst; }
	HWND GetMainWindow() { return m_hWnd; }
	IWICImagingFactory* GetImagingFactory() { return m_pIWICFactory; }

	void ParseCommandLine(LPCWSTR pszCommandLine);
	void SendCommandLine(std::wstring szCommandLine);
	void LoadCommandLine();
	void OpenNewWindow(bool OpenImage=false);
	void CloseWindow(YasivWindow* pWindow);
	void AllWindowsToTop();
	void AllWindowsSetTransparent(bool transparent);

	int SnapX(YasivWindow* pWindow, int x, bool* snaped=NULL);
	int SnapY(YasivWindow* pWindow, int y, bool* snaped=NULL);

	void ImportLayout(LPCWSTR pszFileName = NULL);
	void ExportLayout(LPCWSTR pszFileName = NULL);

	HANDLE m_hSingletonEvent, m_hSingletonSignal, m_hSingletonMap, m_hSingletonMutex;
	LPWSTR m_pSingletonData;
private:
	static const int snapDistance = 15;
	std::list<YasivWindow*> m_Windows;
	typedef std::list<YasivWindow*>::iterator WindowsIterator;

  	HINSTANCE m_hInst;
	HWND m_hWnd;
	IWICImagingFactory *m_pIWICFactory; 
	HANDLE m_hCommandLineMutex;
	std::wstring m_szCommandLine;

	enum { MAX_SINGLETON_DATA = 4096 };

    static LRESULT CALLBACK s_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};