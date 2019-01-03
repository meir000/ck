/*----------------------------------------------------------------------------
 * Copyright 2007-2018  Kazuo Ishii <kish@wb3.so-net.ne.jp>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *--------------------------------------------------------------------------*/
#include "config.h"

#include <cmath>
#pragma warning(push)
#pragma warning(disable:4995)
#include <vector>
#include <map>
#pragma warning(pop)
#include <emmintrin.h>
#include <smmintrin.h>

#include <dxgi1_2.h>
#include <dcomp.h>
#include <d3d11.h>
#include <d2d1.h>
#include <dwrite.h>
#include <WinCodec.h>
#include <dwmapi.h>

#pragma comment(lib, "Imm32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "WindowsCodecs.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "dwmapi.lib")

//#import  "interface.tlb" raw_interfaces_only
#include "interface.tlh"
#include "util.h"




namespace Ck{

_COM_SMARTPTR_TYPEDEF(IDropTargetHelper, __uuidof(IDropTargetHelper));

#define WM_USER_DROP_HDROP  WM_USER+10
#define WM_USER_DROP_WSTR   WM_USER+11

class droptarget: public IUnknownImpl2<IDropTarget>{
protected:
	HWND   m_parent;
	DWORD  m_eff;
	DWORD  m_key;
	IDropTargetHelperPtr m_help;
public:
	droptarget(HWND hwnd):m_parent(hwnd),m_eff(DROPEFFECT_NONE), m_key(0){
		m_help.CreateInstance(CLSID_DragDropHelper);
	}
	STDMETHOD(DragOver)(DWORD key, POINTL pt, DWORD* eff){
		*eff = m_eff;
		m_key = key;
		if(m_help) m_help->DragOver((POINT*)&pt, *eff);
		return S_OK;
	}
	STDMETHOD(DragLeave)(void){
		if(m_help) m_help->DragLeave();
		return S_OK;
	}
	STDMETHOD(DragEnter)(IDataObject* data, DWORD key, POINTL pt, DWORD* eff){
		FORMATETC fmt = { CF_HDROP, 0,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};
		HRESULT hr = data->QueryGetData(&fmt);
		if(FAILED(hr)){
			fmt.cfFormat = CF_UNICODETEXT;
			hr = data->QueryGetData(&fmt);
		}
		if(FAILED(hr))
			*eff = DROPEFFECT_NONE;
		m_eff = *eff;
		m_key = key;
		if(m_help) m_help->DragEnter(m_parent, data, (POINT*)&pt, *eff);
		return S_OK;
	}
	STDMETHOD(Drop)(IDataObject* data, DWORD key, POINTL pt, DWORD* eff){
		(void) key;
		*eff = m_eff;
		if(m_help) m_help->Drop(data, (POINT*)&pt, *eff);
		if(m_eff != DROPEFFECT_NONE){
			STGMEDIUM stg;
			memset(&stg,0,sizeof(stg));
			FORMATETC fmt = { CF_HDROP, 0,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};
			if(SUCCEEDED(data->GetData(&fmt, &stg))){
				SendMessage(m_parent, WM_USER_DROP_HDROP, (WPARAM)stg.hGlobal, (LPARAM)m_key);
				ReleaseStgMedium(&stg);
			}
			else {
				fmt.cfFormat = CF_UNICODETEXT;
				if(SUCCEEDED(data->GetData(&fmt, &stg))){
					SendMessage(m_parent, WM_USER_DROP_WSTR, (WPARAM)stg.hGlobal, (LPARAM)m_key);
					ReleaseStgMedium(&stg);
				}
			}
		}
		return S_OK;
	}
};



#define SRELEASE(p) if(p){ p->Release(); p=0; }
#define SYSFREE(p)  if(p){ SysFreeString(p); p=0; }
#define HRCHK(func) if(SUCCEEDED(hr)){ hr=func; if(FAILED(hr)){ trace("ERR:0x%08X:line%u:%s\n", hr, __LINE__, #func); }}



inline int64_t  perf_get(){
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return li.QuadPart;
}
inline int64_t  perf_freq(){
	LARGE_INTEGER li;
	QueryPerformanceFrequency(&li);
	return li.QuadPart;
}

inline D2D_COLOR_F  D2DColorF(DWORD argb){
	D2D_COLOR_F  rv;
	__m128i abgr = _mm_shuffle_epi32(_mm_cvtepu8_epi32(_mm_cvtsi32_si128(argb)), _MM_SHUFFLE(3,0,1,2));
	_mm_storeu_ps( (float*)&rv, _mm_mul_ps(_mm_set1_ps(1.0f/255.0f), _mm_cvtepi32_ps(abgr)));
	return rv;
}

inline D2D_RECT_F  D2DRectF(const RECT& rc){
	D2D1_RECT_F  rv;
	__m128  rectf = _mm_cvtepi32_ps( _mm_loadu_si128( (__m128i*)&rc ));
	_mm_storeu_ps( (float*)&rv, rectf);
	return rv;
}

inline D2D_RECT_F  D2DRectF_center(const RECT& rc){
	D2D1_RECT_F  rv;
	__m128  rectf = _mm_cvtepi32_ps( _mm_loadu_si128( (__m128i*)&rc ));
	__m128  offset = { 0.5f, 0.5f, -0.5f, -0.5f };
	_mm_storeu_ps( (float*)&rv, _mm_add_ps(offset,rectf));
	return rv;
}

inline DWORD  composite_argb(DWORD dst, const DWORD src){
	UINT  da, sa, sf;

	da = dst>>24;
	sa = src>>24;

	da = (da * (255-sa)) / 255;
	da = da + sa;
	sf = (da) ? ((sa<<16)/da) : 0;

	DWORD  rv;
	BYTE*  O = (BYTE*)(&rv);
	const BYTE* D = (const BYTE*)(&dst);
	const BYTE* S = (const BYTE*)(&src);
	O[0] = (BYTE) (( (S[0]-D[0]) * sf) >> 16) + D[0];
	O[1] = (BYTE) (( (S[1]-D[1]) * sf) >> 16) + D[1];
	O[2] = (BYTE) (( (S[2]-D[2]) * sf) >> 16) + D[2];
	O[3] = (BYTE) da;
	return rv;
}



class FontManager {
private:
	struct Font {
		IDWriteFont*		font;
		IDWriteFontFace*	fontFace;
		float			emUnit;
		bool			bold;
	};

	struct Cache {
		IDWriteFontFace*	fontFace;
		UINT16			glyphIndex;
		float			advanceWidth;
	};

	IDWriteFactory*			m_pDWriteFactory;
	IDWriteFontCollection*		m_pSysFonts;
	std::vector<Font>		m_fonts;
	std::map<uint32_t,Cache>	m_cache;
	float				m_fontWidth;
	float				m_fontHeight;
	float				m_fontBaseline;
	float				m_fontUnderline;
	float				m_dpiX;
	float				m_dpiY;
	float				m_em2pixelX;
	float				m_em2pixelY;

	void  free_fonts(){
		for(std::vector<Font>::iterator itr = m_fonts.begin(); itr != m_fonts.end(); ++itr){
			(*itr).font->Release();
			(*itr).fontFace->Release();
		}
		m_fonts.clear();
		m_cache.clear();
	}

	void  add_font_cache(UINT32 unicode, Cache* v){
		m_cache[unicode] = (*v);
	}

	void  add_font_face(IDWriteFont* font, IDWriteFontFace* fontFace, UINT32 emUnit, bool bold){
		font->AddRef();
		fontFace->AddRef();
		m_fonts.push_back(Font{ font, fontFace, float(emUnit), bold });
	}

	void  add_font(LPCWSTR familyName){
		if(!m_pSysFonts || !familyName || !familyName[0]) return;

		IDWriteFontFamily*   family = nullptr;
		IDWriteFont*         font = nullptr;
		IDWriteFontFace*     fontFace = nullptr;
		DWRITE_FONT_METRICS  fontMetrics;
		UINT32   index = 0;
		BOOL     exists = FALSE;
		HRESULT  hr;

		hr = m_pSysFonts->FindFamilyName(familyName, &index, &exists);
		if(FAILED(hr) || !exists) return;

		hr = m_pSysFonts->GetFontFamily(index, &family);
		if(FAILED(hr)) return;

		// Normal
		hr = family->GetFirstMatchingFont( DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font);
		if(SUCCEEDED(hr)){
			hr = font->CreateFontFace(&fontFace);
			if(SUCCEEDED(hr)){
				fontFace->GetMetrics(&fontMetrics);
				add_font_face(font, fontFace, fontMetrics.designUnitsPerEm, false);
				fontFace->Release();
			}
			font->Release();
		}

		// Bold
		hr = family->GetFirstMatchingFont( DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font);
		if(SUCCEEDED(hr)){
			hr = font->CreateFontFace(&fontFace);
			if(SUCCEEDED(hr)){
				fontFace->GetMetrics(&fontMetrics);
				add_font_face(font, fontFace, fontMetrics.designUnitsPerEm, true);
				fontFace->Release();
			}
			font->Release();
		}

		family->Release();
	}

	void  init_metrics(){
		if(m_fonts.empty()) return;

		HRESULT  hr = S_OK;
		IDWriteFontFace*  fontFace = m_fonts[0].fontFace;

		static const char*   text = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		UINT32   len = 0;
		UINT32   unicodeText[26];
		UINT16   glyphIndices[26];
		DWRITE_GLYPH_METRICS  glyphMetrics[26];

		for( ; text[len]; ++len){
			unicodeText[len] = (UINT32) text[len];
			glyphIndices[len] = 0;
		}

		hr = fontFace->GetGlyphIndices(unicodeText, len, glyphIndices);
		hr = fontFace->GetDesignGlyphMetrics(glyphIndices, len, glyphMetrics, FALSE);

		float  width = 0.0f;
		for(UINT32 i=0; i < len; ++i)
			width += glyphMetrics[i].advanceWidth;
		width /= float(len);

		DWRITE_FONT_METRICS  fontMetrics;
		fontFace->GetMetrics(&fontMetrics);

		const float  emUnit = float(fontMetrics.designUnitsPerEm);

		m_fontWidth     = (width)                                    / emUnit * m_em2pixelX;
		m_fontHeight    = (fontMetrics.ascent + fontMetrics.descent) / emUnit * m_em2pixelY;
		m_fontBaseline  = (fontMetrics.ascent)                       / emUnit * m_em2pixelY;
		m_fontUnderline = (-fontMetrics.underlinePosition)           / emUnit * m_em2pixelY;

		m_fontWidth  = std::floor(m_fontWidth  + 0.9f);
		m_fontHeight = std::floor(m_fontHeight + 0.9f);
	}

	void  get_glyph(UINT32 unicode, CharFlag style, Cache* output){

		if(unicode < 0x20 || unicode == 0x7F){
			unicode = 0x20;
		}

		const bool    isBold = (style & CharFlag_Bold) != 0;
		const UINT32  boldBit = isBold ? 0x80000000 : 0;

		std::map<uint32_t,Cache>::iterator  find = m_cache.find(unicode | boldBit);
		if(find != m_cache.end()){
			(*output) = (*find).second;
			return;
		}

		BOOL has;
		HRESULT  hr;
		UINT16  glyphIndex;
		DWRITE_GLYPH_METRICS   glyphMetrics;
		DWRITE_FONT_METRICS  fontMetrics;
		IDWriteFontFamily*  family;
		IDWriteFont*        font;
		IDWriteFontFace*    fontFace;

		for(std::vector<Font>::iterator itr = m_fonts.begin(); itr != m_fonts.end(); ++itr){
			if(isBold == (*itr).bold){
				hr = (*itr).font->HasCharacter(unicode, &has);
				if(SUCCEEDED(hr) && has){
					hr = (*itr).fontFace->GetGlyphIndices(&unicode, 1, &glyphIndex);
					if(SUCCEEDED(hr)){
						hr = (*itr).fontFace->GetDesignGlyphMetrics(&glyphIndex, 1, &glyphMetrics, FALSE);
						if(SUCCEEDED(hr)){
							output->fontFace      = (*itr).fontFace;
							output->glyphIndex    = glyphIndex;
							output->advanceWidth  = glyphMetrics.advanceWidth / (*itr).emUnit * m_em2pixelY;

							add_font_cache(unicode | boldBit, output);
							return;
						}
					}
				}
			}
		}

		UINT32  familyCount = m_pSysFonts->GetFontFamilyCount();
		for(UINT32 i=0; i < familyCount; ++i){
			hr = m_pSysFonts->GetFontFamily(i, &family);
			if(SUCCEEDED(hr)){
				hr = family->GetFirstMatchingFont( isBold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font);
				if(SUCCEEDED(hr)){
					hr = font->HasCharacter(unicode, &has);
					if(SUCCEEDED(hr) && has){
						hr = font->CreateFontFace(&fontFace);
						if(SUCCEEDED(hr)){
							fontFace->GetMetrics(&fontMetrics);
							hr = fontFace->GetGlyphIndices(&unicode, 1, &glyphIndex);
							if(SUCCEEDED(hr)){
								hr = fontFace->GetDesignGlyphMetrics(&glyphIndex, 1, &glyphMetrics, FALSE);
								if(SUCCEEDED(hr)){
									output->fontFace      = fontFace;
									output->glyphIndex    = glyphIndex;
									output->advanceWidth  = glyphMetrics.advanceWidth / float(fontMetrics.designUnitsPerEm) * m_em2pixelY;

									add_font_cache(unicode | boldBit, output);
									add_font_face(font, fontFace, fontMetrics.designUnitsPerEm, isBold);
									return;
								}
							}
							fontFace->Release();
						}
					}
					font->Release();
				}
				family->Release();
			}
		}

		if(m_fonts.size() > 0){
			glyphIndex = 0;
			hr = m_fonts[0].fontFace->GetDesignGlyphMetrics(&glyphIndex, 1, &glyphMetrics, FALSE);
			if(SUCCEEDED(hr)){
				output->fontFace      = m_fonts[0].fontFace;
				output->glyphIndex    = glyphIndex;
				output->advanceWidth  = glyphMetrics.advanceWidth / float(m_fonts[0].emUnit) * m_em2pixelY;

				add_font_cache(unicode | boldBit, output);
				return;
			}
		}

		return;
	}

public:
	~FontManager(){
		free_fonts();
	}
	FontManager() : 
		m_pDWriteFactory(nullptr),
		m_pSysFonts(nullptr),
		m_fonts(),
		m_fontWidth(0),
		m_fontHeight(0),
		m_fontBaseline(0),
		m_fontUnderline(0),
		m_dpiX(0),
		m_dpiY(0),
		m_em2pixelX(0),
		m_em2pixelY(0) {

		HRESULT  hr = S_OK;
		HRCHK( DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(*m_pDWriteFactory), (IUnknown**)&m_pDWriteFactory) );
		HRCHK( m_pDWriteFactory->GetSystemFontCollection(&m_pSysFonts, FALSE) );
	}
	void  SetFont(LPCWSTR familyName, float fontSize, float dpiX, float dpiY){
		free_fonts();

		m_dpiX = dpiX;
		m_dpiY = dpiY;
		m_em2pixelX = dpiX / 72.0f * fontSize;
		m_em2pixelY = dpiY / 72.0f * fontSize;

		add_font(familyName);
		add_font(L"Consolas");
		add_font(L"Meiryo");

		init_metrics();

		#if 0
		DWRITE_FONT_METRICS  fontMetrics;
		m_fonts[0].fontFace->GetMetrics(&fontMetrics);

		trace("Font {\n");
		trace("  Family    = %S\n", familyName);
		trace("  Size      = %.2f\n", fontSize);
		trace("  DPI       = %.2f %.2f\n", m_dpiX, m_dpiY);
		trace("  EM2Px     = %.2f %.2f\n", m_em2pixelX, m_em2pixelY);
		trace("  Width     = %.2f\n", m_fontWidth);
		trace("  Height    = %.2f\n", m_fontHeight);
		trace("  Baseline  = %.6f\n", m_fontBaseline);
		trace("  Underline = %.6f\n", m_fontUnderline);
		trace("  Metrics {\n");
		trace("    designUnitsPerEm   = %u\n", fontMetrics.designUnitsPerEm);
		trace("    ascent             = %u\n", fontMetrics.ascent);
		trace("    descent            = %u\n", fontMetrics.descent);
		trace("    lineGap            = %d\n", fontMetrics.lineGap);
		trace("    capHeight          = %u\n", fontMetrics.capHeight);
		trace("    xHeight            = %u\n", fontMetrics.xHeight);
		trace("    underlinePosition  = %d\n", fontMetrics.underlinePosition);
		trace("    underlineThickness = %u\n", fontMetrics.underlineThickness);
		trace("    strikethroughPosition  = %d\n", fontMetrics.strikethroughPosition);
		trace("    strikethroughThickness = %u\n", fontMetrics.strikethroughThickness);
		trace("  }\n");
		trace("}\n");
		#endif
	}
	float  GetEmSize() const {
		return m_em2pixelY;
	}
	float  GetFontWidth() const{
		return m_fontWidth;
	}
	float  GetFontHeight() const{
		return m_fontHeight;
	}
	float  GetBaseline() const{
		return m_fontBaseline;
	}
	float  GetUnderline() const{
		return m_fontUnderline;
	}
	void  GetLOGFONT(LOGFONTW* output) const{
		HRESULT  hr = E_FAIL;
		if(m_fonts.size() > 0){
			IDWriteGdiInterop*  gdi = nullptr;
			hr = m_pDWriteFactory->GetGdiInterop(&gdi);
			if(SUCCEEDED(hr)){
				BOOL  system;
				hr = gdi->ConvertFontToLOGFONT(m_fonts[0].font, output, &system);
				gdi->Release();
			}
		}
		if(FAILED(hr)){
			memset(output, 0, sizeof(*output));
		}
		output->lfHeight = (LONG) -std::floor(m_em2pixelY + 0.9f);
	}

