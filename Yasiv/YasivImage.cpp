#include "YasivImage.h"

YasivImage::YasivImage(IWICImagingFactory* IWICFactory)
: m_hDIBBitmap(nullptr)
, m_pIWICFactory(IWICFactory)
, m_pOriginalBitmapSource(nullptr)
, m_pTempBitmapSource(nullptr)
, m_iCurrentWidth(0)
, m_iCurrentHeight(0)
, m_iRotation(0)
{
}

YasivImage::~YasivImage()
{
    SafeRelease(m_pOriginalBitmapSource);
    DeleteObject(m_hDIBBitmap);
}

HRESULT YasivImage::OpenFile(LPCWSTR pszFileName)
{
    HRESULT hr = S_OK;
	m_sFilePath.clear();
    
	WCHAR szFileName[MAX_PATH];
	if(!pszFileName)
	{
		pszFileName = szFileName;
		// Create the open dialog box and locate the image file
		if(!LocateImageFile(m_hWnd, szFileName, ARRAYSIZE(szFileName)))
			return E_FAIL;
	}
	m_sFilePath = pszFileName;

    // Decode the source image to IWICBitmapSource
	IWICBitmapDecoder *pDecoder = nullptr;
    hr = m_pIWICFactory->CreateDecoderFromFilename(
        pszFileName,                      // Image to be decoded
        nullptr,                            // Do not prefer a particular vendor
        GENERIC_READ,                    // Desired read access to the file
        WICDecodeMetadataCacheOnDemand,  // Cache metadata when needed
        &pDecoder                        // Pointer to the decoder
        );

    IWICBitmapFrameDecode *pFrame = nullptr;

    // Retrieve the first frame of the image from the decoder
    if (SUCCEEDED(hr))
        hr = pDecoder->GetFrame(0, &pFrame);

    // Retrieve IWICBitmapSource from the frame
    if (SUCCEEDED(hr))
    {
        SafeRelease(m_pOriginalBitmapSource);
        hr = pFrame->QueryInterface(IID_IWICBitmapSource, reinterpret_cast<void **>(&m_pOriginalBitmapSource));
    }

	// Get the client Rect
	RECT rcClient;
	hr = GetClientRect(m_hWnd, &rcClient) ? S_OK: E_FAIL;

	m_iRotation = 0;

    // Scale the image to the client rect size
    if (SUCCEEDED(hr))
        hr = Resize(rcClient.right - rcClient.left, rcClient.bottom - rcClient.top);

    SafeRelease(pDecoder);
    SafeRelease(pFrame);

    return hr;
}

BOOL YasivImage::LocateImageFile(HWND hWnd, LPWSTR pszFileName, DWORD cchFileName)
{
    pszFileName[0] = L'\0';

    OPENFILENAME ofn;
    RtlZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize     = sizeof(ofn);
    ofn.hwndOwner       = hWnd;
    ofn.lpstrFilter     = L"All Image Files\0"              L"*.bmp;*.dib;*.wdp;*.mdp;*.hdp;*.gif;*.png;*.jpg;*.jpeg;*.tif;*.ico\0"
                          L"Windows Bitmap\0"               L"*.bmp;*.dib\0"
                          L"High Definition Photo\0"        L"*.wdp;*.mdp;*.hdp\0"
                          L"Graphics Interchange Format\0"  L"*.gif\0"
                          L"Portable Network Graphics\0"    L"*.png\0"
                          L"JPEG File Interchange Format\0" L"*.jpg;*.jpeg\0"
                          L"Tiff File\0"                    L"*.tif\0"
                          L"Icon\0"                         L"*.ico\0"
                          L"All Files\0"                    L"*.*\0"
                          L"\0";
    ofn.lpstrFile       = pszFileName;
    ofn.nMaxFile        = cchFileName;
    ofn.lpstrTitle      = L"Open Image";
    ofn.Flags           = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    // Display the Open dialog box. 
    return GetOpenFileName(&ofn);
}

HRESULT YasivImage::Resize(int width, int height, bool fast)
{
	m_iCurrentWidth = width;
	m_iCurrentHeight = height;
	HRESULT hr = ConvertBitmapSource(width, height, m_iRotation, fast);

    if (SUCCEEDED(hr))
        hr = CreateDIBSectionFromBitmapSource();
	
	return hr;
}

