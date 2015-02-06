#include "YasivWindow.h"

YasivWindow::YasivWindow(YasivApp* app, IWICImagingFactory* factory)
	: m_pApp(app)
	, m_hWnd(nullptr)
	, m_Action(NONE)
	, m_Image(factory)
	, m_bSnap(true)
	, m_bActionInWindow(false)
	, m_bDrawTransparent(false)
	, m_iImagePosX(0)
	, m_iImagePosY(0)
{
	int w = 640, h = 480;
    // Create window
    HWND hWnd = CreateWindow(
	//	WS_EX_LAYERED,
        L"Yasiv",
        L"Yet Another Simple Image Viewer",
		WS_POPUP | WS_VISIBLE | WS_EX_NOPARENTNOTIFY | WS_EX_TOOLWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        w,
        h,
        m_pApp->GetMainWindow(),
        nullptr,
		m_pApp->GetInstance(),
        this
        );

	if(hWnd)
	{	
		m_Image.SetWnd(hWnd);
		m_hWnd = hWnd;
			
		// Center the window
		POINT ptZero = {0};
		HMONITOR hmonPrimary = MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY);
		MONITORINFO monitorinfo = {0};
		monitorinfo.cbSize = sizeof(monitorinfo);
		GetMonitorInfo(hmonPrimary, &monitorinfo);

		int x = monitorinfo.rcWork.left + (monitorinfo.rcWork.right - monitorinfo.rcWork.left - w) / 2;
		int y = monitorinfo.rcWork.top + (monitorinfo.rcWork.bottom - monitorinfo.rcWork.top - h) / 2;

		UpdateWindow(x, y, w, h);
	}
	else
	{
		LPTSTR buf = nullptr;
		DWORD size = FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ARGUMENT_ARRAY,
			nullptr,
			GetLastError(),
			LANG_NEUTRAL,
			(LPTSTR)&buf,
			0,
			nullptr);
		std::cerr << buf << std::endl;
		LocalFree((HLOCAL)buf);
	}
}

YasivWindow::~YasivWindow()
{
}

HRESULT YasivWindow::OpenImage(LPWSTR pszFileName)
{
	HRESULT hr = m_Image.OpenFile(pszFileName);
	if(SUCCEEDED(hr) && m_hWnd)
	{
		UINT w, h;
		if(SUCCEEDED(m_Image.GetSize(&w, &h)))
		{
			RECT WorkArea; 
			SystemParametersInfo(SPI_GETWORKAREA, 0, &WorkArea, 0);
			int ww = WorkArea.right - WorkArea.left;
			int wh = WorkArea.bottom - WorkArea.top;
			int nh = min(ww * h / w, h);
			int nw = min(wh * w / h, w);
			if(nh > wh)
			{
				int tw = nw, th = wh;
				KeepImageRatio(tw, th);
				m_Image.Resize(tw, th);
				UpdateWindow(WorkArea.left + (ww - tw) / 2, WorkArea.top + (wh - th) / 2, tw, th);
			}
			else
			{
				int tw = ww, th = nh;
				KeepImageRatio(tw, th);
				m_Image.Resize(tw, th);
				UpdateWindow(WorkArea.left + (ww - tw) / 2, WorkArea.top + (wh - th) / 2, tw, th);
			}
			m_iImagePosX = m_iImagePosY = 0;
		}
	}

	return hr;
}

int clamp(int value, int min, int max)
{
	if(value < min)
		return min;
	if(value > max)
		return max;
	return value;
}