	IDWriteFontFace*  GetGlyph(UINT32 unicode, CharFlag style, UINT16* out_glyphIndex, float* out_advanceWidth){
		Cache  cache;
		get_glyph(unicode, style, &cache);
		(*out_glyphIndex)   = cache.glyphIndex;
		(*out_advanceWidth) = cache.advanceWidth;
		return cache.fontFace;
	}
};



struct ScrollInfo {
	int	nPos;
	int	nPage;
	int	nMax;
	double	trackScale;
	int	hover;
	int	x0, x1;
	int	y0, y1;
	int	y2, y3;
	int	y4, y5;
};


struct CursorInfo {
	int	width;
	int	height;
	bool	active;
	DWORD	cursorColor;
	DWORD	textColor;
	float	baseline;
	FontManager*  fontManager;
	UINT32	  text;
	CharFlag  flags;
};


class DCompScrollButton {
private:
	int const	m_type;
	int		m_posX;
	int		m_posY;
	int		m_width;
	int		m_height;
	DWORD		m_color;
	bool		m_visible;
	bool		m_hover;
	bool		m_dirty;
	IDCompositionVisual3*		m_pVisual;
	IDCompositionVirtualSurface*	m_pSurface;

	void  _geometry_button_up(ID2D1GeometrySink* pSink, const D2D1_RECT_F& rect) {

		pSink->BeginFigure(D2D1::Point2F((rect.right + rect.left)/2, rect.top+1), D2D1_FIGURE_BEGIN_FILLED);
		pSink->AddLine(D2D1::Point2F(rect.left , rect.bottom-1));
		pSink->AddLine(D2D1::Point2F(rect.right, rect.bottom-1));
		pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
	}

	void  _geometry_button_down(ID2D1GeometrySink* pSink, const D2D1_RECT_F& rect) {

		pSink->BeginFigure(D2D1::Point2F((rect.right + rect.left)/2, rect.bottom-1), D2D1_FIGURE_BEGIN_FILLED);
		pSink->AddLine(D2D1::Point2F(rect.left , rect.top+1));
		pSink->AddLine(D2D1::Point2F(rect.right, rect.top+1));
		pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
	}

	void  _geometry_button_bar(ID2D1GeometrySink* pSink, const D2D1_RECT_F& rect) {

		float  left   = rect.left;
		float  right  = rect.right;
		float  center = (right + left)/2;
		float  top    = rect.top;
		float  bottom = rect.bottom;
		float  arc    = (right-left) * 0.6f;

		pSink->BeginFigure(D2D1::Point2F(left, top+arc), D2D1_FIGURE_BEGIN_FILLED);

		pSink->AddQuadraticBezier(D2D1::QuadraticBezierSegment(
			D2D1::Point2F(left, top),
			D2D1::Point2F(center, top)));

		pSink->AddQuadraticBezier(D2D1::QuadraticBezierSegment(
			D2D1::Point2F(right, top),
			D2D1::Point2F(right, top+arc)));

		pSink->AddLine(D2D1::Point2F(right, bottom-arc));

		pSink->AddQuadraticBezier(D2D1::QuadraticBezierSegment(
			D2D1::Point2F(right, bottom),
			D2D1::Point2F(center, bottom)));

		pSink->AddQuadraticBezier(D2D1::QuadraticBezierSegment(
			D2D1::Point2F(left, bottom),
			D2D1::Point2F(left, bottom-arc)));

		pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
	}

public:
	DCompScrollButton(int type) :
		m_type(type),
		m_posX(0),
		m_posY(0),
		m_width(2),
		m_height(2),
		m_color(0),
		m_visible(false),
		m_hover(false),
		m_dirty(false),
		m_pVisual(nullptr),
		m_pSurface(nullptr) {
	}

	void  Clear(){
		SRELEASE( m_pVisual );
		SRELEASE( m_pSurface );
	}

	HRESULT  Init(IDCompositionDevice3* pDCompDevice, IDCompositionVisual2* pParent){
		HRESULT hr = S_OK;
		if(SUCCEEDED(hr) && !m_pSurface){
			m_dirty  = true;
			HRCHK( pDCompDevice->CreateVirtualSurface(m_width, m_height, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ALPHA_MODE_PREMULTIPLIED, &m_pSurface) );
		}
		if(SUCCEEDED(hr) && !m_pVisual){
			IDCompositionVisual2*  pVisual2 = nullptr;
			HRCHK( pDCompDevice->CreateVisual(&pVisual2) );
			HRCHK( pVisual2->QueryInterface(IID_PPV_ARGS(&m_pVisual)) );
			HRCHK( m_pVisual->SetContent(m_pSurface) );
			SRELEASE( pVisual2 );

			HRCHK( pParent->AddVisual(m_pVisual, FALSE, nullptr) );
		}
		return hr;
	}

	HRESULT  Update(int x, int y, int width, int height, DWORD color, bool hover){
		HRESULT  hr = m_pVisual ? S_OK : E_FAIL;
		bool     change = false;
		if(SUCCEEDED(hr)){
			bool  visible = width > 0;
			if(m_visible != visible){
				m_visible = visible;
				change = true;
				HRCHK( m_pVisual->SetVisible( m_visible ? TRUE : FALSE ) );
			}
			if(m_visible){
				if(height < 1) height = 1;

				if(m_dirty || m_width != width || m_height != height || m_color != color){
					m_dirty  = false;
					m_width  = width;
					m_height = height;
					m_color  = color;
					change = true;

					//trace("ScrlButton::Reize w=%d h=%d\n", m_width, m_height);
					HRCHK( m_pSurface->Resize(m_width, m_height) );

					POINT  offset = {};
					ID2D1DeviceContext*  pContext = nullptr;

					HRCHK( m_pSurface->BeginDraw(nullptr, __uuidof(*pContext), (void**)&pContext, &offset) );
					if(SUCCEEDED(hr)){
						D2D1_RECT_F  rect = D2D1::RectF(float(offset.x), float(offset.y), float(offset.x+m_width-3-3), float(offset.y+m_height));

						ID2D1Factory*  pFactory = nullptr;
						ID2D1PathGeometry*  pGeom = nullptr;
						ID2D1GeometrySink*  pSink = nullptr;
						ID2D1SolidColorBrush*  pBrush = nullptr;

						pContext->Clear();
						pContext->GetFactory(&pFactory);
						HRCHK( pFactory->CreatePathGeometry(&pGeom) );
						HRCHK( pGeom->Open(&pSink) );
						if(SUCCEEDED(hr)){
							if(m_type == 1) _geometry_button_up  (pSink, rect);
							if(m_type == 2) _geometry_button_down(pSink, rect);
							if(m_type == 3) _geometry_button_bar (pSink, rect);
							HRCHK( pSink->Close() );

							HRCHK( pContext->CreateSolidColorBrush(D2DColorF(m_color), &pBrush) );
							if(SUCCEEDED(hr)){
								pContext->FillGeometry(pGeom, pBrush);
							}
						}

						SRELEASE( pBrush );
						SRELEASE( pSink );
						SRELEASE( pGeom );
						SRELEASE( pFactory );
						SRELEASE( pContext );

						m_pSurface->EndDraw();
					}
					SRELEASE( pContext );
				}
				if(m_posX != x || m_posY != y || m_hover != hover){
					m_posX = x;
					m_posY = y;
					m_hover = hover;
					change = true;
					//trace("ScrlButton::Pos x=%d x=%d\n", m_posX, m_posY);

					float  opacity = (m_hover) ? 1.0f : (m_type==3) ? 0.7f : 0.1f;
					HRCHK( m_pVisual->SetOpacity(opacity) );

					float  fw = float(m_width-3-3);
					float  hw = (m_hover || m_type!=3) ? (fw) : std::ceil(fw*0.4f);

					float  sx = hw / fw;
					float  sy = 1.0f;
					float  tx = float(m_posX+3) + (fw-hw)/2;
					float  ty = float(m_posY);

					D2D_MATRIX_4X4_F  xf = {
						sx, 0,  0, 0,
						0,  sy, 0, 0,
						0,  0,  1, 0,
						tx, ty, 0, 1,
					};
					HRCHK( m_pVisual->SetTransform(xf) );
				}
			}
		}
		if(SUCCEEDED(hr)){
			hr = change ? S_OK : S_FALSE;
		}
		return hr;
	}
};


class DCompCursor {
private:
	int		m_posX;
	int		m_posY;
	bool		m_visible;
	bool		m_dirty;
	CursorInfo	m_info;
	IDCompositionVisual3*		m_pVisual;
	IDCompositionVirtualSurface*	m_pSurface;
	IDCompositionAnimation*		m_pAnimation;

	void  _draw_active(ID2D1DeviceContext* pContext, const POINT& offset){
		HRESULT hr = S_OK;
		pContext->Clear(D2DColorF(m_info.cursorColor));

		if(m_info.text == 0){
			return;
		}

		UINT16  glIndex = 0;
		float   glWidth = 0;
		IDWriteFontFace*  fontFace = m_info.fontManager->GetGlyph(m_info.text, m_info.flags, &glIndex, &glWidth);

		UINT16  glyphIndices[1];
		float   glyphAdvances[1];
		DWRITE_GLYPH_OFFSET   glyphOffsets[1];

		glyphIndices[0]  = glIndex;
		glyphAdvances[0] = float(m_info.width);
		glyphOffsets[0].advanceOffset = (glyphAdvances[0] - glWidth) * 0.5f;
		glyphOffsets[0].ascenderOffset = 0.0f;

		DWRITE_GLYPH_RUN   run;
		run.fontEmSize    = m_info.fontManager->GetEmSize();
		run.fontFace      = fontFace;
		run.glyphCount    = 1;
		run.glyphIndices  = glyphIndices;
		run.glyphAdvances = glyphAdvances;
		run.glyphOffsets  = glyphOffsets;
		run.isSideways    = FALSE;
		run.bidiLevel     = 0;

		D2D1_POINT_2F      point;
		point.x = float(offset.x);
		point.y = float(offset.y) + m_info.baseline;

		ID2D1SolidColorBrush*  pBrush = nullptr;
		HRCHK( pContext->CreateSolidColorBrush(D2DColorF(m_info.textColor), &pBrush) );
		if(SUCCEEDED(hr)){
			pContext->DrawGlyphRun(point, &run, pBrush);
		}
		SRELEASE( pBrush );
	}

	void  _draw_inactive(ID2D1DeviceContext* pContext, const POINT& offset){
		HRESULT hr = S_OK;
		pContext->Clear(D2D1::ColorF(0,0,0,0));

		ID2D1SolidColorBrush*  pBrush = nullptr;
		HRCHK( pContext->CreateSolidColorBrush(D2DColorF(m_info.cursorColor), &pBrush) );
		if(SUCCEEDED(hr)){
			pContext->DrawRectangle(D2D1::RectF(float(offset.x+.5f), float(offset.y+.5f), float(offset.x+m_info.width-.5f), float(offset.y+m_info.height-.5f)), pBrush);
		}
		SRELEASE( pBrush );
	}

public:
	DCompCursor() : 
		m_posX(0),
		m_posY(0),
		m_visible(false),
		m_dirty(false),
		m_info(),
		m_pVisual(nullptr),
		m_pSurface(nullptr),
		m_pAnimation(nullptr) {
	}

	void  Clear(){
		SRELEASE( m_pVisual );
		SRELEASE( m_pSurface );
		SRELEASE( m_pAnimation );
	}

	HRESULT  Init(IDCompositionDevice3* pDCompDevice, IDCompositionVisual2* pParent){
		HRESULT hr = S_OK;
		if(SUCCEEDED(hr) && !m_pSurface){
			m_dirty = true;
			m_info.width  = 2;
			m_info.height = 2;
			HRCHK( pDCompDevice->CreateVirtualSurface(m_info.width, m_info.height, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ALPHA_MODE_PREMULTIPLIED, &m_pSurface) );
		}
		if(SUCCEEDED(hr) && !m_pAnimation){
			HRCHK( pDCompDevice->CreateAnimation(&m_pAnimation) );
			HRCHK( m_pAnimation->AddCubic(0.0, 1.0f, 0.0f, 0.0f, 0.0f) );        //time, constant, linear, quad, cubic
			HRCHK( m_pAnimation->AddSinusoidal(0.5, 0.5f, 0.5f, 0.75f, 90.0f) ); //time, bias, ampli, freq, phase
		}
		if(SUCCEEDED(hr) && !m_pVisual){
			IDCompositionVisual2*  pVisual2 = nullptr;
			HRCHK( pDCompDevice->CreateVisual(&pVisual2) );
			HRCHK( pVisual2->QueryInterface(IID_PPV_ARGS(&m_pVisual)) );
			HRCHK( m_pVisual->SetContent(m_pSurface) );
			SRELEASE( pVisual2 );

			HRCHK( pParent->AddVisual(m_pVisual, FALSE, nullptr) );
		}
		return hr;
	}

	HRESULT  UpdateBlink(bool blink){
		HRESULT  hr = m_pVisual ? S_OK : E_FAIL;
		if(SUCCEEDED(hr)){
			if(blink){
				HRCHK( m_pVisual->SetOpacity(m_pAnimation) );
			}else{
				HRCHK( m_pVisual->SetOpacity(1.0f) );
			}
		}
		if(SUCCEEDED(hr)){
			hr = S_OK;
		}
		return hr;
	}

	HRESULT  Update(int x, int y, const CursorInfo& info){
		HRESULT  hr = m_pVisual ? S_OK : E_FAIL;
		bool     change  = false;
		if(SUCCEEDED(hr)){
			bool  visible = info.width > 0;

			if(m_visible != visible){
				m_visible = visible;
				change = true;
				HRCHK( m_pVisual->SetVisible( m_visible ? TRUE : FALSE ) );
			}

			if(m_visible){
				if(m_dirty || memcmp(&m_info, &info, sizeof(info)) != 0){
					m_dirty = false;
					m_info = info;
					change = true;

					//trace("AnimCursor::Reize size=%dx%d active=%d bg=%08x fg=%08x bl=%.1f font=%p ch=%x flag=%x\n",
					//      m_info.width, m_info.height, m_info.active, m_info.cursorColor, m_info.textColor,
					//      m_info.baseline, m_info.fontManager, m_info.text, m_info.flags);

					HRCHK( m_pSurface->Resize(m_info.width, m_info.height) );

					ID2D1DeviceContext*    pContext = nullptr;
					POINT   offset;

					HRCHK( m_pSurface->BeginDraw(nullptr, __uuidof(*pContext), (void**)&pContext, &offset) );
					if(SUCCEEDED(hr)){
						if(m_info.active){
							_draw_active(pContext, offset);
						}else{
							_draw_inactive(pContext, offset);
						}
						pContext->Release();
						m_pSurface->EndDraw();
					}
				}

				if(m_posX != x || m_posY != y){
					m_posX = x;
					m_posY = y;
					change = true;
					//trace("AnimCursor::Pos x=%d y=%d\n", m_posX, m_posY);
					HRCHK( m_pVisual->SetOffsetX(float(m_posX)) );
					HRCHK( m_pVisual->SetOffsetY(float(m_posY)) );
				}
			}
		}
		if(SUCCEEDED(hr)){
			hr = change ? S_OK : S_FALSE;
		}
		return hr;
	}

};



class Graphics {
private:
	HWND			m_hwnd;
	int			m_width;
	int			m_height;
	bool			m_swap_dirty;
	bool			m_dcomp_dirty;
	float			m_dpiX;
	float			m_dpiY;