HRESULT YasivImage::Rotate(int deltaRotation)
{
	int newRotation = (m_iRotation + deltaRotation) % 4;
	HRESULT hr = ConvertBitmapSource(m_iCurrentHeight, m_iCurrentWidth, newRotation, false);
	
	if(SUCCEEDED(hr))
        hr = CreateDIBSectionFromBitmapSource();

	if (SUCCEEDED(hr))
	{
		UINT temp = m_iCurrentWidth;
		m_iCurrentWidth = m_iCurrentHeight;
		m_iCurrentHeight = temp;

		m_iRotation = newRotation;
	}

	return hr;
}

HRESULT YasivImage::ConvertBitmapSource(int width, int height, int rotation, bool fast)
{
    SafeRelease(m_pTempBitmapSource);

    HRESULT hr = S_OK;

	// Create a BitmapScaler
	IWICBitmapScaler *pScaler = nullptr;
	hr = m_pIWICFactory->CreateBitmapScaler(&pScaler);
	
	if (rotation % 2) // If rotation 90 or 270
	{
		int tmp = width;
		width = height;
		height = tmp;
	}
	
	// Initialize the bitmap scaler from the original bitmap map bits
	if (SUCCEEDED(hr))
		hr = pScaler->Initialize(m_pOriginalBitmapSource, width, height, fast ? WICBitmapInterpolationModeNearestNeighbor : WICBitmapInterpolationModeFant);

	// Format convert the bitmap into 32bppBGR, a convenient 
	// pixel format for GDI rendering 
	if (SUCCEEDED(hr))
	{
		IWICFormatConverter *pConverter = nullptr;
		hr = m_pIWICFactory->CreateFormatConverter(&pConverter);

		// Format convert to 32bppBGR
		if (SUCCEEDED(hr))
		{
			hr = pConverter->Initialize(
				pScaler,                         // Input bitmap to convert
				GUID_WICPixelFormat32bppBGR,     // Destination pixel format
				WICBitmapDitherTypeNone,         // Specified dither patterm
				nullptr,                            // Specify a particular palette 
				0.f,                             // Alpha threshold
				WICBitmapPaletteTypeCustom       // Palette translation type
				);

			// Rotate the image
			if (SUCCEEDED(hr))
			{
				// Create the flip/rotator.
				IWICBitmapFlipRotator *pFlipRotator = nullptr;
				hr = m_pIWICFactory->CreateBitmapFlipRotator(&pFlipRotator);

				if (SUCCEEDED(hr))
					hr = pFlipRotator->Initialize(pConverter, static_cast<WICBitmapTransformOptions>(rotation));

				// Store the converted bitmap as ppToRenderBitmapSource 
				if (SUCCEEDED(hr))
					hr = pFlipRotator->QueryInterface(IID_PPV_ARGS(&m_pTempBitmapSource));

				SafeRelease(pFlipRotator);
			}
		}

		SafeRelease(pConverter);
	}

	SafeRelease(pScaler);

    return hr;
}