LRESULT YasivWindow::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
		case WM_USER:
		{
			m_pApp->LoadCommandLine();
			return 0;
		}
        case WM_PAINT:
        {
            return OnPaint(hWnd);
        }
		case WM_KEYDOWN:
		{
			return OnKeyDown(hWnd, wParam);
		}
		case WM_LBUTTONDOWN:
		{
			if(wParam & MK_LBUTTON)
			{
				m_bActionInWindow = (wParam & MK_CONTROL) == MK_CONTROL;

				int x = m_iPreviousX = GET_X_LPARAM(lParam); 
				int y = m_iPreviousY = GET_Y_LPARAM(lParam); 
				SetCapture(hWnd);

				GetWindowRect(hWnd, &m_rcMoving);
				int width = (m_rcMoving.right - m_rcMoving.left);
				int height = (m_rcMoving.bottom - m_rcMoving.top);
				int handleW = clamp(width / 4, 15, 50);
				int handleH = clamp(height / 4, 15, 50);
				
				bool bLeft=false, bTop=false, bRight=false, bBottom=false;
				if(x < handleW) bLeft = true;
				else if(x > width - handleW) bRight = true;
				if(y < handleH) bTop = true;
				else if(y > height - handleH) bBottom = true;

				if(bTop)
				{
					if(bLeft)
						m_Action = RESIZE_TOPLEFT;
					else if(bRight)
						m_Action = RESIZE_TOPRIGHT;
					else
						m_Action = RESIZE_TOP;
				}
				else if(bBottom)
				{
					if(bLeft)
						m_Action = RESIZE_BOTTOMLEFT;
					else if(bRight)
						m_Action = RESIZE_BOTTOMRIGHT;
					else
						m_Action = RESIZE_BOTTOM;
				}
				else if(bLeft)
					m_Action = RESIZE_LEFT;
				else if(bRight)
					m_Action = RESIZE_RIGHT;
				else
					m_Action = MOVING;

				if(m_Action != MOVING && !m_bActionInWindow)
					m_iImagePosX = m_iImagePosY = 0;
			}

			break;
		}
 		case WM_LBUTTONUP:
		{
			if(GetCapture() == hWnd && m_Action != NONE)
			{
				ReleaseCapture();
				if(m_Action != MOVING)
				{
					RECT rcClient;
					if(GetClientRect(hWnd, &rcClient))
					{
						if(!m_bActionInWindow)
							m_Image.Resize(rcClient.right - rcClient.left, rcClient.bottom - rcClient.top, false);
						InvalidateRect(hWnd, &rcClient, TRUE);
					}
				}
				m_Action = NONE;
			}
			break;
		}
		case WM_MOUSEMOVE:
		{
			m_bSnap = !(wParam & MK_SHIFT);
			int xPos = GET_X_LPARAM(lParam); 
			int yPos = GET_Y_LPARAM(lParam);

			int dX = xPos - m_iPreviousX;
			int dY = yPos - m_iPreviousY;

			int w = m_rcMoving.right - m_rcMoving.left;
			int h = m_rcMoving.bottom - m_rcMoving.top;

			switch(m_Action)
			{
				case RESIZE_LEFT:
					dX = SnapX(m_rcMoving.left+dX) - m_rcMoving.left;
					m_rcMoving.left += dX;
					w -= dX;
					KeepImageRatioH(w, h);
					UpdateWindow(m_rcMoving.left, m_rcMoving.top, w, h);
					break;
				case RESIZE_TOP:
					dY = SnapY(m_rcMoving.top+dY) - m_rcMoving.top;
					m_rcMoving.top += dY;
					h -= dY;
					KeepImageRatioW(w, h);
					UpdateWindow(m_rcMoving.left, m_rcMoving.top, w, h);
					break;
				case RESIZE_RIGHT:
					dX = SnapX(m_rcMoving.right+dX) - m_rcMoving.right;
					w += dX;
					KeepImageRatioH(w, h);
					UpdateWindow(m_rcMoving.left, m_rcMoving.top, w, h);
					break;
				case RESIZE_BOTTOM:
					dY = SnapY(m_rcMoving.bottom+dY) - m_rcMoving.bottom;
					h += dY;
					KeepImageRatioW(w, h);
					UpdateWindow(m_rcMoving.left, m_rcMoving.top, w, h);
					break;
				case RESIZE_TOPLEFT:
					dX = SnapX(m_rcMoving.left+dX) - m_rcMoving.left;
					dY = SnapY(m_rcMoving.top+dY) - m_rcMoving.top;
					w -= dX; h -= dY;
					KeepImageRatio(w, h);
					m_rcMoving.left = m_rcMoving.right - w; 
					m_rcMoving.top =  m_rcMoving.bottom - h;
					UpdateWindow(m_rcMoving.left, m_rcMoving.top, w, h);
					break;
				case RESIZE_TOPRIGHT:
					dX = SnapX(m_rcMoving.right+dX) - m_rcMoving.right;
					dY = SnapY(m_rcMoving.top+dY) - m_rcMoving.top;
					w += dX; h -= dY;
					KeepImageRatio(w, h);
					m_rcMoving.top =  m_rcMoving.bottom - h;
					UpdateWindow(m_rcMoving.left, m_rcMoving.top, w, h);
					break;
				case RESIZE_BOTTOMLEFT:
					dX = SnapX(m_rcMoving.left+dX) - m_rcMoving.left;
					dY = SnapY(m_rcMoving.bottom+dY) - m_rcMoving.bottom;
					w -= dX; h += dY;
					KeepImageRatio(w, h);
					m_rcMoving.left = m_rcMoving.right - w; 
					UpdateWindow(m_rcMoving.left, m_rcMoving.top, w, h);
					break;
				case RESIZE_BOTTOMRIGHT:
					dX = SnapX(m_rcMoving.right+dX) - m_rcMoving.right;
					dY = SnapY(m_rcMoving.bottom+dY) - m_rcMoving.bottom;
					w += dX; h += dY;
					KeepImageRatio(w, h);
					UpdateWindow(m_rcMoving.left, m_rcMoving.top, w, h);
					break;
				case MOVING:
				{
					if(m_bActionInWindow)	// Moving the image inside the window
					{
						RECT rcClient;
						UINT iw, ih;
						if(GetClientRect(hWnd, &rcClient) && SUCCEEDED(m_Image.GetCurrentSize(&iw, &ih)))
						{
							int w = rcClient.right-rcClient.left, h = rcClient.bottom-rcClient.top;
							m_iImagePosX = clamp(m_iImagePosX+dX, w-iw, 0);
							m_iImagePosY = clamp(m_iImagePosY+dY, h-ih, 0);
							InvalidateRect(hWnd, nullptr, FALSE);		
							m_iPreviousX = xPos;
							m_iPreviousY = yPos;
						}
					}
					else	// Moving the window and the image with it
					{
						bool snaped;
						dX = SnapX(m_rcMoving.left+dX, &snaped) - m_rcMoving.left;
						if(!snaped) dX = SnapX(m_rcMoving.right+dX, &snaped) - m_rcMoving.right;
						dY = SnapY(m_rcMoving.top+dY, &snaped) - m_rcMoving.top;
						if(!snaped) dY = SnapY(m_rcMoving.bottom+dY, &snaped) - m_rcMoving.bottom;
						m_rcMoving.left += dX;	m_rcMoving.right += dX;
						m_rcMoving.top += dY;	m_rcMoving.bottom += dY;
						MoveWindow(hWnd, m_rcMoving.left, m_rcMoving.top, w, h, FALSE);
					}
					break;
				}
			}

			break;
		}
        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