	ID3D11Device*			m_pD3D11Device;
	ID3D11DeviceContext*		m_pD3D11Context;
	ID2D1Device*			m_pD2D1Device;

	IDCompositionDevice3*		m_pDCompDevice;
	IDCompositionTarget*		m_pDCompTarget;
	IDCompositionVisual2*		m_pDCompVisual;
	DCompScrollButton		m_ScrollUp;
	DCompScrollButton		m_ScrollDown;
	DCompScrollButton		m_ScrollBar;
	DCompCursor			m_AnimCursor;

	IDXGISwapChain2*		m_pSwapChain;
	HANDLE				m_hSwapChainEvent;
	ID3D11RenderTargetView*		m_pSwapChainRenderView;

	ID3DBlob*			m_pVshBlob;
	ID3DBlob*			m_pPshBlob;
	ID3D11VertexShader*		m_pVsh;
	ID3D11PixelShader*		m_pPsh;
	ID3D11InputLayout*		m_pInputLayout;
	ID3D11Buffer*			m_pVertices;
	ID3D11Buffer*			m_pConstParams;

	ID3D11SamplerState*		m_pImageSampler;
	ID3D11Texture2D*		m_pImageTexture;
	ID3D11ShaderResourceView*	m_pImageTextureResourceView;

	ID3D11SamplerState*		m_pTextSampler;
	ID3D11Texture2D*		m_pTextTexture;
	ID3D11ShaderResourceView*	m_pTextTextureResourceView;

	ID2D1RenderTarget*		m_pD2D1TextRT;
	ID2D1SolidColorBrush*		m_pForeBrush;

	DWORD			m_fore_color;
	DWORD			m_scrollbar_color;
	DWORD			m_background_color;
	DWORD			m_background_place;
	UINT			m_background_width;
	UINT			m_background_height;
	BSTR			m_background_filepath;
	IWICBitmap*		m_background_bitmap;

public:
	~Graphics(){
		free_device();
		trace("Graphics:dtor\n");
	}

	Graphics(HWND hwnd)
		: m_hwnd(hwnd),
		  m_width(0),
		  m_height(0),
		  m_swap_dirty(false),
		  m_dcomp_dirty(false),
		  m_dpiX(96),
		  m_dpiY(96),

		  m_pD3D11Device(nullptr),
		  m_pD3D11Context(nullptr),
		  m_pD2D1Device(nullptr),

		  m_pDCompDevice(nullptr),
		  m_pDCompTarget(nullptr),
		  m_pDCompVisual(nullptr),
		  m_ScrollUp(1),
		  m_ScrollDown(2),
		  m_ScrollBar(3),
		  m_AnimCursor(),

		  m_pSwapChain(nullptr),
		  m_hSwapChainEvent(nullptr),
		  m_pSwapChainRenderView(nullptr),

		  m_pVshBlob(nullptr),
		  m_pPshBlob(nullptr),
		  m_pVsh(nullptr),
		  m_pPsh(nullptr),
		  m_pInputLayout(nullptr),
		  m_pVertices(nullptr),
		  m_pConstParams(nullptr),

		  m_pImageSampler(nullptr),
		  m_pImageTexture(nullptr),
		  m_pImageTextureResourceView(nullptr),

		  m_pTextSampler(nullptr),
		  m_pTextTexture(nullptr),
		  m_pTextTextureResourceView(nullptr),

		  m_pD2D1TextRT(nullptr),
		  m_pForeBrush(nullptr),

		  m_fore_color(0),
		  m_scrollbar_color(0xFF888888),
		  m_background_color(0xFFFFFFFF),
		  m_background_place(0x03010301),
		  m_background_width(2),
		  m_background_height(2),
		  m_background_filepath(nullptr),
		  m_background_bitmap(nullptr) {
		trace("Graphics:ctor\n");

		HDC  hdc = GetDC(nullptr);
		m_dpiX = (float) GetDeviceCaps(hdc, LOGPIXELSX);
		m_dpiY = (float) GetDeviceCaps(hdc, LOGPIXELSY);
		trace("GDI DPI = %.2f %.2f\n", m_dpiX, m_dpiY);
	}

private:
	static HRESULT  compile_shader(LPCSTR szCode, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut){

		typedef HRESULT ( WINAPI * tD3DCompile)(LPCSTR, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*, ID3DInclude*, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);
		static tD3DCompile  pD3DCompile = nullptr;
		if(pD3DCompile == nullptr){
			HMODULE  dll = LoadLibraryA("d3dcompiler_47.dll");
			if(!dll) dll = LoadLibraryA("d3dcompiler_43.dll");
			if(!dll) dll = LoadLibraryA("d3dcompiler_42.dll");
			if(dll){
				pD3DCompile = (tD3DCompile) GetProcAddress(dll, "D3DCompile");
			}
		}

		DWORD      dwShaderFlags1 = 0;
		DWORD      dwShaderFlags2 = 0;
		ID3DBlob*  pErrorBlob = nullptr;
		HRESULT    hr = S_OK;
		HRCHK( pD3DCompile(szCode, strlen(szCode), "memory.fx", nullptr, nullptr, szEntryPoint, szShaderModel, dwShaderFlags1, dwShaderFlags2, ppBlobOut, &pErrorBlob) );
		if(FAILED(hr)){
			if(pErrorBlob){
				fputs( (const char*)pErrorBlob->GetBufferPointer(), stdout );
				fflush(stdout);
				pErrorBlob->Release();
			}
		}
		return hr;
	}