HRESULT YasivImage::CreateDIBSectionFromBitmapSource()
{
    HRESULT hr = S_OK;

    // Get image attributes and check for valid image
    UINT width = 0;
    UINT height = 0;

    void *pvImageBits = nullptr;
    
    // Check BitmapSource format
    WICPixelFormatGUID pixelFormat;
    hr = m_pTempBitmapSource->GetPixelFormat(&pixelFormat);

    if (SUCCEEDED(hr))
        hr = (pixelFormat == GUID_WICPixelFormat32bppBGR) ? S_OK : E_FAIL;

    if (SUCCEEDED(hr))
        hr = m_pTempBitmapSource->GetSize(&width, &height);

    // Create a DIB section based on Bitmap Info
    // BITMAPINFO Struct must first be setup before a DIB can be created.
    // Note that the height is negative for top-down bitmaps
    if (SUCCEEDED(hr))
    {
        BITMAPINFO bminfo;
        ZeroMemory(&bminfo, sizeof(bminfo));
        bminfo.bmiHeader.biSize         = sizeof(BITMAPINFOHEADER);
        bminfo.bmiHeader.biWidth        = width;
        bminfo.bmiHeader.biHeight       = -(LONG)height;
        bminfo.bmiHeader.biPlanes       = 1;
        bminfo.bmiHeader.biBitCount     = 32;
        bminfo.bmiHeader.biCompression  = BI_RGB;

        // Get a DC for the full screen
        HDC hdcScreen = GetDC(nullptr);

        hr = hdcScreen ? S_OK : E_FAIL;

        // Release the previously allocated bitmap 
        if (SUCCEEDED(hr))
        {
            if (m_hDIBBitmap)
                DeleteObject(m_hDIBBitmap);

            m_hDIBBitmap = CreateDIBSection(hdcScreen, &bminfo, DIB_RGB_COLORS,
                &pvImageBits, nullptr, 0);

            ReleaseDC(nullptr, hdcScreen);

            hr = m_hDIBBitmap ? S_OK : E_FAIL;
        }
    }

    UINT cbStride = 0;
    if (SUCCEEDED(hr))
        // Size of a scan line represented in bytes: 4 bytes each pixel
        hr = UIntMult(width, sizeof(ARGB), &cbStride);
    
    UINT cbImage = 0;
    if (SUCCEEDED(hr))
        // Size of the image, represented in bytes
        hr = UIntMult(cbStride, height, &cbImage);

    // Extract the image into the HBITMAP    
    if (SUCCEEDED(hr))
    {
        hr = m_pTempBitmapSource->CopyPixels(
            nullptr,
            cbStride,
            cbImage, 
            reinterpret_cast<BYTE *> (pvImageBits));
    }

    // Image Extraction failed, clear allocated memory
    if (FAILED(hr))
    {
        DeleteObject(m_hDIBBitmap);
        m_hDIBBitmap = nullptr;
    }

	SafeRelease(m_pTempBitmapSource);

    return hr;
}

HRESULT YasivImage::GetSize(UINT* width, UINT* height)
{
	if(!m_pOriginalBitmapSource)
		return E_FAIL;

	UINT w, h;
	HRESULT hr = m_pOriginalBitmapSource->GetSize(&w, &h);

	if (m_iRotation % 2) // If rotation 90 or 270
	{
		*width = h;
		*height = w;
	}
	else
	{
		*width = w;
		*height = h;
	}

	return hr;
}

HRESULT YasivImage::GetCurrentSize(UINT* width, UINT* height)
{
	if(!m_pOriginalBitmapSource)
		return E_FAIL;

	*width = m_iCurrentWidth;
	*height = m_iCurrentHeight;

	return S_OK;
}

void YasivImage::TestBlur()
{
	UINT uw, uh;
	if(FAILED(GetCurrentSize(&uw, &uh)))
		return;
	int w=uw, h=uh;
	HDC hdcScreen = GetDC(nullptr);
	if(!hdcScreen)
		return;

	BITMAPINFO bminfo;
    ZeroMemory(&bminfo, sizeof(bminfo));
    bminfo.bmiHeader.biSize         = sizeof(BITMAPINFOHEADER);
    bminfo.bmiHeader.biWidth        = w;
    bminfo.bmiHeader.biHeight       = -(LONG)h;
    bminfo.bmiHeader.biPlanes       = 1;
    bminfo.bmiHeader.biBitCount     = 32;
    bminfo.bmiHeader.biCompression  = BI_RGB;

	int size = w * h * 4;
	unsigned char* buffer = new unsigned char[size];
	unsigned char* buffer2 = new unsigned char[size];
	memset(buffer2, 0, size);
	GetDIBits(hdcScreen, m_hDIBBitmap, 0, h, buffer, &bminfo, DIB_RGB_COLORS);

	const int r = 3;

	for(int y=0; y<h; y++)
	{
		for(int x=0; x<w; x++)
		{
			long sum[3];
			int n=0;
			for(int i=0; i<3; i++)
				sum[i] = 0;

			for(int iy=max(y-r, 0); iy<min(y+r+1, h); iy++)
			{
				for(int ix=max(x-r, 0); ix<min(x+r+1, w); ix++)
				{
					int index2 = (iy*w+ix)*4;
					for(int i=0; i<3; i++)
						sum[i] += buffer[index2+i];
					n++;
				}
			}
	
			int index = (y*w+x)*4;
			n = max(n+1, 1);
			
			for(int i=0; i<3; i++)
				buffer2[index+i] = static_cast<unsigned char>(sum[i] / n);
		}
	}

	SetDIBits(hdcScreen, m_hDIBBitmap, 0, h, buffer2, &bminfo, DIB_RGB_COLORS);

	delete[] buffer;
	delete[] buffer2;
}