LRESULT YasivWindow::OnKeyDown(HWND hWnd, WPARAM wParam)
{
	switch(wParam)
	{	
		case '1': // Reset to initial size
		{
			UINT w, h;
			if(SUCCEEDED(m_Image.GetSize(&w, &h)))
			{
				m_Image.Resize(w, h);
				m_iImagePosX = m_iImagePosY = 0;

				RECT rcWnd; 
				GetWindowRect(hWnd, &rcWnd);
				UpdateWindow(rcWnd.left, rcWnd.top, w, h);
			}
			break;
		}
		case 'E': // Export
		{
			m_pApp->ExportLayout();
			return 0;
		}
		case 'F': // Resize to use the whole screen
		{
			UINT w, h;
			if(SUCCEEDED(m_Image.GetSize(&w, &h)))
			{
				RECT WorkArea, rcWnd; 
				SystemParametersInfo(SPI_GETWORKAREA, 0, &WorkArea, 0);
				int ww = WorkArea.right - WorkArea.left;
				int wh = WorkArea.bottom - WorkArea.top;
				GetWindowRect(hWnd, &rcWnd);
				int nh = ww * h / w;
				int nw = wh * w / h;
				if(nh > wh)
				{
					m_Image.Resize(nw, wh);
					int x = rcWnd.left;
					if(nw > ww)
					{
						nw = ww;
						x = 0;
					}
					else if(x + nw > WorkArea.right)
						x = WorkArea.right - nw;
					UpdateWindow(x, WorkArea.top, nw, wh);
				}
				else
				{
					m_Image.Resize(ww, nh);
					int y = rcWnd.top;
					if(nh > wh)
					{
						nh = wh;
						y = 0;
					}
					else if(y + nh > WorkArea.bottom)
						y = WorkArea.bottom - nh;
					UpdateWindow(WorkArea.left, y, ww, nh);
				}
				m_iImagePosX = m_iImagePosY = 0;
			}
			return 0;
		}
		case 'H': // Resize to maximize the horizontal size
		{
			UINT w, h;
			if(SUCCEEDED(m_Image.GetSize(&w, &h)))
			{
				RECT WorkArea, rcWnd; 
				SystemParametersInfo(SPI_GETWORKAREA, 0, &WorkArea, 0);
				GetWindowRect(hWnd, &rcWnd);

				int ww = WorkArea.right - WorkArea.left;
				int wh = WorkArea.bottom - WorkArea.top;
				int nh = ww * h / w;
				int y = rcWnd.top;
				
				m_Image.Resize(ww, nh);

				if(nh > wh)
				{
					nh = wh;
					y = 0;
				}
				else if(y + nh > WorkArea.bottom)
					y = WorkArea.bottom - nh;

				UpdateWindow(WorkArea.left, y, ww, nh);
				m_iImagePosX = m_iImagePosY = 0;
			}
			return 0;
		}
		case 'I': // Import
		{
			m_pApp->ImportLayout();
			return 0;
		}
		case 'L': // Rotate left
		{
			UINT iw, ih;
			if (SUCCEEDED(m_Image.GetCurrentSize(&iw, &ih)))
			{
				if (SUCCEEDED(m_Image.Rotate(-1)))
				{
					RECT rcWnd; 
					GetWindowRect(hWnd, &rcWnd);
					LONG ww, wh;
					ww = rcWnd.right-rcWnd.left;
					wh = rcWnd.bottom-rcWnd.top;
					UpdateWindow(rcWnd.left, rcWnd.top, wh, ww);
					int tmp = m_iImagePosX;
					m_iImagePosX = m_iImagePosY;
					m_iImagePosY = ww - iw - tmp;
				}
			}
			break;
		}
		case 'N': // New blank window
		{
			m_pApp->OpenNewWindow(true);
			return 0;
		}
		case 'O': // Open another image in this window	
		{
			// In this case we try to keep the same size we had previously
			if(SUCCEEDED(m_Image.OpenFile()))
			{
				RECT rcWnd; 
				GetWindowRect(hWnd, &rcWnd);
				int w = rcWnd.right-rcWnd.left, h = rcWnd.bottom-rcWnd.top;
				KeepImageRatio(w, h);
				m_Image.Resize(w, h);
				UpdateWindow(rcWnd.left, rcWnd.top, w, h);
				m_iImagePosX = m_iImagePosY = 0;
			}
			else
				MessageBox(nullptr, L"Failed to load image, select another one.", L"Application Error", MB_ICONEXCLAMATION | MB_OK);

			return 0;
		}
		case 'Q': // Quit
		{
			PostQuitMessage(0);
			return 0;
		}
		case 'R': // Rotate right
		{
			UINT iw, ih;
			if (SUCCEEDED(m_Image.GetCurrentSize(&iw, &ih)))
			{
				if (SUCCEEDED(m_Image.Rotate(1)))
				{
					RECT rcWnd; 
					GetWindowRect(hWnd, &rcWnd);
					LONG ww, wh;
					ww = rcWnd.right-rcWnd.left;
					wh = rcWnd.bottom-rcWnd.top;
					UpdateWindow(rcWnd.left, rcWnd.top, wh, ww);
					int tmp = m_iImagePosX;
					m_iImagePosX = wh - ih - m_iImagePosY;
					m_iImagePosY = tmp;
				}
			}
			break;
		}
		case 'S' : // Show all windows
		{
			m_pApp->AllWindowsToTop();
			return 0;
		}
		case 'T':
		{
			m_bDrawTransparent = !m_bDrawTransparent;

			if (m_bDrawTransparent)
			{
				m_Image.PremultiplyAlpha();
				SetWindowLong(hWnd, GWL_EXSTYLE, WS_EX_LAYERED);
			}
			else
				SetWindowLong(hWnd, GWL_EXSTYLE, 0);
			
			// Update the window in the same position
			RECT rcWnd;
			GetWindowRect(hWnd, &rcWnd);
			InvalidateRect(hWnd, nullptr, TRUE);
			int w = rcWnd.right - rcWnd.left, h = rcWnd.bottom - rcWnd.top;
			UpdateWindow(rcWnd.left, rcWnd.top, w, h);
			
			break;
		}
		case 'V': // Resize to maximize the vertical size
		{
			UINT w, h;
			if(SUCCEEDED(m_Image.GetSize(&w, &h)))
			{
				RECT WorkArea, rcWnd; 
				SystemParametersInfo(SPI_GETWORKAREA, 0, &WorkArea, 0);
				GetWindowRect(hWnd, &rcWnd);

				int ww = WorkArea.right - WorkArea.left;
				int wh = WorkArea.bottom - WorkArea.top;
				int nw = wh * w / h;
				int x = rcWnd.left;
				
				m_Image.Resize(nw, wh);

				if(nw > ww)
				{
					nw = ww;
					x = 0;
				}
				else if(x + nw > WorkArea.right)
					x = WorkArea.right - nw;

				UpdateWindow(x, WorkArea.top, nw, wh);
				m_iImagePosX = m_iImagePosY = 0;
			}
			return 0;
		}
		case VK_ESCAPE: // Close only this window
		{
			m_pApp->CloseWindow(this);
			return 0;
		}
	}

	return 1;
}