	void  load_background_image(BSTR cygpath){
		HRESULT   hr = S_OK;

		SRELEASE( m_background_bitmap );
		SYSFREE ( m_background_filepath );

		//trace("[Image-cyg] \"%S\n", cygpath);

		m_background_filepath = SysAllocString(cygpath);
		if(!m_background_filepath) return;

		BSTR winpath = Ck::Util::to_windows_path(cygpath);
		if(!winpath) return;

		//trace("[Image-win] \"%S\n", winpath);

		IWICImagingFactory*     pFactory = nullptr;
		IWICStream*             pStream  = nullptr;
		IWICBitmapDecoder*      pDecoder = nullptr;
		IWICBitmapFrameDecode*  pFrame   = nullptr;
		IWICFormatConverter*	pConvert = nullptr;

		HRCHK( CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory)) );
		HRCHK( pFactory->CreateStream(&pStream) );
		HRCHK( pStream->InitializeFromFilename(winpath, GENERIC_READ) );
		HRCHK( pFactory->CreateDecoderFromStream(pStream, NULL, WICDecodeMetadataCacheOnDemand, &pDecoder) );
		HRCHK( pDecoder->GetFrame(0, &pFrame) );
		HRCHK( pFactory->CreateFormatConverter(&pConvert) );
		HRCHK( pConvert->Initialize(pFrame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeCustom) );
		HRCHK( pFactory->CreateBitmapFromSource(pConvert, WICBitmapCacheOnDemand, &m_background_bitmap) );
		HRCHK( m_background_bitmap->GetSize(&m_background_width, &m_background_height) );

		SRELEASE( pConvert );
		SRELEASE( pFrame );
		SRELEASE( pDecoder );
		SRELEASE( pStream );
		SRELEASE( pFactory );
		SYSFREE( winpath );
	}

	HRESULT  init_device(){
		static const char*  shaderCode = ""
			"SamplerState  gImageSampler : register(s0); \n"
			"SamplerState  gTextSampler  : register(s1); \n"
			"Texture2D     gImageTexture : register(t0); \n"
			"Texture2D     gTextTexture  : register(t1); \n"
			"cbuffer PARAM : register(b0) {\n"
			"	float4	gBackColor;\n"
			"	float2	gImageScale;\n"
			"	float2	gImageOffset;\n"
			"	float2	gGrowPoint0;\n"
			"	float2	gGrowPoint1;\n"
			"	float2	gGrowPoint2;\n"
			"	float2	gGrowPoint3;\n"
			"};\n"
			"struct VSIN{ \n"
			"	float2 Pos   : POSITION; \n"
			"	float2 Tex   : TEXCOORD0; \n"
			"}; \n"
			"struct VSOUT{ \n"
			"	float4 Pos   : SV_POSITION; \n"
			"	float2 Tex0  : TEXCOORD0; \n"
			"	float2 Tex1  : TEXCOORD1; \n"
			"}; \n"
			"VSOUT  vs_main(VSIN IN){ \n"
			"	VSOUT OUT; \n"
			"	OUT.Pos   = float4(IN.Pos, 0.0, 1.0); \n"
			"	OUT.Tex0  = IN.Tex * gImageScale + gImageOffset; \n"
			"	OUT.Tex1  = IN.Tex; \n"
			"	return OUT; \n"
			"} \n"
			"float4  ps_main(VSOUT IN) : SV_Target{ \n"
			"	float4  image = gImageTexture.Sample( gImageSampler, IN.Tex0 ); \n"
			"	float4  color = gTextTexture.Sample( gTextSampler, IN.Tex1 ); \n"
			"	float3  sum; \n"
			"	sum =  gTextTexture.Sample( gTextSampler, IN.Tex1 + gGrowPoint0 ); \n"
			"	sum += gTextTexture.Sample( gTextSampler, IN.Tex1 + gGrowPoint1 ); \n"
			"	sum += gTextTexture.Sample( gTextSampler, IN.Tex1 + gGrowPoint2 ); \n"
			"	sum += gTextTexture.Sample( gTextSampler, IN.Tex1 + gGrowPoint3 ); \n"
			"	sum = abs(sum - gBackColor); \n"
			"	float  alpha = min(1.0, (sum.r + sum.g + sum.b) * 16.0); \n"
			"	color = lerp(image, color, alpha); \n"
			"	color.rgb *= color.a; \n"
			"	return color; \n"
			"} \n"
			"";

		static const D3D11_INPUT_ELEMENT_DESC  layout[]={
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		HRESULT  hr = S_OK;
		bool update_sampler = false;
		bool update_resourceview = false;

		if(SUCCEEDED(hr) && !m_pD3D11Device){
			HRCHK( D3D11CreateDevice( (IDXGIAdapter*)nullptr, D3D_DRIVER_TYPE_HARDWARE, (HMODULE) nullptr,
				D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT,
				nullptr, 0, D3D11_SDK_VERSION, &m_pD3D11Device, nullptr, &m_pD3D11Context) );
		}
		if(SUCCEEDED(hr) && !m_pD2D1Device){
			D2D1_CREATION_PROPERTIES  prop = {};
			prop.threadingMode = D2D1_THREADING_MODE_SINGLE_THREADED;
			prop.debugLevel    = D2D1_DEBUG_LEVEL_NONE; //NONE, ERROR, WARNING, INFORMATION;
			prop.options       = D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS;

			IDXGIDevice*   pDXGIDevice = nullptr;
			HRCHK( m_pD3D11Device->QueryInterface(IID_PPV_ARGS(&pDXGIDevice)) );
			HRCHK( D2D1CreateDevice(pDXGIDevice, &prop, &m_pD2D1Device) );
			SRELEASE( pDXGIDevice );
		}
		if(SUCCEEDED(hr) && !m_pSwapChain){
			IDXGIDevice*     pDXGIDevice  = nullptr;
			IDXGIAdapter*    pDXGIAdapter = nullptr;
			IDXGIFactory2*   pDXGIFactory = nullptr;
			IDXGISwapChain1* pSwapChain1 = nullptr;

			HRCHK( m_pD3D11Device->QueryInterface(IID_PPV_ARGS(&pDXGIDevice)) );
			HRCHK( pDXGIDevice->GetAdapter(&pDXGIAdapter) );
			HRCHK( pDXGIAdapter->GetParent(IID_PPV_ARGS(&pDXGIFactory)) );

			DXGI_SWAP_CHAIN_DESC1  desc = {};
			desc.Width              = m_width;
			desc.Height             = m_height;
			desc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
			desc.Stereo             = FALSE;
			desc.SampleDesc.Count   = 1;
			desc.SampleDesc.Quality = 0;
			desc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			desc.BufferCount        = 2;
			desc.Scaling            = DXGI_SCALING_STRETCH;
			desc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			desc.AlphaMode          = DXGI_ALPHA_MODE_PREMULTIPLIED;
			desc.Flags              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
			HRCHK( pDXGIFactory->CreateSwapChainForComposition(pDXGIDevice, &desc, nullptr, &pSwapChain1) );
			HRCHK( pSwapChain1->QueryInterface(IID_PPV_ARGS(&m_pSwapChain)) );
			HRCHK( m_pSwapChain->SetMaximumFrameLatency(1) );
			if(SUCCEEDED(hr)){
				m_swap_dirty = true;
				m_hSwapChainEvent = m_pSwapChain->GetFrameLatencyWaitableObject();
			}

			SRELEASE( pSwapChain1 );
			SRELEASE( pDXGIFactory );
			SRELEASE( pDXGIAdapter );
			SRELEASE( pDXGIDevice );
		}
		if(SUCCEEDED(hr) && !m_pSwapChainRenderView){
			ID3D11Texture2D*  pBackBuffer = nullptr;
			HRCHK( m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)) );
			HRCHK( m_pD3D11Device->CreateRenderTargetView( pBackBuffer, nullptr, &m_pSwapChainRenderView ) );
			SRELEASE( pBackBuffer );
		}
		if(SUCCEEDED(hr) && !m_pVshBlob){
			HRCHK( compile_shader(shaderCode, "vs_main", "vs_4_0_level_9_3", &m_pVshBlob) );
		}
		if(SUCCEEDED(hr) && !m_pPshBlob){
			HRCHK( compile_shader(shaderCode, "ps_main", "ps_4_0_level_9_3", &m_pPshBlob) );
		}
		if(SUCCEEDED(hr) && !m_pVsh){
			HRCHK( m_pD3D11Device->CreateVertexShader( m_pVshBlob->GetBufferPointer(), m_pVshBlob->GetBufferSize(), nullptr, &m_pVsh ) );
			if(SUCCEEDED(hr)){
				m_pD3D11Context->VSSetShader(m_pVsh,  nullptr, 0);
			}
		}
		if(SUCCEEDED(hr) && !m_pPsh){
			HRCHK( m_pD3D11Device->CreatePixelShader( m_pPshBlob->GetBufferPointer(), m_pPshBlob->GetBufferSize(), nullptr, &m_pPsh ) );
			if(SUCCEEDED(hr)){
				m_pD3D11Context->PSSetShader(m_pPsh,  nullptr, 0);
			}
		}
		if(SUCCEEDED(hr) && !m_pInputLayout){
			HRCHK( m_pD3D11Device->CreateInputLayout ( layout, _countof(layout), m_pVshBlob->GetBufferPointer(), m_pVshBlob->GetBufferSize(), &m_pInputLayout ) );
		}
		if(SUCCEEDED(hr) && !m_pVertices){
			static const float  vertices[] = {
				-1.0f, -1.0f,   0.0f, 1.0f,
				-1.0f,  1.0f,   0.0f, 0.0f,
				 1.0f, -1.0f,   1.0f, 1.0f,
				 1.0f,  1.0f,   1.0f, 0.0f,
			};
			D3D11_BUFFER_DESC  desc = {};
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.ByteWidth = sizeof(vertices);
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = 0;
			D3D11_SUBRESOURCE_DATA  initdata = {};
			initdata.pSysMem = vertices;
			HRCHK( m_pD3D11Device->CreateBuffer(&desc, &initdata, &m_pVertices) );
			if(SUCCEEDED(hr)){
				const UINT  vertexStride = 4*4;
				const UINT  vertexOffset = 0;
				m_pD3D11Context->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
				m_pD3D11Context->IASetInputLayout(m_pInputLayout);
				m_pD3D11Context->IASetVertexBuffers(0, 1, &m_pVertices, &vertexStride, &vertexOffset);
			}
		}
		if(SUCCEEDED(hr) && !m_pConstParams){
			D3D11_BUFFER_DESC  desc = {};
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.ByteWidth = sizeof(float) * 16;
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = 0;
			HRCHK( m_pD3D11Device->CreateBuffer(&desc, nullptr, &m_pConstParams) );
			if(SUCCEEDED(hr)){
				update_viewport();
				update_constant();
				m_pD3D11Context->VSSetConstantBuffers(0, 1, &m_pConstParams);
				m_pD3D11Context->PSSetConstantBuffers(0, 1, &m_pConstParams);
			}
		}
		if(SUCCEEDED(hr) && !m_pImageSampler){
			int  placeY = (m_background_place>>24) & 0xFF;
			int  placeX = (m_background_place>> 8) & 0xFF;

			D3D11_SAMPLER_DESC  desc = {};
			desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			desc.AddressU       = (placeX==Place_Repeat) ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.AddressV       = (placeY==Place_Repeat) ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.MipLODBias     = 0.0f;
			desc.MaxAnisotropy  = 1;
			desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
			desc.BorderColor[0] = 0.0f;
			desc.BorderColor[1] = 0.0f;
			desc.BorderColor[2] = 0.0f;
			desc.BorderColor[3] = 0.0f;
			desc.MinLOD         = -FLT_MAX;
			desc.MaxLOD         =  FLT_MAX;
			HRCHK( m_pD3D11Device->CreateSamplerState(&desc, &m_pImageSampler) );
			update_sampler = true;
		}
		if(SUCCEEDED(hr) && !m_pTextSampler){
			D3D11_SAMPLER_DESC  desc = {};
			desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			desc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.MipLODBias     = 0.0f;
			desc.MaxAnisotropy  = 1;
			desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
			desc.BorderColor[0] = 0.0f;
			desc.BorderColor[1] = 0.0f;
			desc.BorderColor[2] = 0.0f;
			desc.BorderColor[3] = 0.0f;
			desc.MinLOD         = -FLT_MAX;
			desc.MaxLOD         =  FLT_MAX;
			HRCHK( m_pD3D11Device->CreateSamplerState(&desc, &m_pTextSampler) );
			update_sampler = true;
		}
		if(SUCCEEDED(hr) && !m_pImageTexture){
			uint32_t   width =0, height =0;
			uint32_t*  bits = nullptr;

			if(m_background_bitmap){
				HRCHK( m_background_bitmap->GetSize(&width, &height) );
				if(SUCCEEDED(hr)){
					bits = new uint32_t[width * height];
					HRCHK( m_background_bitmap->CopyPixels(nullptr, 4*width, 4*width*height, (BYTE*)bits) );
					if(FAILED(hr)){
						delete [] bits;
						bits = nullptr;
					}
				}
			}
			if(!bits){
				width = height = 2;
				bits = new uint32_t[width * height];
				for(uint32_t i=0; i < width * height; ++i){
					bits[i] = m_background_color;
				}
			}

			m_background_width  = width;
			m_background_height = height;

			D3D11_TEXTURE2D_DESC  desc = {};
			desc.Width              = width;
			desc.Height             = height;
			desc.MipLevels          = 1;
			desc.ArraySize          = 1;
			desc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
			desc.SampleDesc.Count   = 1;
			desc.SampleDesc.Quality = 0;
			desc.Usage              = D3D11_USAGE_DEFAULT;
			desc.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags     = 0;
			desc.MiscFlags          = 0;

			D3D11_SUBRESOURCE_DATA  initdata = {};
			initdata.pSysMem = bits;
			initdata.SysMemPitch = 4 * width;
			initdata.SysMemSlicePitch = 4 * width * height;

			HRCHK( m_pD3D11Device->CreateTexture2D(&desc, &initdata, &m_pImageTexture) );

			delete [] bits;
		}
		if(SUCCEEDED(hr) && !m_pTextTexture){
			D3D11_TEXTURE2D_DESC  desc = {};
			desc.Width              = m_width;
			desc.Height             = m_height;
			desc.MipLevels          = 1;
			desc.ArraySize          = 1;
			desc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
			desc.SampleDesc.Count   = 1;
			desc.SampleDesc.Quality = 0;
			desc.Usage              = D3D11_USAGE_DEFAULT;
			desc.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags     = 0;
			desc.MiscFlags          = 0;
			HRCHK( m_pD3D11Device->CreateTexture2D(&desc, nullptr, &m_pTextTexture) );
		}
		if(SUCCEEDED(hr) && !m_pImageTextureResourceView){
			HRCHK( m_pD3D11Device->CreateShaderResourceView(m_pImageTexture, nullptr, &m_pImageTextureResourceView) );
			update_resourceview = true;
		}
		if(SUCCEEDED(hr) && !m_pTextTextureResourceView){
			HRCHK( m_pD3D11Device->CreateShaderResourceView(m_pTextTexture, nullptr, &m_pTextTextureResourceView) );
			update_resourceview = true;
		}
		if(SUCCEEDED(hr) && !m_pD2D1TextRT){
			D2D1_RENDER_TARGET_PROPERTIES  prop = {};
			prop.type     = D2D1_RENDER_TARGET_TYPE_DEFAULT;
			prop.usage    = D2D1_RENDER_TARGET_USAGE_NONE;
			prop.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;
			prop.pixelFormat.format    = DXGI_FORMAT_UNKNOWN;
			prop.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
			prop.dpiX     = 0;
			prop.dpiY     = 0;

			ID2D1Factory*  pD2D1Factory = nullptr;
			IDXGISurface*  pSurface = nullptr;

			m_pD2D1Device->GetFactory(&pD2D1Factory);
			//pD2D1Factory->ReloadSystemMetrics();
			//pD2D1Factory->GetDesktopDpi(&prop.dpiX, &prop.dpiY);
			//trace("D2D1 DPI = %.2f %.2f\n", prop.dpiX, prop.dpiY);

			HRCHK( m_pTextTexture->QueryInterface(IID_PPV_ARGS(&pSurface)) );
			HRCHK( pD2D1Factory->CreateDxgiSurfaceRenderTarget(pSurface, &prop, &m_pD2D1TextRT) );

			if(SUCCEEDED(hr)){
				m_pD2D1TextRT->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
			}

			SRELEASE( pSurface );
			SRELEASE( pD2D1Factory );
		}
		if(SUCCEEDED(hr) && !m_pForeBrush){
			m_fore_color = 0;
			HRCHK( m_pD2D1TextRT->CreateSolidColorBrush(D2D1::ColorF(0,0,0,0), &m_pForeBrush) );
		}
		if(SUCCEEDED(hr) && !m_pDCompDevice){
			IDCompositionDesktopDevice*  pDesktop = nullptr;
			HRCHK( DCompositionCreateDevice3(m_pD2D1Device, IID_PPV_ARGS(&pDesktop)) );
			HRCHK( pDesktop->QueryInterface(IID_PPV_ARGS(&m_pDCompDevice)) );
			SRELEASE( pDesktop );
		}
		if(SUCCEEDED(hr) && !m_pDCompTarget){
			IDCompositionDesktopDevice*  pDesktop = nullptr;
			HRCHK( m_pDCompDevice->QueryInterface(IID_PPV_ARGS(&pDesktop)) );
			HRCHK( pDesktop->CreateTargetForHwnd(m_hwnd, FALSE, &m_pDCompTarget) );
			SRELEASE( pDesktop );
		}
		if(SUCCEEDED(hr) && !m_pDCompVisual){
			IDCompositionVisual2*  pMainVisual = nullptr;

			HRCHK( m_pDCompDevice->CreateVisual(&pMainVisual) );
			HRCHK( pMainVisual->SetContent(m_pSwapChain) );

			HRCHK( m_pDCompDevice->CreateVisual(&m_pDCompVisual) );
			HRCHK( m_pDCompTarget->SetRoot(m_pDCompVisual) );
			HRCHK( m_pDCompVisual->AddVisual(pMainVisual, FALSE, nullptr) );

			HRCHK( m_ScrollUp.Init(m_pDCompDevice, m_pDCompVisual) );
			HRCHK( m_ScrollDown.Init(m_pDCompDevice, m_pDCompVisual) );
			HRCHK( m_ScrollBar.Init(m_pDCompDevice, m_pDCompVisual) );
			HRCHK( m_AnimCursor.Init(m_pDCompDevice, m_pDCompVisual) );

			HRCHK( m_pDCompDevice->Commit() );

			SRELEASE( pMainVisual );
		}
		if(SUCCEEDED(hr)){
			if(update_sampler){
				ID3D11SamplerState*  samplers[]  = { m_pImageSampler, m_pTextSampler };
				m_pD3D11Context->PSSetSamplers(0, _countof(samplers), samplers);
			}
			if(update_resourceview){
				ID3D11ShaderResourceView*  resources[] = { m_pImageTextureResourceView, m_pTextTextureResourceView };
				m_pD3D11Context->PSSetShaderResources(0, _countof(resources), resources);
			}
		}
		return hr;
	}
	void  free_device(){

		m_AnimCursor.Clear();
		m_ScrollBar.Clear();
		m_ScrollDown.Clear();
		m_ScrollUp.Clear();

		SRELEASE( m_pDCompVisual );
		SRELEASE( m_pDCompTarget );
		SRELEASE( m_pDCompDevice );

		SRELEASE( m_pForeBrush );
		SRELEASE( m_pD2D1TextRT );

		SRELEASE( m_pTextTextureResourceView );
		SRELEASE( m_pTextTexture );
		SRELEASE( m_pTextSampler );

		SRELEASE( m_pImageTextureResourceView );
		SRELEASE( m_pImageTexture );
		SRELEASE( m_pImageSampler );

		SRELEASE( m_pConstParams );
		SRELEASE( m_pVertices );
		SRELEASE( m_pInputLayout );
		SRELEASE( m_pPsh );
		SRELEASE( m_pVsh );
		SRELEASE( m_pPshBlob );
		SRELEASE( m_pVshBlob );

		SRELEASE( m_pSwapChainRenderView );
		SRELEASE( m_pSwapChain );

		SRELEASE( m_pD2D1Device );
		SRELEASE( m_pD3D11Context );
		SRELEASE( m_pD3D11Device );
	}
	void  update_constant(){
		if(m_pD3D11Context && m_pConstParams){
			const int  placeY = (m_background_place>>24) & 0xFF;
			const int  alignY = (m_background_place>>16) & 0xFF;
			const int  placeX = (m_background_place>> 8) & 0xFF;
			const int  alignX = (m_background_place>> 0) & 0xFF;

			float  sx = (float) m_width  / (float) m_background_width;
			float  sy = (float) m_height / (float) m_background_height;
			float  ox = 0.0f;
			float  oy = 0.0f;
			float  contain = (sx<sy) ? sx : sy;
			float  cover   = (sx<sy) ? sy : sx;

			switch(placeX){
			case Place_Scale:     sx=1.0f; break;
			case Place_Contain:   sx=sx/contain; break;
			case Place_Cover:     sx=sx/cover; break;
			case Place_Repeat:    break;
			case Place_NoRepeat:  break;
			}
			switch(placeY){
			case Place_Scale:     sy=1.0f; break;
			case Place_Contain:   sy=sy/contain; break;
			case Place_Cover:     sy=sy/cover; break;
			case Place_Repeat:    break;
			case Place_NoRepeat:  break;
			}

			switch(alignX){
			case Align_Near:   break;
			case Align_Center: ox = 0.5f-(sx/2); break;
			case Align_Far:    ox = 1.0f-sx; break;
			}
			switch(alignY){
			case Align_Near:   break;
			case Align_Center: oy = 0.5f-(sy/2); break;
			case Align_Far:    oy = 1.0f-sy; break;
			}

			struct PARAM {
				float	gBackColor[4];
				float	gImageScale[2];
				float	gImageOffset[2];
				float	gGrowPoint0[2];
				float	gGrowPoint1[2];
				float	gGrowPoint2[2];
				float	gGrowPoint3[2];
			} param;
			const D2D1_COLOR_F  backcolor = D2DColorF(m_background_color);
			const float  hx = 0.5f / (float)m_width;
			const float  hy = 0.5f / (float)m_height;

			param.gBackColor[0] = backcolor.r * 4;
			param.gBackColor[1] = backcolor.g * 4;
			param.gBackColor[2] = backcolor.b * 4;
			param.gBackColor[3] = backcolor.a * 4;

			param.gImageScale[0]  = sx;
			param.gImageScale[1]  = sy;
			param.gImageOffset[0] = ox;
			param.gImageOffset[1] = oy;

			param.gGrowPoint0[0] = -hx;
			param.gGrowPoint0[1] = -hy;
			param.gGrowPoint1[0] =  hx;
			param.gGrowPoint1[1] = -hy;
			param.gGrowPoint2[0] = -hx;
			param.gGrowPoint2[1] =  hy;
			param.gGrowPoint3[0] =  hx;
			param.gGrowPoint3[1] =  hy;

			m_pD3D11Context->UpdateSubresource(m_pConstParams, 0, nullptr, &param, sizeof(param), 0);
		}
	}
	void  update_viewport(){
		if(m_pD3D11Context){
			D3D11_VIEWPORT  viewport;
			viewport.Width   = (FLOAT) m_width;
			viewport.Height  = (FLOAT) m_height;
			viewport.MinDepth = 0.0f;
			viewport.MaxDepth = 1.0f;
			viewport.TopLeftX = 0;
			viewport.TopLeftY = 0;
			m_pD3D11Context->RSSetViewports(1, &viewport);
		}
	}
	void  set_fore_color(DWORD argb){
		if(m_fore_color != argb){
			m_fore_color = argb;
			m_pForeBrush->SetColor( D2DColorF(m_fore_color) );
		}
	}

public:
	int   width() const  { return m_width;  }
	int   height() const { return m_height; }
	void  GetDesktopDpi(float* out_x, float* out_y) const { (*out_x) = m_dpiX; (*out_y) = m_dpiY; }
	DWORD GetBackgroundPlace() const { return m_background_place; }
	BSTR  GetBackgroundFile() const  { return m_background_filepath; }

	void  SetBackgroundPlace(DWORD place) {
		if(place != m_background_place){
			m_background_place = place;
			SRELEASE(m_pImageSampler);
			update_constant();
		}
	}
	void  SetBackgroundFile(BSTR filepath) {
		load_background_image(filepath);
		SRELEASE(m_pImageTextureResourceView);
		SRELEASE(m_pImageTexture);
	}
	void  SetBackgroundColor(DWORD color) {
		m_background_color = color;
		SRELEASE(m_pImageTextureResourceView);
		SRELEASE(m_pImageTexture);
	}
	void  SetScrollbarColor(DWORD color) {
		m_scrollbar_color = color;
	}
	void  Resize(int w, int h) {
		if(m_width != w || m_height != h){
			m_swap_dirty = true;
			m_width  = w;
			m_height = h;

			SRELEASE( m_pD2D1TextRT );
			SRELEASE( m_pTextTextureResourceView );
			SRELEASE( m_pTextTexture );
			SRELEASE( m_pSwapChainRenderView );

			HRESULT hr = S_OK;
			if(m_pSwapChain){
				DWORD  flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
				HRCHK( m_pSwapChain->ResizeBuffers(0, m_width, m_height, DXGI_FORMAT_UNKNOWN, flags) );
				update_viewport();
				update_constant();
			}
		}
	}
	HRESULT   BeginDraw() {
		HRESULT  hr = init_device();
		if(SUCCEEDED(hr)){
			// 前フレーム待ち
			WaitForSingleObject(m_hSwapChainEvent, INFINITE);

			// テキスト描画, 開始
			m_pD2D1TextRT->BeginDraw();
			m_pD2D1TextRT->Clear(D2DColorF(m_background_color | 0xFF000000));
		}
		return 0;
	}
	void  EndDraw() {
		if(m_pSwapChain){
			// テキスト描画, 終了
			m_swap_dirty = false;
			m_pD2D1TextRT->EndDraw();

			// テキストレイヤーと背景をブレンド
			m_pD3D11Context->OMSetRenderTargets(1, &m_pSwapChainRenderView, nullptr);
			m_pD3D11Context->Draw(4,0);

			// 表示
			HRESULT hr = S_OK;
			HRCHK( m_pSwapChain->Present(1,0) );
		}
	}
	void  PushClipRect(const RECT& rc) {
		if(m_pD2D1TextRT){
			m_pD2D1TextRT->PushAxisAlignedClip(D2DRectF(rc), D2D1_ANTIALIAS_MODE_ALIASED);
		}
	}
	void  PopClipRect() {
		if(m_pD2D1TextRT){
			m_pD2D1TextRT->PopAxisAlignedClip();
		}
	}
	void  FillRect(const D2D1_RECT_F& rect, DWORD color) {
		if(m_pForeBrush){
			set_fore_color(color);
			m_pD2D1TextRT->FillRectangle(rect, m_pForeBrush);
		}
	}
	void  DrawLine(const D2D1_POINT_2F& p0, const D2D1_POINT_2F& p1, DWORD color) {
		if(m_pForeBrush){
			set_fore_color(color);
			m_pD2D1TextRT->DrawLine(p0, p1, m_pForeBrush);
		}
	}
	void  DrawGlyphRun(const D2D1_POINT_2F& baselineOrigin, const DWRITE_GLYPH_RUN* glyphRun, DWORD color) {
		if(m_pForeBrush){
			set_fore_color(color);
			m_pD2D1TextRT->DrawGlyphRun(baselineOrigin, glyphRun, m_pForeBrush);
		}
	}
	void  UpdateScrollBar(const ScrollInfo& scrl) {
		HRESULT  hr;
		bool  change = false;

		hr = m_ScrollUp.Update(scrl.x0, scrl.y0, scrl.x1 - scrl.x0, scrl.y1 - scrl.y0, m_scrollbar_color, scrl.hover == 1);
		if(hr == S_OK) change = true;

		hr = m_ScrollDown.Update(scrl.x0, scrl.y4, scrl.x1 - scrl.x0, scrl.y5 - scrl.y4, m_scrollbar_color, scrl.hover == 2);
		if(hr == S_OK) change = true;

		hr = m_ScrollBar.Update(scrl.x0, scrl.y2, scrl.x1 - scrl.x0, scrl.y3 - scrl.y2, m_scrollbar_color, scrl.hover >= 3);
		if(hr == S_OK) change = true;

		if(change){
			//trace("DCompDirty::ScrollBar\n");
			m_dcomp_dirty = true;
		}
	}
	void  UpdateCursorBlink(bool blink){

		HRESULT hr = m_AnimCursor.UpdateBlink(blink);
		if(hr == S_OK){
			//trace("DCompDirty::AnimCursorBlink\n");
			m_dcomp_dirty = true;
		}
	}
	void  UpdateCursor(int x, int y, const CursorInfo& info) {

		HRESULT hr = m_AnimCursor.Update(x, y, info);
		if(hr == S_OK){
			//trace("DCompDirty::AnimCursor\n");
			m_dcomp_dirty = true;
		}
	}
	void  Commit(){
		if(m_pDCompDevice && m_dcomp_dirty){
			m_dcomp_dirty = false;
			HRESULT  hr = S_OK;
			HRCHK( m_pDCompDevice->Commit() );
			//trace("DCompCommit\n");
		}
	}
};





