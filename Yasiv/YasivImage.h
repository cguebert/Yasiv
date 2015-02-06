#pragma once

#include <windows.h>
#include <wincodec.h>
#include <windowsx.h>
#include <commdlg.h>

#include <string>

template <typename T>
inline void SafeRelease(T *&p)
{
    if (nullptr != p)
    {
        p->Release();
        p = nullptr;
    }
}

template <typename T>
inline void SafeDelete(T *&p)
{
    if (nullptr != p)
    {
        delete p;
        p = nullptr;
    }
}

#ifndef ARGB
typedef DWORD   ARGB;
#endif

class YasivImage
{
public:
    YasivImage(IWICImagingFactory* IWICFactory);
    ~YasivImage();

	void SetWnd(HWND hWnd) { m_hWnd = hWnd; }
	HRESULT OpenFile(LPCWSTR pszFileName = nullptr);
	HBITMAP GetBitmap() { return m_hDIBBitmap; }
	HRESULT Resize(int width, int height, bool fast = false);
	HRESULT Rotate(int deltaRotation);
	HRESULT GetSize(UINT* width, UINT* height);
	HRESULT GetCurrentSize(UINT* width, UINT* height);
	UINT GetRotation() { return m_iRotation; }
	void SetRotation(UINT rot) { m_iRotation = rot; }
	std::wstring GetFilePath() { return m_sFilePath; }
	void TestBlur();

private:
    BOOL LocateImageFile(HWND hWnd, LPWSTR pszFileName, DWORD cbFileName);
	HRESULT ConvertBitmapSource(int width, int height, int rotation, bool fast=false);
    HRESULT CreateDIBSectionFromBitmapSource();

	HWND m_hWnd;
    HBITMAP m_hDIBBitmap;
    IWICImagingFactory *m_pIWICFactory;  
    IWICBitmapSource *m_pOriginalBitmapSource, *m_pTempBitmapSource;
	UINT m_iCurrentWidth, m_iCurrentHeight, m_iRotation;
	std::wstring m_sFilePath;
};