LRESULT YasivWindow::OnPaint(HWND hWnd)
{
    LRESULT lRet = 1;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);

    if (hdc)
    {
		RECT rcClient;
		HRESULT hr = S_OK;
		hr = GetClientRect(hWnd, &rcClient) ? S_OK: E_FAIL;
		int cw = rcClient.right-rcClient.left, ch = rcClient.bottom-rcClient.top;
		if (!m_bDrawTransparent && SUCCEEDED(hr))
		{
			HBRUSH hbBk, hbrOld;
			hbBk = CreateSolidBrush(0xFF000000);
			hbrOld = (HBRUSH)SelectObject(hdc, hbBk);

			Rectangle(hdc, 0, 0, cw, ch);

			SelectObject(hdc, hbrOld);
			DeleteObject(hbBk);
		}

        // Create a memory device context
        HDC hdcMem = CreateCompatibleDC(nullptr);
        if (hdcMem)
        {
            // Select DIB section into the memory DC
			HBITMAP m_hDIBBitmap = m_Image.GetBitmap();
            HBITMAP hbmOld = SelectBitmap(hdcMem, m_hDIBBitmap);
            if (hbmOld)
            {
                BITMAP bm;
                // Fill in a BITMAP with the DIB section object in memory DC
                if (GetObject(m_hDIBBitmap, sizeof(bm), &bm) == sizeof(bm))
                {
					if(!m_bActionInWindow && m_Action > MOVING)
					{
						UINT iw, ih;
						if(SUCCEEDED(m_Image.GetCurrentSize(&iw, &ih)))
							StretchBlt(hdc, m_iImagePosX, m_iImagePosY, cw, ch, hdcMem, 0, 0, iw, ih, SRCCOPY);
					}
					else
					{
						// Perform a bit-block transfer of the BITMAP to screen DC
						BitBlt(hdc, 0, 0, bm.bmWidth, bm.bmHeight, hdcMem, -m_iImagePosX, -m_iImagePosY, SRCCOPY);
					}

                    // Restore the memory DC
                    SelectBitmap(hdcMem, hbmOld);
                    lRet = 0;
                }
            }
            DeleteDC(hdcMem);
        }
        EndPaint(hWnd, &ps);
    }

    return lRet;
}