static SIZE  get_window_frame_size(HWND hwnd){
	WINDOWINFO wi;
	wi.cbSize = sizeof(wi);
	GetWindowInfo(hwnd, &wi);

	RECT rc = {0,0,0,0};
	AdjustWindowRectEx(&rc, wi.dwStyle, FALSE, wi.dwExStyle);

	SIZE sz = { rc.right-rc.left, rc.bottom-rc.top };
	return sz;
}


//----------------------------------------------------------------------------

class IWindowNotify_{
public:
	STDMETHOD(OnClosed)() = 0;
	STDMETHOD(OnKeyDown)(DWORD vk) = 0;
	STDMETHOD(OnMouseWheel)(int delta) = 0;
	STDMETHOD(OnMenuInit)(HMENU menu) = 0;
	STDMETHOD(OnMenuExec)(DWORD id) = 0;
	STDMETHOD(OnTitleInit)() = 0;
	STDMETHOD(OnDrop)(BSTR bs, int type, DWORD key) = 0;
	STDMETHOD(OnPasteClipboard)() = 0;
};

//----------------------------------------------------------------------------

class Window_{
	static const int TIMERID_SCREEN = 0x673;
	static const int TIMERID_MOUSE  = 0x674;
	inline void set_screen_timer()  {  SetTimer(m_hwnd, TIMERID_SCREEN, 10, 0);}
	inline void kill_screen_timer() { KillTimer(m_hwnd, TIMERID_SCREEN);}
	inline void set_mouse_timer()   {  SetTimer(m_hwnd, TIMERID_MOUSE, 20, 0);}
	inline void kill_mouse_timer()  { KillTimer(m_hwnd, TIMERID_MOUSE);}

	enum CID{
		CID_FG = 256,
		CID_BG,
		CID_Select,
		CID_Cursor,
		CID_ImeCursor,
		CID_Scrollbar,
		CID_Max,
	};

	IPty*		m_pty;
	IWindowNotify_* const  m_notify;
	HWND		m_hwnd;
	int		m_win_posx;
	int		m_win_posy;
	int		m_win_width;
	int		m_win_height;
	int		m_char_width;
	int		m_char_height;
	Graphics*	m_graphics;
	FontManager	m_fontManager;
	Snapshot*	m_snapshot;
	int		m_screen_tick;
	float		m_font_size;
	int		m_font_width;
	int		m_font_height;
	LOGFONT 	m_font_log;
	MouseCmd	m_lbtn_cmd;
	MouseCmd	m_mbtn_cmd;
	MouseCmd	m_rbtn_cmd;
	WinZOrder	m_zorder;
	RECT		m_border;
	int		m_linespace;
	int		m_vscrl_size;
	int		m_vscrl_mode;
	ScrollInfo	m_vscrl;
	int		m_draw_request;
	int		m_wheel_delta;
	bool		m_blink_cursor;
	bool		m_is_active;
	VARIANT_BOOL	m_ime_on;
	//
	DWORD		m_colors[CID_Max];
	BYTE		m_bg_alpha;
	//
	bool		m_mouse_track;
	bool		m_click;
	bool		m_click_moved;
	BYTE		m_click_count;
	MouseCmd	m_click_cmd;
	DWORD		m_click_time;
	int		m_click_posx;
	int		m_click_posy;
	int		m_click_scroll;
	int		m_click_dx;
	int		m_click_dy;

protected:
	void draw_text(RECT& rect, CharFlag style, UINT32* unicodeText, FLOAT* unicodeStep, UINT32 unicodeLen);
	void draw_screen();

	HRESULT _setup_font(LPCWSTR familyName, float fontSize){

		if(familyName){
			StringCbCopy(m_font_log.lfFaceName, sizeof(m_font_log.lfFaceName), familyName);
		}

		if(fontSize < 8.0f)  fontSize = 8.0f;
		if(fontSize > 72.0f) fontSize = 72.0f;

		float  dpiX, dpiY;
		m_graphics->GetDesktopDpi(&dpiX, &dpiY);

		m_fontManager.SetFont(m_font_log.lfFaceName, fontSize, dpiX, dpiY);

		m_font_size   = fontSize;
		m_font_width  = (int) m_fontManager.GetFontWidth();
		m_font_height = (int) m_fontManager.GetFontHeight();

		m_fontManager.GetLOGFONT(&m_font_log);

		m_draw_request = 2;
		Resize(m_char_width, m_char_height);

		HIMC imc = ImmGetContext(m_hwnd);
		if(imc){
			ImmSetCompositionFont(imc, &m_font_log);
			ImmReleaseContext(m_hwnd,imc);
		}
		return S_OK;
	}
	void popup_menu(bool curpos){
		POINT pt;
		if(curpos){
			GetCursorPos(&pt);
		}
		else{
			pt.x=0;
			pt.y=-1;
			ClientToScreen(m_hwnd,&pt);
		}
		HMENU menu = CreatePopupMenu();
		m_notify->OnMenuInit((HMENU)menu);
		//
		DWORD id = (DWORD) TrackPopupMenuEx(menu, TPM_LEFTALIGN|TPM_TOPALIGN|TPM_LEFTBUTTON|TPM_RETURNCMD, pt.x, pt.y, m_hwnd, 0);
		DestroyMenu(menu);
		//
		m_notify->OnMenuExec(id);
	}

