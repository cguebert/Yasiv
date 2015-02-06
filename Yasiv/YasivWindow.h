#pragma once

#include "YasivImage.h"
#include "YasivApp.h"

#include <iostream>


class YasivApp;

class YasivWindow
{
public:
	YasivWindow(YasivApp* app, IWICImagingFactory* factory);
	~YasivWindow();

	LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	HRESULT OpenImage(LPWSTR pszFileName = NULL);
	HWND GetWnd() { return m_hWnd; }

	void Export(std::wostream& out);
	void Import(std::wistream& in);

private:
	YasivApp* m_pApp;
	HWND m_hWnd;

	YasivImage m_Image;
	bool m_bSnap, m_bActionInWindow, m_bDrawTransparent;
	RECT m_rcMoving;
	int m_iImagePosX, m_iImagePosY;

	enum MouseAction
	{
		NONE,
		MOVING,
		RESIZE_LEFT,
		RESIZE_TOP,
		RESIZE_RIGHT,
		RESIZE_BOTTOM,
		RESIZE_TOPLEFT,
		RESIZE_TOPRIGHT,
		RESIZE_BOTTOMLEFT,
		RESIZE_BOTTOMRIGHT,
	};

	MouseAction m_Action;
	int m_iPreviousX, m_iPreviousY;

    LRESULT OnPaint(HWND hWnd);
	LRESULT OnKeyDown(HWND hWnd, WPARAM wParam);
	void UpdateWindow(int x, int y, int w, int h);
	
	int SnapX(int x, bool* snaped=NULL);
	int SnapY(int y, bool* snaped=NULL);
	void KeepImageRatio(int& w, int& h);
	void KeepImageRatioW(int& w, int h);
	void KeepImageRatioH(int w, int& h);
};