void YasivWindow::UpdateWindow(int x, int y, int w, int h)
{
	if (!m_bDrawTransparent)
	{
		MoveWindow(m_hWnd, x, y, w, h, TRUE);
		return;
	}

	HDC hdcScreen = GetDC(NULL);
	HDC hdcMem = CreateCompatibleDC(hdcScreen);
	HBITMAP hbmOld = SelectBitmap(hdcMem, m_Image.GetBitmap());

	POINT ptScreen { x, y };
	SIZE size { w, h };
	POINT ptSrc { m_iImagePosX, m_iImagePosY };

	BLENDFUNCTION blend = { 0 };
	blend.BlendOp = AC_SRC_OVER;
	blend.BlendFlags = 0;
	blend.SourceConstantAlpha = 255;
	blend.AlphaFormat = AC_SRC_ALPHA;

	// Paint the window (in the right location) with the alpha-blended bitmap
	UpdateLayeredWindow(m_hWnd, hdcScreen, &ptScreen, &size, hdcMem, &ptSrc, RGB(0, 0, 0), &blend, ULW_ALPHA);

	SelectBitmap(hdcMem, hbmOld);
	DeleteDC(hdcMem);
	ReleaseDC(NULL, hdcScreen);
}

int YasivWindow::SnapX(int x, bool* snaped)
{
	if(!m_bSnap) 
	{
		if(snaped) 
			*snaped = false;
		return x;
	}
	else
		return m_pApp->SnapX(this, x, snaped);
}