	void ime_set_state(){
		HIMC imc = ImmGetContext(m_hwnd);
		if(imc){
			ImmSetOpenStatus(imc, m_ime_on);
			ImmSetCompositionFont(imc, &m_font_log);
			ImmReleaseContext(m_hwnd, imc);
		}
	}
	void ime_set_position(){
		if(m_pty){
			HIMC imc = ImmGetContext(m_hwnd);
			if(imc){
				int x,y;
				m_pty->get_CursorPosX(&x);
				m_pty->get_CursorPosY(&y);
				x = m_border.left + 0 + (x * (m_font_width + 0));
				y = m_border.top  + (m_linespace>>1) + (y * (m_font_height + m_linespace));
				COMPOSITIONFORM cf;
				cf.dwStyle = CFS_POINT;
				cf.ptCurrentPos.x = x;
				cf.ptCurrentPos.y = y;
				ImmSetCompositionWindow(imc,&cf);
				ImmReleaseContext(m_hwnd,imc);
			}
		}
	}
	void ime_send_result_string(){
		if(m_pty){
			HIMC imc = ImmGetContext(m_hwnd);
			if(imc){
				LONG n = ImmGetCompositionString(imc, GCS_RESULTSTR, 0, 0);
				if(n>0){
					BSTR bs = SysAllocStringByteLen(0, n+sizeof(WCHAR));
					if(bs){
						n = ImmGetCompositionString(imc, GCS_RESULTSTR, bs, n);
						if(n>0){
							bs[n/sizeof(WCHAR)] = '\0';
							((UINT*)bs)[-1] = n;
							m_pty->PutString(bs);
						}
						SysFreeString(bs);
					}
				}
				ImmReleaseContext(m_hwnd,imc);
			}
		}
	}
	//
	static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp){
		Window_* p;
		if(msg==WM_CREATE){
			p = (Window_*) ((CREATESTRUCT*)lp)->lpCreateParams;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)p);
		}
		else{
			p = (Window_*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
		}
		return (p)? p->wm_on_message(hwnd,msg,wp,lp): DefWindowProc(hwnd,msg,wp,lp);
	}
	LRESULT wm_on_message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp){
		switch(msg){
		case WM_NCDESTROY:
			kill_mouse_timer();
			kill_screen_timer();
			if(m_graphics){
				delete m_graphics;
				m_graphics = 0;
			}
			RevokeDragDrop(m_hwnd);
			DefWindowProc(hwnd,msg,wp,lp);
			trace("Window_::wm_on_nc_destroy\n");
			m_hwnd = 0;
			m_notify->OnClosed();
			return 0;
		case WM_CREATE:
			m_hwnd = hwnd;
			wm_on_create();
			break;
		case WM_ACTIVATE:
			if(LOWORD(wp) == WA_INACTIVE){
				m_is_active = false;
				m_wheel_delta = 0;
				m_draw_request = 1;
				m_graphics->UpdateCursorBlink(m_is_active & m_blink_cursor);
			}
			else{//active
				m_is_active = true;
				ime_set_state();
				m_draw_request = 1;
				m_graphics->UpdateCursorBlink(m_is_active & m_blink_cursor);

				if(m_pty){
					m_pty->Resize(m_char_width, m_char_height);
				}
			}
			break;
		case WM_GETMINMAXINFO:     wm_on_get_min_max_info((MINMAXINFO*)lp);return 0;
		case WM_WINDOWPOSCHANGING: wm_on_window_pos_changing((WINDOWPOS*)lp);return 0;
		case WM_WINDOWPOSCHANGED:  wm_on_window_pos_changed ((WINDOWPOS*)lp);return 0;
		case WM_TIMER:       wm_on_timer((DWORD)wp);break;
		case WM_MOUSEMOVE:   wm_on_mouse_move(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));return 0;
		case WM_LBUTTONDOWN: wm_on_mouse_down(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), m_lbtn_cmd);return 0;
		case WM_LBUTTONUP:   wm_on_mouse_up  (GET_X_LPARAM(lp), GET_Y_LPARAM(lp), m_lbtn_cmd);return 0;
		case WM_MBUTTONDOWN: wm_on_mouse_down(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), m_mbtn_cmd);return 0;
		case WM_MBUTTONUP:   wm_on_mouse_up  (GET_X_LPARAM(lp), GET_Y_LPARAM(lp), m_mbtn_cmd);return 0;
		case WM_RBUTTONDOWN: wm_on_mouse_down(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), m_rbtn_cmd);return 0;
		case WM_RBUTTONUP:   wm_on_mouse_up  (GET_X_LPARAM(lp), GET_Y_LPARAM(lp), m_rbtn_cmd);return 0;
		case WM_MOUSELEAVE:  wm_on_mouse_leave(); return 0;

		case WM_ERASEBKGND:
			return 1;
		case WM_PAINT:
			wm_on_paint();
			return 0;
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
			m_draw_request = 1;
			m_graphics->UpdateCursorBlink(m_is_active & m_blink_cursor);
			m_notify->OnKeyDown((DWORD)wp);
			return 0;
		case WM_MOUSEWHEEL:
			{
				m_wheel_delta += GET_WHEEL_DELTA_WPARAM(wp);
				int notch = m_wheel_delta / WHEEL_DELTA;
				if(notch){
					m_wheel_delta -= notch * WHEEL_DELTA;
					m_notify->OnMouseWheel(notch);
				}
			}
			return 0;
		case WM_INITMENUPOPUP:
			if(HIWORD(lp)){
				UINT nb=GetMenuItemCount((HMENU)wp);
				for(UINT i=0; i<nb; i++){
					if(GetMenuItemID((HMENU)wp, i)==SC_CLOSE){
						for(UINT pos=(i+=2); i<nb; i++)
							DeleteMenu((HMENU)wp, pos, MF_BYPOSITION);
						break;
					}
				}
				m_notify->OnMenuInit((HMENU)wp);
			}
			break;
		case WM_SYSCOMMAND:
			if((wp & 0xFFF0) < 0xF000){
				m_notify->OnMenuExec((DWORD)wp);
				return 0;
			}
			break;
		case WM_CONTEXTMENU:
			if(GET_X_LPARAM(lp)==-1 && GET_Y_LPARAM(lp)==-1){
				popup_menu(false);
				return 0;
			}
			break;
		case WM_CHAR:
		case WM_SYSCHAR:
		case WM_IME_CHAR:
			return 0;
		case WM_IME_NOTIFY:
			if(wp == IMN_SETOPENSTATUS){
				if(m_is_active){
					HIMC imc = ImmGetContext(m_hwnd);
					if(imc){
						m_ime_on = ImmGetOpenStatus(imc)? TRUE: FALSE;
						ImmReleaseContext(m_hwnd, imc);
						m_draw_request = 1;
					}
				}
			}
			break;
		case WM_IME_STARTCOMPOSITION:
			ime_set_position();
			break;
		case WM_IME_COMPOSITION:
			if(lp & GCS_CURSORPOS)
				ime_set_position();
			if(lp & GCS_RESULTSTR)
				ime_send_result_string();
			break;
		case WM_DWMCOMPOSITIONCHANGED:
			//Util::set_window_transp(m_hwnd, m_transp_mode);
			break;
		case WM_USER_DROP_HDROP:
			if(wm_on_user_drop_hdrop((HGLOBAL)wp,(BOOL)lp))
				return 0;
			break;
		case WM_USER_DROP_WSTR:
			if(wm_on_user_drop_wstr((HGLOBAL)wp,(BOOL)lp))
				return 0;
			break;
		}
		return DefWindowProc(hwnd,msg,wp,lp);
	}
	void wm_on_create(){
		HMENU menu = GetSystemMenu(m_hwnd,FALSE);
		if(menu){
			MENUITEMINFO mi;
			mi.cbSize = sizeof(mi);
			mi.fType = MFT_SEPARATOR;
			mi.fMask = MIIM_FTYPE;
			InsertMenuItem(menu, GetMenuItemCount(menu), TRUE, &mi);
		}

		m_graphics = new Graphics(m_hwnd);

		float  dpiX, dpiY;
		m_graphics->GetDesktopDpi(&dpiX, &dpiY);

		m_vscrl_size = (int) std::ceil( 13.0f * dpiY / 96.0f );

		_setup_font(L"", 10.5f);

		IDropTarget* p = new droptarget(m_hwnd);
		p->AddRef();
		RegisterDragDrop(m_hwnd, p);
		p->Release();

		set_screen_timer();

		//Util::set_window_composition_grass_filter(m_hwnd);
	}

	void wm_on_get_min_max_info(MINMAXINFO* p){
		SIZE frame = get_window_frame_size(m_hwnd);
		int wx = frame.cx + (m_border.left + m_border.right) + (m_font_width + 0);
		int wy = frame.cy + (m_border.top + m_border.bottom) + (m_font_height + m_linespace);
		if(m_vscrl_mode) wx += m_vscrl_size;
		p->ptMinTrackSize.x = wx;
		p->ptMinTrackSize.y = wy;
	}
	void wm_on_window_pos_changing(WINDOWPOS* p){
		if(m_zorder == WinZOrder_Bottom){
			if(!(p->flags & SWP_NOZORDER))
				p->hwndInsertAfter = HWND_BOTTOM;
		}
		if(!(p->flags & SWP_NOSIZE) && !IsZoomed(m_hwnd)){
			SIZE frame = get_window_frame_size(m_hwnd);
			int  vscrl = (m_vscrl_mode) ? m_vscrl_size : 0;

			int  x = (p->cx) - (frame.cx + m_border.left + m_border.right + vscrl);
			int  y = (p->cy) - (frame.cy + m_border.top  + m_border.bottom       );

			int chaW = x / (m_font_width  + 0);
			int chaH = y / (m_font_height + m_linespace);

			x = (frame.cx + m_border.left + m_border.right + vscrl) + (chaW * (m_font_width  + 0));
			y = (frame.cy + m_border.top  + m_border.bottom       ) + (chaH * (m_font_height + m_linespace));

			if(!(p->flags & SWP_NOMOVE)){
				if(p->x != m_win_posx) p->x += (p->cx - x);
				if(p->y != m_win_posy) p->y += (p->cy - y);
			}

			p->cx = x;
			p->cy = y;
		}
	}
	void wm_on_window_pos_changed(WINDOWPOS* p){

		if(!(p->flags & SWP_NOSIZE)){
			SIZE frame = get_window_frame_size(m_hwnd);
			int  vscrl = (m_vscrl_mode) ? m_vscrl_size : 0;

			int  x = (p->cx) - (frame.cx + m_border.left + m_border.right + vscrl);
			int  y = (p->cy) - (frame.cy + m_border.top  + m_border.bottom       );

			int chaW = x / (m_font_width  + 0);
			int chaH = y / (m_font_height + m_linespace);

			int cliW, cliH;
			if(IsZoomed(m_hwnd)){
				cliW = p->cx - frame.cx;
				cliH = p->cy - frame.cy;
			}else{
				cliW = (m_border.left + m_border.right + vscrl) + (chaW * (m_font_width  + 0));
				cliH = (m_border.top  + m_border.bottom       ) + (chaH * (m_font_height + m_linespace));

				x = (cliW + frame.cx);
				y = (cliH + frame.cy);
				if(x != p->cx || y != p->cy){
					if(!(p->flags & SWP_NOMOVE)){
						if(p->x != m_win_posx) p->x += (p->cx - x);
						if(p->y != m_win_posy) p->y += (p->cy - y);
						SetWindowPos(m_hwnd,0, p->x, p->y, x, y, SWP_NOZORDER|SWP_NOACTIVATE);
					}
					else{
						SetWindowPos(m_hwnd,0, 0, 0, x, y, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
					}
					return;
				}
			}

			if(m_char_width != chaW || m_char_height != chaH){
				m_char_width  = chaW;
				m_char_height = chaH;
				if(m_pty){
					m_pty->Resize(chaW, chaH);
				}
			}

			m_win_width  = cliW;
			m_win_height = cliH;

			if(m_graphics->width() != cliW || m_graphics->height() != cliH){
				m_graphics->Resize(cliW, cliH);
				m_draw_request = 2;
				draw_screen();
			}
		}

		if(!(p->flags & SWP_NOMOVE)){
			m_win_posx = p->x;
			m_win_posy = p->y;
		}
	}

	void wm_on_paint(){
		ValidateRect(m_hwnd, NULL);
		//draw_screen();
	}

	void on_select_move(int x, int y, bool start){
		if(m_pty){
			//int  i;
			//m_pty->get_ViewPos(&i);
			m_pty->SetSelection(x, y, (m_click_count & 3) | (start ? 4 : 0));
		}
	}

	MouseCmd  _scroll_hittest(int x, int y){
		MouseCmd  cmd = MouseCmd_None;
		if(m_vscrl.x0 <= x && x < m_vscrl.x1){
			if(y < m_vscrl.y0){
			}else if(y <  m_vscrl.y1){
				cmd = MouseCmd_ScrollUp;
			}else if(y >= m_vscrl.y4){
				cmd = MouseCmd_ScrollDown;
			}else if(y <  m_vscrl.y2){
				cmd = MouseCmd_ScrollPageUp;
			}else if(y >= m_vscrl.y3){
				cmd = MouseCmd_ScrollPageDown;
			}else{
				cmd = MouseCmd_ScrollTrack;
			}
		}
		return cmd;
	}

	void  _scroll_rel(int n){
		int  i;
		m_pty->get_ViewPos(&i);
		m_pty->put_ViewPos(i+n);
	}

	void  _scroll_timer_rel(int n){
		DWORD t = GetTickCount() - m_click_time;
		if(t >= 350){ //350ms
			int  i;
			m_pty->get_ViewPos(&i);
			if(m_click_scroll > 0){
				if(n<0){
					if(i <= m_click_scroll)
						return;
				}else{
					if(i+n >= m_click_scroll)
						return;
				}
			}
			m_pty->put_ViewPos(i+n);
		}
	}

	void wm_on_mouse_down(int x, int y, MouseCmd cmd){

		MouseCmd  test = _scroll_hittest(x,y);
		if(test != MouseCmd_None){
			cmd = test;
		}

		DWORD now = GetTickCount();
		DWORD st = (now>m_click_time) ? (now-m_click_time) : (now + ~m_click_time +1);
		int   sx = (x>m_click_posx)? (x-m_click_posx) : (m_click_posx-x);
		int   sy = (y>m_click_posy)? (y-m_click_posy) : (m_click_posy-y);
		if(cmd == m_click_cmd &&
		   st <= GetDoubleClickTime() &&
		   sx <= GetSystemMetrics(SM_CXDOUBLECLK) &&
		   sy <= GetSystemMetrics(SM_CYDOUBLECLK))
			m_click_count++;
		else
			m_click_count=0;
		m_click_time = now;
		m_click_cmd = cmd;
		m_click = true;
		m_click_moved = false;
		m_click_posx = x;
		m_click_posy = y;
		SetCapture(m_hwnd);
		int  vscrl = (m_vscrl_mode==3) ? m_vscrl_size : 0;
		m_click_dx = (x - m_border.left - vscrl) / (m_font_width + 0);
		m_click_dy = (y - m_border.top         ) / (m_font_height + m_linespace);
		if(m_click_cmd == MouseCmd_Select){
			on_select_move(m_click_dx, m_click_dy, true);
			set_mouse_timer();
		}
		else if(m_click_cmd == MouseCmd_Paste){
			m_click_scroll = m_click_dy;
			set_mouse_timer();
		}
		else if(m_click_cmd == MouseCmd_Menu){
		}
		else if(m_click_cmd == MouseCmd_ScrollUp){
			m_click_scroll = -1;
			set_mouse_timer();
			_scroll_rel(-1);
		}
		else if(m_click_cmd == MouseCmd_ScrollDown){
			m_click_scroll = -1;
			set_mouse_timer();
			_scroll_rel(1);
		}
		else if(m_click_cmd == MouseCmd_ScrollPageUp){
			m_click_scroll = (int) std::round( m_vscrl.nMax * double(y - m_vscrl.y1) / double(m_vscrl.y4 - m_vscrl.y1) );
			set_mouse_timer();
			_scroll_rel(-m_vscrl.nPage);
		}
		else if(m_click_cmd == MouseCmd_ScrollPageDown){
			m_click_scroll = (int) std::round( m_vscrl.nMax * double(y - m_vscrl.y1) / double(m_vscrl.y4 - m_vscrl.y1) );
			set_mouse_timer();
			_scroll_rel(m_vscrl.nPage);
		}
		else if(m_click_cmd == MouseCmd_ScrollTrack){
			m_click_scroll = y - m_vscrl.y2;
		}
	}
	void wm_on_timer(DWORD id){
		if(id == TIMERID_SCREEN){
			draw_screen();
		}
		else if(id == TIMERID_MOUSE){
			if(m_click && m_pty){
				if(m_click_cmd == MouseCmd_Select){
					//trace("timer %d %d\n", m_select_x2,m_select_y2);
					int n;
					if(m_click_dy < 0){
						n = (m_click_dy);
						if(n<-10) n=-10;
					}
					else if(m_click_dy >= m_char_height){
						n = (m_click_dy - m_char_height) +1;
						if(n>10) n=10;
					}
					else{
						return;
					}
					int  i;
					m_pty->get_ViewPos(&i);
					m_pty->put_ViewPos(i+n);
					m_pty->get_ViewPos(&n);
					if(n != i){
						on_select_move(m_click_dx, m_click_dy, false);
					}
				}
				else if(m_click_cmd == MouseCmd_Paste){
					int n = (m_click_dy - m_click_scroll) / 2;
					if(n<-10) n=-10; else if(n>10) n=10;
					_scroll_rel(n);
				}
				else if(m_click_cmd == MouseCmd_ScrollUp){
					_scroll_timer_rel(-1);
				}
				else if(m_click_cmd == MouseCmd_ScrollDown){
					_scroll_timer_rel(1);
				}
				else if(m_click_cmd == MouseCmd_ScrollPageUp){
					_scroll_timer_rel(-m_vscrl.nPage);
				}
				else if(m_click_cmd == MouseCmd_ScrollPageDown){
					_scroll_timer_rel(m_vscrl.nPage);
				}
			}
		}
	}
	void wm_on_mouse_move(int x, int y){
		if(m_click){
			if(m_click_cmd == MouseCmd_Select || 
			   m_click_cmd == MouseCmd_Paste || 
			   m_click_cmd == MouseCmd_Menu ){
				int  vscrl = (m_vscrl_mode==3) ? m_vscrl_size : 0;
				int  chaX = (x - m_border.left - vscrl) / (m_font_width + 0);
				int  chaY = (y - m_border.top         ) / (m_font_height + m_linespace);
				if(m_click_dx != chaX || m_click_dy != chaY){
					m_click_moved = true;
					m_click_dx = chaX;
					m_click_dy = chaY;
					if(m_click_cmd == MouseCmd_Select){
						on_select_move(chaX,chaY,false);
					}
				}
			}
			else if(m_click_cmd == MouseCmd_ScrollTrack){
				int  pos = (int) std::round((y - m_vscrl.y1 - m_click_scroll) / m_vscrl.trackScale);
				m_pty->put_ViewPos(pos);
			}
		}else{
			MouseCmd  test = _scroll_hittest(x,y);
			int  hover = 0;
			if(test == MouseCmd_ScrollUp) {
				hover = 1;
			} else if(test == MouseCmd_ScrollDown) {
				hover = 2;
			} else if(test == MouseCmd_ScrollPageUp) {
				hover = 3;
			} else if(test == MouseCmd_ScrollPageDown) {
				hover = 4;
			} else if(test == MouseCmd_ScrollTrack) {
				hover = 5;
			} else{
				hover = 0;
			}
			if(m_vscrl.hover != hover){
				m_vscrl.hover = hover;
				m_graphics->UpdateScrollBar(m_vscrl);
				m_graphics->Commit();
			}
		}

		if(!m_mouse_track){
			TRACKMOUSEEVENT  me = {sizeof(me),TME_LEAVE,m_hwnd,0};
			if(TrackMouseEvent(&me)){
				m_mouse_track = true;
			}
		}
	}
	void wm_on_mouse_up(int x, int y, MouseCmd cmd){
		(void) cmd;
		if(m_click){
			m_click = false;
			ReleaseCapture();

			int  vscrl = (m_vscrl_mode==3) ? m_vscrl_size : 0;
			int  chaX = (x - m_border.left - vscrl) / (m_font_width + 0);
			int  chaY = (y - m_border.top         ) / (m_font_height + m_linespace);
			if(m_click_cmd == MouseCmd_Select){
				kill_mouse_timer();
				if(m_click_dx != chaX || m_click_dy != chaY)
					on_select_move(chaX,chaY,false);
				if(m_pty){
					BSTR bs;
					m_pty->get_SelectedString(&bs);
					if(bs){
						Util::set_clipboard_text(bs);
						SysFreeString(bs);
					}
				}
			}
			else if(m_click_cmd == MouseCmd_Paste){
				kill_mouse_timer();
				if(!m_click_moved){
					m_notify->OnPasteClipboard();
				}
			}
			else if(m_click_cmd == MouseCmd_Menu){
				popup_menu(true);
			}
			else if(m_click_cmd == MouseCmd_ScrollUp){
				kill_mouse_timer();
			}
			else if(m_click_cmd == MouseCmd_ScrollDown){
				kill_mouse_timer();
			}
			else if(m_click_cmd == MouseCmd_ScrollTrack){
			}
		}
	}
	void  wm_on_mouse_leave(){
		m_mouse_track = false;
		if(m_vscrl.hover != 0){
			m_vscrl.hover = 0;
			m_graphics->UpdateScrollBar(m_vscrl);
			m_graphics->Commit();
		}
	}

	bool wm_on_user_drop_hdrop(HGLOBAL mem, DWORD key){
		if(mem){
			HDROP drop = (HDROP) GlobalLock(mem);
			if(drop){
				BSTR  bs = SysAllocStringLen(0, MAX_PATH+32);
				if(bs){
					DWORD nb = DragQueryFile(drop, (DWORD)-1, 0,0);
					for(DWORD i=0; i<nb; i++){
						int len = (int) DragQueryFile(drop, i, bs, MAX_PATH+8);
						if(len>0){
							bs[len] = '\0';
							((UINT*)bs)[-1] = len * sizeof(WCHAR);
							m_notify->OnDrop(bs, 1, key);
						}
					}
					SysFreeString(bs);
				}
				GlobalUnlock(mem);
				return true;
			}
		}
		return false;
	}
	bool wm_on_user_drop_wstr(HGLOBAL mem, DWORD key){
		if(mem){
			LPWSTR wstr = (LPWSTR) GlobalLock(mem);
			if(wstr){
				BSTR bs = SysAllocString(wstr);
				if(bs){
					m_notify->OnDrop(bs, 0, key);
				}
				GlobalUnlock(mem);
				return true;
			}
		}
		return false;
	}

	//---

	void _finalize(){
		trace("Window_::dtor\n");
		if(m_pty){
			if(m_snapshot){
				m_pty->_del_snapshot(m_snapshot);
				m_snapshot = 0;
			}
			m_pty->Release();
			m_pty = 0;
		}
		if(m_hwnd){
			DestroyWindow(m_hwnd);
		}
	}
public:
	~Window_(){ _finalize();}
	Window_(IWindowNotify_* cb)
		: m_pty(0),
		  m_notify(cb),
		  m_hwnd(0),
		  m_win_posx(0),
		  m_win_posy(0),
		  m_win_width(0),
		  m_win_height(0),
		  m_char_width(80),
		  m_char_height(24),
		  m_graphics(0),
		  m_fontManager(),
		  m_snapshot(0),
		  m_screen_tick(0),
		  m_font_size(10.0f),
		  m_font_width(0),
		  m_font_height(0),
		  m_lbtn_cmd(MouseCmd_Select),
		  m_mbtn_cmd(MouseCmd_Paste),
		  m_rbtn_cmd(MouseCmd_Menu),
		  m_zorder(WinZOrder_Normal),
		  m_linespace(0),
		  m_vscrl_size(13),
		  m_vscrl_mode(1),
		  m_draw_request(0),
		  m_wheel_delta(0),
		  m_blink_cursor(true),
		  m_is_active(false),
		  m_ime_on(FALSE),
		  m_bg_alpha(0xCC),
		  m_mouse_track(false),
		  m_click(false),
		  m_click_moved(false),
		  m_click_count(0),
		  m_click_cmd(MouseCmd_None),
		  m_click_time(0),
		  m_click_posx(0),
		  m_click_posy(0),
		  m_click_scroll(0),
		  m_click_dx(0),
		  m_click_dy(0) {

		trace("Window_::ctor\n");

		memset(&m_font_log, 0, sizeof(m_font_log));
		m_font_log.lfCharSet        = DEFAULT_CHARSET;
		m_font_log.lfOutPrecision   = OUT_DEFAULT_PRECIS;
		m_font_log.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
		m_font_log.lfQuality        = DEFAULT_QUALITY;
		m_font_log.lfPitchAndFamily = FIXED_PITCH | FF_DONTCARE;

		memset(&m_border, 0, sizeof(m_border));
		memset(&m_vscrl, 0, sizeof(m_vscrl));
		memset(&m_colors, 0, sizeof(m_colors));

		try{
			static const BYTE  table[]={0x00,0x5F,0x87,0xAF,0xD7,0xFE};
			static const DWORD color16[]={
				0x000000,0xCD0000,0x00CD00,0xCDCD00, 0x0000CD,0xCD00CD,0x00CDCD,0xE5E5E5,
				0x4D4D4D,0xFF0000,0x00FF00,0xFFFF00, 0x0000FF,0xFF00FF,0x00FFFF,0xFFFFFF,
			};
			int r,g,b, i=0;
			// 16 colors:: 0-15
			for(g=0; g<16; g++, i++){
				m_colors[i] = color16[g];
			}
			// 216 colors:: 16-231
			for(r=0; r<6; r++){
				for(g=0; g<6; g++){
					for(b=0; b<6; b++, i++){
						m_colors[i] = (table[r]<<16) | (table[g]<<8) | (table[b]);
					}
				}
			}
			// 24 gray colors:: 232-255
			for(g=8; g<248; g+=10,i++){
				m_colors[i] = (g<<16) | (g<<8) | (g);
			}
			// sys colors:: 256-261
			m_colors[CID_FG]        = 0xFF000000;
			m_colors[CID_BG]        = 0xDDFFFFFF;
			m_colors[CID_Select]    = 0x663297FD;
			m_colors[CID_Cursor]    = 0xFF00AA00;
			m_colors[CID_ImeCursor] = 0xFFAA0000;
			m_colors[CID_Scrollbar] = 0xFF888888;

			static bool regist = false;
			LPCWSTR classname = L"ckWindowClass";
			if(!regist){
				regist = true;
				WNDCLASSEX wc;
				memset(&wc, 0, sizeof(wc));
				wc.cbSize = sizeof(wc);
				wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
				wc.lpfnWndProc = wndproc;
				wc.hInstance = g_this_module;
				wc.hCursor = LoadCursor(0, IDC_ARROW);
				wc.hIcon = wc.hIconSm = LoadIcon(g_this_module, MAKEINTRESOURCE(1));
				wc.lpszClassName = classname;
				if(!RegisterClassEx(&wc))
					throw (HRESULT) E_FAIL;
			}
			HWND hwnd = CreateWindowEx( WS_EX_NOREDIRECTIONBITMAP, classname, 0, WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT,CW_USEDEFAULT,0,0, 0,0,g_this_module,this);
			if(!hwnd) throw (HRESULT) E_FAIL;
		}
		catch(...){
			_finalize();
			throw;
		}
	}

	STDMETHOD(get_Handle)(LONGLONG* p){
		*p = (LONGLONG) m_hwnd;
		return S_OK;
	}
	STDMETHOD(get_Pty)(VARIANT* p){
		if(m_pty){
			m_pty->AddRef();
			p->pdispVal = m_pty;
			p->vt = VT_DISPATCH;
		}
		else{
			p->vt = VT_EMPTY;
		}
		return S_OK;
	}
	STDMETHOD(put_Pty)(VARIANT var){
		if(m_pty){
			if(m_snapshot){
				m_pty->_del_snapshot(m_snapshot);
				m_snapshot = 0;
			}
			m_pty->Release();
			m_pty = 0;
		}
		if(var.vt == VT_DISPATCH && var.pdispVal){
			var.pdispVal->QueryInterface(__uuidof(IPty), (void**)&m_pty);
		}
		if(m_pty){
			if(m_is_active){
				m_pty->Resize(m_char_width, m_char_height);
			}
			m_notify->OnTitleInit();
		}
		draw_screen();
		return S_OK;
	}
	STDMETHOD(put_Title)(BSTR bs){ SetWindowText(m_hwnd, bs); return S_OK;}
	STDMETHOD(get_MouseLBtn)(MouseCmd* p){ *p = m_lbtn_cmd; return S_OK;}
	STDMETHOD(get_MouseMBtn)(MouseCmd* p){ *p = m_mbtn_cmd; return S_OK;}
	STDMETHOD(get_MouseRBtn)(MouseCmd* p){ *p = m_rbtn_cmd; return S_OK;}
	STDMETHOD(put_MouseLBtn)(MouseCmd n) { m_lbtn_cmd=n; return S_OK;}
	STDMETHOD(put_MouseMBtn)(MouseCmd n) { m_mbtn_cmd=n; return S_OK;}
	STDMETHOD(put_MouseRBtn)(MouseCmd n) { m_rbtn_cmd=n; return S_OK;}
	STDMETHOD(get_Color)(int i, DWORD* p){
		*p = (i < _countof(m_colors)) ? m_colors[i] : 0;
		return S_OK;
	}
	STDMETHOD(put_Color)(int i, DWORD color){
		if(i < _countof(m_colors)){
			if(i==CID_FG){
				color |= 0xFF000000;
			}
			if(i==CID_BG){
				;
			}
			if(i==CID_Select){
				if((color>>24) == 0){
					color |= 0x66000000;
				}
			}
			if(i==CID_Cursor || i==CID_ImeCursor || i==CID_Scrollbar){
				if((color>>24) == 0){
					color |= 0xFF000000;
				}
			}

			m_colors[i] = color;
			m_draw_request = 2;

			if(i==CID_BG){
				m_graphics->SetBackgroundColor(color);
			}
			if(i==CID_Scrollbar){
				m_graphics->SetScrollbarColor(color);
			}
		}
		return S_OK;
	}
	STDMETHOD(get_ColorAlpha)(BYTE* p){
		*p = m_bg_alpha;
		return S_OK;
	}
	STDMETHOD(put_ColorAlpha)(BYTE n){
		m_bg_alpha = n;
		m_draw_request = 2;
		return S_OK;
	}
	STDMETHOD(get_BackgroundImage)(BSTR* pp){
		BSTR path = m_graphics->GetBackgroundFile();
		*pp = (path) ? SysAllocStringLen(path, SysStringLen(path)) : 0;
		return S_OK;
	}
	STDMETHOD(put_BackgroundImage)(BSTR cygpath){
		m_graphics->SetBackgroundFile(cygpath);
		m_draw_request = 2;
		return S_OK;
	}
	STDMETHOD(get_BackgroundPlace)(DWORD* p){
		*p = m_graphics->GetBackgroundPlace();
		return S_OK;
	}
	STDMETHOD(put_BackgroundPlace)(DWORD n){
		m_graphics->SetBackgroundPlace(n);
		m_draw_request = 2;
		return S_OK;
	}
	STDMETHOD(get_FontName)(BSTR* pp){
		int len = (int)wcslen(m_font_log.lfFaceName);
		if(len<0)len=0;
		*pp = SysAllocStringLen(m_font_log.lfFaceName, len);
		return S_OK;
	}
	STDMETHOD(put_FontName)(BSTR p){ _setup_font(p, m_font_size); return S_OK;}
	STDMETHOD(get_FontSize)(float* p){ *p = m_font_size; return S_OK;}
	STDMETHOD(put_FontSize)(float n){ _setup_font(nullptr, n); return S_OK;}
	STDMETHOD(get_Linespace)(BYTE* p){ *p = (BYTE)m_linespace; return S_OK;}
	STDMETHOD(put_Linespace)(BYTE n){
		if(m_linespace != n){
			m_linespace = n;
			m_draw_request = 2;
			Resize(m_char_width, m_char_height);
		}
		return S_OK;
	}
	STDMETHOD(get_Border)(DWORD* p){
		*p = (m_border.left<<24) | (m_border.top<<16) | (m_border.right<<8) | (m_border.bottom);
		return S_OK;
	}
	STDMETHOD(put_Border)(DWORD n){
		m_border.left   = (n>>24) & 0xff;
		m_border.top    = (n>>16) & 0xff;
		m_border.right  = (n>>8 ) & 0xff;
		m_border.bottom = (n>>0 ) & 0xff;
		m_draw_request = 2;
		return S_OK;
	}
	STDMETHOD(get_Scrollbar)(int* p){ *p = m_vscrl_mode; return S_OK;}
	STDMETHOD(put_Scrollbar)(int n){
		if(m_vscrl_mode != n){
			m_vscrl_mode = n;
			m_draw_request = 2;
			Resize(m_char_width, m_char_height);
		}
		return S_OK;
	}
	STDMETHOD(get_ZOrder)(WinZOrder* p){ *p = m_zorder; return S_OK;}
	STDMETHOD(put_ZOrder)(WinZOrder n){
		if(m_zorder != n){
			m_zorder = n;
			HWND after;
			switch(m_zorder){
			case WinZOrder_Top:   after=HWND_TOPMOST;break;
			case WinZOrder_Bottom:after=HWND_BOTTOM;break;
			default:              after=HWND_NOTOPMOST;break;
			}
			SetWindowPos(m_hwnd, after,    0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
			SetWindowPos(m_hwnd, HWND_TOP, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
		}
		return S_OK;
	}
	STDMETHOD(get_BlinkCursor)(VARIANT_BOOL* p){ *p = m_blink_cursor; return S_OK;}
	STDMETHOD(put_BlinkCursor)(VARIANT_BOOL vb){
		bool b = vb != 0;
		if(m_blink_cursor != b){
			m_blink_cursor = b;
			m_draw_request = 1;
			m_graphics->UpdateCursorBlink(m_is_active & m_blink_cursor);
		}
		return S_OK;
	}
	STDMETHOD(get_ImeState)(VARIANT_BOOL* p){ *p = m_ime_on; return S_OK;}
	STDMETHOD(put_ImeState)(VARIANT_BOOL b){
		m_ime_on = b;
		ime_set_state();
		return S_OK;
	}
	STDMETHOD(Close)(){ PostMessage(m_hwnd, WM_CLOSE, 0,0); return S_OK;}
	STDMETHOD(Show)(){ Util::show_window(m_hwnd, SW_SHOW); return S_OK;}
	STDMETHOD(Resize)(int cw, int ch){
		if(cw < 1) cw=1;
		if(ch < 1) ch=1;
		SIZE frame = get_window_frame_size(m_hwnd);
		int  vscrl = (m_vscrl_mode) ? m_vscrl_size : 0;
		cw = frame.cx + (m_border.left + m_border.right + vscrl) + (cw * (m_font_width  + 0));
		ch = frame.cy + (m_border.top + m_border.bottom        ) + (ch * (m_font_height + m_linespace));
		SetWindowPos(m_hwnd,0, 0,0,cw,ch, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
		return S_OK;
	}
	STDMETHOD(Move)(int ax, int ay){
		RECT   workarea = {};
		RECT   dwm = {};
		RECT   gdi = {};
		DWORD  style = GetWindowLong(m_hwnd, GWL_STYLE);

		SystemParametersInfo(SPI_GETWORKAREA,0,(LPVOID)&workarea,0);

		GetWindowRect(m_hwnd, &gdi);

		if(!(style & WS_VISIBLE)) SetWindowLong(m_hwnd, GWL_STYLE, style | WS_VISIBLE);

		DwmGetWindowAttribute(m_hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &dwm, sizeof(dwm));

		if(!(style & WS_VISIBLE)) SetWindowLong(m_hwnd, GWL_STYLE, style);

		dwm.left   = gdi.left   - dwm.left;
		dwm.top    = gdi.top    - dwm.top;
		dwm.right  = gdi.right  - dwm.right;
		dwm.bottom = gdi.bottom - dwm.bottom;

		int  width  = gdi.right  - gdi.left;
		int  height = gdi.bottom - gdi.top;
		int  posx = (ax<0) ? (workarea.right -width +ax+1+dwm.right ) : (workarea.left+ax+dwm.left);
		int  posy = (ay<0) ? (workarea.bottom-height+ay+1+dwm.bottom) : (workarea.top +ay+dwm.top );

		SetWindowPos(m_hwnd,0, posx, posy, 0, 0, SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
		return S_OK;
	}
	STDMETHOD(PopupMenu)(VARIANT_BOOL system_menu){
		if(system_menu)
			PostMessageW(m_hwnd, WM_SYSCOMMAND, SC_KEYMENU, VK_SPACE);
		else
			PostMessageW(m_hwnd, WM_CONTEXTMENU, 0,MAKELPARAM(-1,-1));
		return S_OK;
	}
};


//----------------------------------------------------------------------------


struct DrawBuffer {

	struct Rect {
		int	left, top, right, bottom;
		DWORD	color;
	};
	struct Line {
		int	left, top, right;
		DWORD	color;
	};
	struct GlyphSegment {
		IDWriteFontFace*	fontFace;
		DWORD	fontColor;
		int	glyphCount;
		int	left;
		int	top;
	};

	int	ri;
	Rect*	Rects;

	int	li;
	Line*	Lines;

	int	si;
	int	glen;
	int	gi;
	GlyphSegment*		GlyphSegments;
	UINT16*			GlyphIndices;
	float*			GlyphAdvances;
	DWRITE_GLYPH_OFFSET*	GlyphOffsets;

	//

	~DrawBuffer(){
		this->mem_free();
	}

	DrawBuffer(size_t size){
		this->mem_alloc(size);
	}

	void  mem_free(){
		delete [] (BYTE*)(this->Rects);
	}

	void  mem_alloc(size_t size){

		const size_t  nRect   = size * sizeof(Rect);
		const size_t  nLine   = size * sizeof(Line);
		const size_t  nSeg    = size * sizeof(GlyphSegment);
		const size_t  nGlyphI = size * sizeof(UINT16);
		const size_t  nGlyphA = size * sizeof(float);
		const size_t  nGlyphO = size * sizeof(DWRITE_GLYPH_OFFSET);

		BYTE*  p = new BYTE[ nRect + nLine + nSeg + nGlyphI + nGlyphA + nGlyphO ];

		this->ri = -1;
		this->Rects = (Rect*) p;
		p += nRect;

		this->li = -1;
		this->Lines = (Line*) p;
		p += nLine;

		this->si = 0;
		this->glen = 0;
		this->gi = -1;
		this->GlyphSegments = (GlyphSegment*) p;
		this->GlyphSegments[0].fontFace = nullptr;
		p += nSeg;

		this->GlyphIndices  = (UINT16*) p;
		p += nGlyphI;

		this->GlyphAdvances = (float*) p;
		p += nGlyphA;

		this->GlyphOffsets  = (DWRITE_GLYPH_OFFSET*) p;
		p += nGlyphO;
	}
};

void Window_::draw_screen(){
	if(!m_pty) return;

	Snapshot*  nowSnapshot = m_snapshot;
	Snapshot*  oldSnapshot = nullptr;
	int  drawMode = 0;

	int  screen_tick;
	m_pty->_screen_tick(&screen_tick);
	if(!m_snapshot || m_screen_tick != screen_tick){
		m_screen_tick = screen_tick;
		m_pty->_new_snapshot(&nowSnapshot);
		if(nowSnapshot){
			oldSnapshot = m_snapshot;
			m_snapshot = nowSnapshot;
			drawMode = 2;
		}
	}
	if(!nowSnapshot){
		return;
	}

	if(drawMode == 0){
		switch(m_draw_request){
		case 0: //none
		default:
			return;
		case 1: //dcomp
			break;
		case 2: //full
			drawMode = 2;
			break;
		}
	}

	if(drawMode == 2){
		HRESULT hr = m_graphics->BeginDraw();
		if(FAILED(hr)){
			drawMode = 0;
		}
	}

	const int  charWidth  = m_font_width;
	const int  lineHeight = m_font_height + m_linespace;
	const RECT  clipRect = {
		m_border.left + (m_vscrl_mode==3 ? m_vscrl_size : 0),
		m_border.top,
		m_win_width  - m_border.right - (m_vscrl_mode==1 ? m_vscrl_size : 0),
		m_win_height - m_border.bottom,
	};

	const int    basePosX  = clipRect.left;
	const int    basePosY  = clipRect.top;
	const float  baseline  = std::round(m_fontManager.GetBaseline() + (m_linespace * 0.5f));
	const float  underline = std::round(m_fontManager.GetBaseline() + (m_linespace * 0.5f) + m_fontManager.GetUnderline()) + 0.5f; //pixel center

	if(drawMode == 2){

		const Char*  nowChars = nowSnapshot->chars;
		const int    maxCW = (m_char_width  < nowSnapshot->width)  ? m_char_width  : nowSnapshot->width;
		const int    maxCH = (m_char_height < nowSnapshot->height) ? m_char_height : nowSnapshot->height;

		DrawBuffer  prim(maxCH * maxCW);

		IDWriteFontFace*   fontFace = nullptr;
		UINT16    glIndex = 0;
		float     glWidth = 0;
		CharFlag  style   = (CharFlag)0;
		DWORD     colorFG = m_colors[CID_FG];
		DWORD     colorBG = m_colors[CID_BG];

		//int64_t  t0 = perf_get();

		for(int y=0; y < maxCH; y++){
			for(int x=0; x < maxCW; x++){
				const Char&     c = nowChars[x];
				const CharFlag  s = (CharFlag)(c.flags & CharFlag_Styles);
				const bool      skipMbTrail = (x>0 && (c.flags & CharFlag_MbTrail));

				if(skipMbTrail){
					// skip multibyte trailing char
				}
				else if(style != s){
					style = s;

					int  cidFG = (style & CharFlag_FG) ? ((style>>16)&0xFF) : CID_FG;
					int  cidBG = (style & CharFlag_BG) ? ((style>>24)&0xFF) : CID_BG;
					if(style & CharFlag_Invert){
						std::swap(cidFG, cidBG);
					}

					colorFG = m_colors[CID_FG];
					colorBG = m_colors[CID_BG];
					if(cidFG != CID_FG){
						colorFG = (m_colors[cidFG] & 0x00FFFFFF) | (colorFG & 0xFF000000);
					}
					if(cidBG != CID_BG){
						DWORD  over = (m_colors[cidBG] & 0x00FFFFFF) | (m_bg_alpha<<24);
						colorBG = composite_argb(colorBG, over);
					}
					if((style & CharFlag_Select) != 0){
						colorFG = composite_argb(colorFG, m_colors[CID_Select]);
						colorBG = composite_argb(colorBG, m_colors[CID_Select]);
					}
				}


				// Background Color
				if((colorBG>>24) == 0){
				}
				else if(prim.ri >= 0 && prim.Rects[prim.ri].top == y && prim.Rects[prim.ri].right == x && prim.Rects[prim.ri].color == colorBG){
					prim.Rects[prim.ri].right = x+1;
				}
				else{
					prim.ri++;
					prim.Rects[prim.ri].left   = x;
					prim.Rects[prim.ri].right  = x+1;
					prim.Rects[prim.ri].top    = y;
					prim.Rects[prim.ri].bottom = y+1;
					prim.Rects[prim.ri].color  = colorBG;
				}


				// Underline
				if((c.flags & CharFlag_Uline) == 0){
				}
				else if(prim.li >= 0 && prim.Lines[prim.li].top == y && prim.Lines[prim.li].right == x && prim.Lines[prim.li].color == colorFG){
					prim.Lines[prim.li].right = x+1;
				}
				else{
					prim.li++;
					prim.Lines[prim.li].left  = x;
					prim.Lines[prim.li].right = x+1;
					prim.Lines[prim.li].top   = y;
					prim.Lines[prim.li].color = colorFG;
				}


				// Text
				const UINT32  unicode = c.ch;
				const float   unicodeWidth = (c.flags & (CharFlag_MbLead|CharFlag_MbTrail)) ? float(charWidth*2) : float(charWidth);

				if(skipMbTrail){
					// skip multibyte trailing char
				}
				else if(unicode <= 0x20 || unicode == 0x7F){
					// skip space
					if(prim.glen > 0){
						prim.GlyphAdvances[prim.gi] += unicodeWidth;
					}
				}
				else{
					fontFace = m_fontManager.GetGlyph(unicode, style, &glIndex, &glWidth);

					if(prim.GlyphSegments[prim.si].fontFace != fontFace || prim.GlyphSegments[prim.si].fontColor != colorFG){
						if(prim.glen > 0){
							prim.GlyphSegments[prim.si++].glyphCount = prim.glen;
							prim.glen = 0;
						}
						prim.GlyphSegments[prim.si].fontFace   = fontFace;
						prim.GlyphSegments[prim.si].fontColor  = colorFG;
						prim.GlyphSegments[prim.si].glyphCount = 0;
						prim.GlyphSegments[prim.si].left       = (x==0 && (c.flags & CharFlag_MbTrail)) ? (x-1) : (x);
						prim.GlyphSegments[prim.si].top        = y;
					}

					prim.glen++;
					prim.gi++;
					prim.GlyphIndices[prim.gi]  = glIndex;
					prim.GlyphAdvances[prim.gi] = unicodeWidth;
					prim.GlyphOffsets[prim.gi].advanceOffset  = (unicodeWidth - glWidth) * 0.5f;
					prim.GlyphOffsets[prim.gi].ascenderOffset = 0.0f;
				}
			}

			if(prim.ri >= 1){
				// rect: merge
				if(prim.Rects[prim.ri-1].left   == prim.Rects[prim.ri].left  &&
				   prim.Rects[prim.ri-1].right  == prim.Rects[prim.ri].right &&
				   prim.Rects[prim.ri-1].bottom == prim.Rects[prim.ri].top   ){
					prim.Rects[prim.ri].bottom = prim.Rects[prim.ri].bottom;
				}
			}

			if(prim.glen > 0){
				// glyph: linefeed
				prim.GlyphSegments[prim.si++].glyphCount = prim.glen;
				prim.glen = 0;
			}
			prim.GlyphSegments[prim.si].fontFace = nullptr;


			nowChars += nowSnapshot->width;
		}


		//int64_t  t1 = perf_get();


		m_graphics->PushClipRect(clipRect);

		{//draw rect
			D2D1_RECT_F   rect;

			for(int ri=0; ri <= prim.ri; ++ri){
				rect.left   = (float)(basePosX + (prim.Rects[ri].left   * charWidth));
				rect.right  = (float)(basePosX + (prim.Rects[ri].right  * charWidth));
				rect.top    = (float)(basePosY + (prim.Rects[ri].top    * lineHeight));
				rect.bottom = (float)(basePosY + (prim.Rects[ri].bottom * lineHeight));

				m_graphics->FillRect(rect, prim.Rects[ri].color);
			}
		}
		{//draw line
			D2D1_POINT_2F  point0, point1;

			for(int li=0; li <= prim.li; ++li){
				point0.x = (float)(basePosX + (prim.Lines[li].left  * charWidth));
				point1.x = (float)(basePosX + (prim.Lines[li].right * charWidth));
				point0.y = \
				point1.y = (float)(basePosY + (prim.Lines[li].top   * lineHeight)) + underline;
				m_graphics->DrawLine(point0, point1, prim.Lines[li].color);
			}
		}
		{//draw glyph
			D2D1_POINT_2F      point;
			DWRITE_GLYPH_RUN   run;
			run.isSideways = FALSE;
			run.bidiLevel = 0;

			for(int si=0, gi=0; si < prim.si; ++si){
				point.x = (float)(basePosX + (prim.GlyphSegments[si].left * charWidth));
				point.y = (float)(basePosY + (prim.GlyphSegments[si].top  * lineHeight)) + baseline;

				run.fontEmSize    = m_fontManager.GetEmSize();
				run.fontFace      = prim.GlyphSegments[si].fontFace;
				run.glyphCount    = prim.GlyphSegments[si].glyphCount;
				run.glyphIndices  = prim.GlyphIndices  + gi;
				run.glyphAdvances = prim.GlyphAdvances + gi;
				run.glyphOffsets  = prim.GlyphOffsets  + gi;
				gi += run.glyphCount;

				m_graphics->DrawGlyphRun(point, &run, prim.GlyphSegments[si].fontColor);
			}
		}

		m_graphics->PopClipRect();

		#if 0
		int64_t  t2 = perf_get();
		double   freq = perf_freq() / 1000.0;
		trace("DrawScreen Build=%.2f Draw=%.2f Total=%.2f\n", (t1-t0)/freq, (t2-t1)/freq, (t2-t0)/freq);
		#endif

		m_graphics->EndDraw();
	}

	if(oldSnapshot){
		m_pty->_del_snapshot(oldSnapshot);
	}

	// dcomp scrollbar
	if(m_draw_request == 2 ||
	   m_vscrl.nPage != nowSnapshot->height ||
	   m_vscrl.nPos  != nowSnapshot->view ||
	   m_vscrl.nMax  != nowSnapshot->nlines-1) {

		m_vscrl.nPage = nowSnapshot->height;
		m_vscrl.nPos  = nowSnapshot->view;
		m_vscrl.nMax  = nowSnapshot->nlines-1;

		m_vscrl.x0 = (m_vscrl_mode==1) ? (m_win_width-m_vscrl_size) : 0;
		m_vscrl.x1 = m_vscrl.x0 + (m_vscrl_mode ? m_vscrl_size : 0);

		m_vscrl.y0 = 0;
		m_vscrl.y1 = m_vscrl.y0 + m_vscrl_size;

		m_vscrl.y5 = m_win_height;
		m_vscrl.y4 = m_vscrl.y5 - m_vscrl_size;

		m_vscrl.y2 = m_vscrl.y5;
		m_vscrl.y3 = m_vscrl.y5;
		m_vscrl.trackScale = 1.0f;

		if((m_vscrl.y4 - m_vscrl.y1) <= 0){
			m_vscrl.y1 = m_vscrl.y4 = (m_win_height / 2);
		}
		else if((m_vscrl.y4 - m_vscrl.y1) >= m_vscrl_size &&
			m_vscrl.nMax >= m_vscrl.nPage ){

			double  range_px = double(m_vscrl.y4 - m_vscrl.y1);
			double  scale = (range_px) / double(m_vscrl.nMax);
			double  page_px = (m_vscrl.nPage-1) * scale;
			if(page_px < m_vscrl_size){
				scale = (range_px - (m_vscrl_size - page_px)) / double(m_vscrl.nMax);
				page_px  = m_vscrl_size;
			}

			double  pos_px = (m_vscrl.nPos) * scale;
			m_vscrl.y2 = m_vscrl.y1 + (int) std::round(pos_px);
			m_vscrl.y3 = m_vscrl.y2 + (int) std::round(page_px);
			m_vscrl.trackScale = scale;
		}

		m_graphics->UpdateScrollBar(m_vscrl);
	}

	// dcomp cursor
	{
		int   x = nowSnapshot->cursor_x;
		int   y = nowSnapshot->cursor_y;
		int   w = 0;
		Char  c;

		if(x >= 0){
			c = nowSnapshot->chars[nowSnapshot->width * y + x];
			if(c.flags & CharFlag_MbTrail){
				x--;
				w = 2;
			}else if(c.flags & CharFlag_MbLead){
				w = 2;
			}else{
				w = 1;
			}
		}else{
			c = Char{};
			x = 0;
			y = 0;
			w = 0;
		}

		x = basePosX + (charWidth * x);
		y = basePosY + (lineHeight * y);

		CursorInfo  info = {};
		info.width       = charWidth * w;
		info.height      = lineHeight;
		info.active      = m_is_active;
		info.cursorColor = m_ime_on ? m_colors[CID_ImeCursor] : m_colors[CID_Cursor];
		info.textColor   = m_colors[CID_BG] | 0xFF000000;
		info.baseline    = baseline;
		info.fontManager = &m_fontManager;
		info.text        = (c.ch <= 0x20 || c.ch == 0x7F) ? 0 : c.ch;
		info.flags       = (CharFlag)(c.flags & CharFlag_Bold);
		m_graphics->UpdateCursor(x, y, info);
	}
	
	m_graphics->Commit();
	m_draw_request = 0;
}



//----------------------------------------------------------------------------

class Window: public IDispatchImpl3<IWnd>, public IWindowNotify_{
protected:
	Window_*  m_obj;
	IWndNotify* const  m_notify;
	//
	void _finalize(){
		if(m_obj){ delete m_obj; m_obj=0; }
	}
	virtual ~Window(){
		trace("Window::dtor\n");
		_finalize();
	}
public:
	Window(IWndNotify* cb): m_obj(0), m_notify(cb){
		trace("Window::ctor\n");
		m_obj = new Window_(this);
	}

	//STDMETHOD_(ULONG,AddRef)(){ ULONG n=IDispatchImpl3<IWnd>::AddRef(); trace("Window::AddRef %d\n", n); return n;}
	//STDMETHOD_(ULONG,Release)(){ ULONG n=IDispatchImpl3<IWnd>::Release(); trace("Window::Release %d\n", n); return n;}

	STDMETHOD(OnClosed)(){ return m_notify->OnClosed(this);}
	STDMETHOD(OnKeyDown)(DWORD vk){ return m_notify->OnKeyDown(this, vk);}
	STDMETHOD(OnMouseWheel)(int delta){ return m_notify->OnMouseWheel(this,delta);}
	STDMETHOD(OnMenuInit)(HMENU menu){ return m_notify->OnMenuInit(this, (int*)menu);}
	STDMETHOD(OnMenuExec)(DWORD id){ return m_notify->OnMenuExec(this, id);}
	STDMETHOD(OnTitleInit)(){ return m_notify->OnTitleInit(this);}
	STDMETHOD(OnDrop)(BSTR bs, int type, DWORD key){ return m_notify->OnDrop(this,bs,type,key);}
	STDMETHOD(OnPasteClipboard)(){ return m_notify->OnPasteClipboard(this);}
	//
	STDMETHOD(Dispose)(){
		trace("Window::dispose\n");
		_finalize();
		return S_OK;
	}
	//
	STDMETHOD(get_Handle)(LONGLONG* p){ return m_obj->get_Handle(p);}
	STDMETHOD(get_Pty)(VARIANT* p){ return m_obj->get_Pty(p);}
	STDMETHOD(put_Pty)(VARIANT var){ return m_obj->put_Pty(var);}
	STDMETHOD(put_Title)(BSTR p){ return m_obj->put_Title(p);}
	STDMETHOD(get_MouseLBtn)(MouseCmd* p){ return m_obj->get_MouseLBtn(p);}
	STDMETHOD(put_MouseLBtn)(MouseCmd n) { return m_obj->put_MouseLBtn(n);}
	STDMETHOD(get_MouseMBtn)(MouseCmd* p){ return m_obj->get_MouseMBtn(p);}
	STDMETHOD(put_MouseMBtn)(MouseCmd n) { return m_obj->put_MouseMBtn(n);}
	STDMETHOD(get_MouseRBtn)(MouseCmd* p){ return m_obj->get_MouseRBtn(p);}
	STDMETHOD(put_MouseRBtn)(MouseCmd n) { return m_obj->put_MouseRBtn(n);}
	STDMETHOD(get_Color)(int i, DWORD* p){ return m_obj->get_Color(i,p);}
	STDMETHOD(put_Color)(int i, DWORD color){ return m_obj->put_Color(i,color);}
	STDMETHOD(get_ColorAlpha)(BYTE* p){ return m_obj->get_ColorAlpha(p);}
	STDMETHOD(put_ColorAlpha)(BYTE n){ return m_obj->put_ColorAlpha(n);}
	STDMETHOD(get_BackgroundImage)(BSTR* pp){ return m_obj->get_BackgroundImage(pp);}
	STDMETHOD(put_BackgroundImage)(BSTR path){ return m_obj->put_BackgroundImage(path);}
	STDMETHOD(get_BackgroundPlace)(DWORD* p){ return m_obj->get_BackgroundPlace(p);}
	STDMETHOD(put_BackgroundPlace)(DWORD n){ return m_obj->put_BackgroundPlace(n);}
	STDMETHOD(get_FontName)(BSTR* pp){ return m_obj->get_FontName(pp);}
	STDMETHOD(put_FontName)(BSTR p){ return m_obj->put_FontName(p);}
	STDMETHOD(get_FontSize)(float* p){ return m_obj->get_FontSize(p);}
	STDMETHOD(put_FontSize)(float n){ return m_obj->put_FontSize(n);}
	STDMETHOD(get_LineSpace)(BYTE* p){ return m_obj->get_Linespace(p);}
	STDMETHOD(put_LineSpace)(BYTE n){ return m_obj->put_Linespace(n);}
	STDMETHOD(get_Border)(DWORD* p){ return m_obj->get_Border(p);}
	STDMETHOD(put_Border)(DWORD n){ return m_obj->put_Border(n);}
	STDMETHOD(get_Scrollbar)(int* p){ return m_obj->get_Scrollbar(p);}
	STDMETHOD(put_Scrollbar)(int n){ return m_obj->put_Scrollbar(n);}
	STDMETHOD(get_ZOrder)(WinZOrder* p){ return m_obj->get_ZOrder(p);}
	STDMETHOD(put_ZOrder)(WinZOrder n) { return m_obj->put_ZOrder(n);}
	STDMETHOD(get_BlinkCursor)(VARIANT_BOOL* p){ return m_obj->get_BlinkCursor(p);}
	STDMETHOD(put_BlinkCursor)(VARIANT_BOOL b) { return m_obj->put_BlinkCursor(b);}
	STDMETHOD(get_ImeState)(VARIANT_BOOL* p){ return m_obj->get_ImeState(p);}
	STDMETHOD(put_ImeState)(VARIANT_BOOL b) { return m_obj->put_ImeState(b);}
	STDMETHOD(Close)(){ return m_obj->Close();}
	STDMETHOD(Show)(){ return m_obj->Show();}
	STDMETHOD(Resize)(int cw, int ch){ return m_obj->Resize(cw,ch);}
	STDMETHOD(Move)(int x, int y){ return m_obj->Move(x,y);}
	STDMETHOD(PopupMenu)(VARIANT_BOOL system_menu){ return m_obj->PopupMenu(system_menu);}
};


}//namespace Ck

//----------------------------------------------------------------------------

extern "C" __declspec(dllexport) HRESULT CreateWnd(Ck::IWndNotify* cb, Ck::IWnd** pp){
	if(!pp) return E_POINTER;
	if(!cb) return E_INVALIDARG;

	HRESULT hr;
	Ck::Window* w = 0;

	try{
		w = new Ck::Window(cb);
		w->AddRef();
		hr = S_OK;
	}
	catch(HRESULT e){
		hr = e;
	}
	catch(std::bad_alloc&){
		hr = E_OUTOFMEMORY;
	}
	catch(...){
		hr = E_FAIL;
	}

	(*pp) = w;
	return hr;
}

//EOF