int YasivWindow::SnapY(int y, bool* snaped)
{
	if(!m_bSnap) 
	{
		if(snaped) 
			*snaped = false;
		return y;
	}
	else
		return m_pApp->SnapY(this, y, snaped);
}

void YasivWindow::KeepImageRatio(int& w, int& h)
{
	if(m_bActionInWindow)
		return;

	UINT iw, ih;
	if(SUCCEEDED(m_Image.GetSize(&iw, &ih)))
	{
		int nh = w * ih / iw;
		int nw = h * iw / ih;
		if(nh > h)
			w = nw;
		else
			h = nh;
	}
}

void YasivWindow::KeepImageRatioW(int& w, int h)
{
	if(m_bActionInWindow)
		return;

	UINT iw, ih;
	if(SUCCEEDED(m_Image.GetSize(&iw, &ih)))
		w = h * iw / ih;
}

void YasivWindow::KeepImageRatioH(int w, int& h)
{
	if(m_bActionInWindow)
		return;

	UINT iw, ih;
	if(SUCCEEDED(m_Image.GetSize(&iw, &ih)))
		h = w * ih / iw;
}

void YasivWindow::Export(std::wostream& out)
{
	out << m_Image.GetFilePath() << std::endl;
	RECT rcClient;
	UINT iw, ih, ir;
	GetWindowRect(m_hWnd, &rcClient);
	m_Image.GetCurrentSize(&iw, &ih);
	ir = m_Image.GetRotation();
	out << rcClient.left << " " 
		<< rcClient.top << " " 
		<< rcClient.right - rcClient.left << " "
		<< rcClient.bottom - rcClient.top << std::endl
		<< m_iImagePosX << " " 
		<< m_iImagePosY << " "
		<< iw << " " 
		<< ih << std::endl
		<< ir << std::endl;
}

void ignoreEOL(std::wistream& in)
{
	wchar_t c = in.peek();
	while (c == '\n' || c == '\r')
	{
		in.get();
		c = in.peek();
	}
}

void YasivWindow::Import(std::wistream& in)
{
	std::wstring fileName;
	ignoreEOL(in);

	std::getline(in, fileName);
	int wl, wt, ww, wh, ix, iy, iw, ih, ir=0;
	in >> wl >> wt >> ww >> wh >> ix >> iy >> iw >> ih;

	// Read rotation, if present
	ignoreEOL(in);
	wchar_t c = in.peek();
	if (c >= L'0' && c <= L'3')
		in >> ir;

	m_Image.OpenFile(fileName.c_str());
	m_Image.SetRotation(ir);
	m_Image.Resize(iw, ih);
	m_iImagePosX = ix;
	m_iImagePosY = iy;

	UpdateWindow(wl, wt, ww, wh);
}