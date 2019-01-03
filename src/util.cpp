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
#include <DwmApi.h>

//#import  "interface.tlb" raw_interfaces_only
#include "interface.tlh"
#include "util.h"
#pragma comment(lib, "DwmApi.lib")

namespace cygwin{
	enum{
		CCP_POSIX_TO_WIN_A = 0,
		CCP_POSIX_TO_WIN_W,
		CCP_WIN_A_TO_POSIX,
		CCP_WIN_W_TO_POSIX,
		CCP_ABSOLUTE = 0,
		CCP_RELATIVE = 0x100
	};
	extern "C"{
		__declspec(dllimport) int   chdir(const char*);
		__declspec(dllimport) int   cygwin_conv_path(int what, const void* from, void* to, int size);
	}
}

namespace Ck{
namespace Util{

BOOL show_window(HWND hwnd, int cmd){
	static bool first = true;
	if(hwnd){
		if(first){
			first = false;
			::ShowWindow(hwnd, SW_HIDE);//skip StartupInfo.wShowWindow
		}
		BOOL rv = ::ShowWindow(hwnd, cmd);
		SetForegroundWindow(hwnd);//request
		return rv;
	}
	return FALSE;
}

static BYTE* get_resource(HMODULE module, LPCWSTR type, LPCWSTR name, DWORD* outsize){
	HRSRC rsrc = FindResource(module, name, type);
	if(rsrc){
		HGLOBAL glb  = LoadResource(module, rsrc);
		if(glb){
			*outsize = SizeofResource(module, rsrc);
			return (BYTE*) LockResource(glb);
		}
	}
	*outsize = 0;
	return 0;
}

static void _get_module_date(HMODULE module, SYSTEMTIME* p){
	FILETIME   bt = {0xD53E8000, 0x019DB1DE};//1970/01/01
	_int64     ft;
	FileTimeToLocalFileTime(&bt, (FILETIME*)&ft);//timezone

	IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*) module;
	IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*) ((BYTE*)dos + dos->e_lfanew);
	if(dos->e_magic == IMAGE_DOS_SIGNATURE && nt->Signature == IMAGE_NT_SIGNATURE){
		ft += (_int64) nt->FileHeader.TimeDateStamp * 10000000;
	}
	FileTimeToSystemTime((FILETIME*)&ft, p);
}

static void _get_module_version(HMODULE module, WORD* p){
	p[0]=p[1]=p[2]=p[3]=0;
	DWORD size;
	BYTE* rsrc = get_resource(module, RT_VERSION, MAKEINTRESOURCE(1), &size);
	if(rsrc && size > 54){
		WORD* w = (WORD*)(rsrc+40);
		if(w[0]==0x04BD && w[1]==0xFEEF){//0xFEEF04BD
			p[0]=w[5];
			p[1]=w[4];
			p[2]=w[7];
			p[3]=w[6];
		}
	}
}

BSTR get_module_version(HMODULE module, BOOL full){
	BSTR bs = SysAllocStringLen(0,128);
	if(bs){
		SYSTEMTIME st;
		WORD vs[4];
		_get_module_version(module, vs);
		_get_module_date(module, &st);
		if(full){
			StringCchPrintf(bs,128,
				L"ver%d.%d.%d.%d %04d/%02d/%02d %02d:%02d:%02d (0x%p)",
				vs[0], vs[1], vs[2], vs[3],
				st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
				module);
		}else{
			StringCchPrintf(bs,128,
				L"ver%d.%d.%d (0x%p)",
				vs[0], vs[1], vs[2], module);
		}

		((UINT*)bs)[-1] = (UINT)(sizeof(WCHAR*) * wcslen(bs));
	}
	return bs;
}



static LPWSTR _conv_encoding(BYTE* ptr, DWORD size){
	LPWSTR result;

	if(ptr[0]==0xFF && ptr[1]==0xFE){//ucs2le
		size >>= 1;
		result = new WCHAR[size+4];
		memcpy(result, ptr, size<<1);
	}
	else if(ptr[0]==0xFE && ptr[1]==0xFF){//ucs2be
		size >>= 1;
		result = new WCHAR[size+4];
		for(DWORD i=0; i<size; i++, ptr+=2)
			result[i] = (ptr[0]<<8) | (ptr[1]);
	}
	else if(ptr[0]==0xEF && ptr[1]==0xBB && ptr[2]==0xBF){//utf8
		result = new WCHAR[size+4];
		size = MultiByteToWideChar(CP_UTF8,0, (LPCSTR)ptr,size, result,size);
	}
	else{//ansi code page
		result = new WCHAR[size+4];
		size = MultiByteToWideChar(CP_ACP,0, (LPCSTR)ptr,size, result,size);
	}

	if(size<1){
		delete [] result;
		return 0;
	}
	result[size] = '\0';
	return result;
}
LPWSTR load_script_rsrc(LPCWSTR name){
	LPWSTR result = 0;
	DWORD  size = 0;
	BYTE*  ptr = get_resource(g_this_module, L"SCRIPT", name, &size);
	if(ptr && 4 <= size && size <= 0x1fffffff){
		result = _conv_encoding(ptr,size);
	}
	return result;
}
LPWSTR load_script_file(LPCWSTR path){
	LPWSTR result = 0;
	HANDLE fd = CreateFile(path, GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,0);
	if(fd != INVALID_HANDLE_VALUE){
		DWORD dw, size = GetFileSize(fd, &dw);
		if(dw==0 && 4<=size && size<=0x1fffffff){
			BYTE* ptr = new BYTE[size];
			if(ReadFile(fd,ptr,size,&dw,0) && dw==size){
				result = _conv_encoding(ptr,size);
			}
			delete [] ptr;
		}
		CloseHandle(fd);
	}
	return result;
}


static bool open_clipboard(){
	int count = 6;
	do{
		if(OpenClipboard(0))
			return true;
		Sleep(16);
	} while(--count);
	return false;
}

void set_clipboard_text(BSTR str){
	UINT len = SysStringLen(str)+1;
	if(len<=1) return;
	HANDLE mem = GlobalAlloc(GMEM_MOVEABLE, sizeof(WCHAR)*len);
	if(mem){
		void* ptr = GlobalLock(mem);
		if(ptr){
			memcpy(ptr, str, sizeof(WCHAR)*len);
			GlobalUnlock(mem);
			//
			if(open_clipboard()){
				if(EmptyClipboard()){
					if(SetClipboardData(CF_UNICODETEXT, mem)){
						CloseClipboard();
						return;
					}
				}
				CloseClipboard();
			}
		}
		GlobalFree(mem);
	}
}

BSTR get_clipboard_text(){
	BSTR result = 0;
	if(IsClipboardFormatAvailable(CF_UNICODETEXT)){
		if(open_clipboard()){
			HANDLE mem = GetClipboardData(CF_UNICODETEXT);
			if(mem){
				WCHAR* src = (WCHAR*) GlobalLock(mem);
				if(src){
					result = SysAllocString(src);
					GlobalUnlock(mem);
				}
			}
			CloseClipboard();
		}
	}
	return result;
}



static char*  bstr_to_char(const BSTR wstr){
	char* result = 0;
	int  wlen = (int) SysStringLen(wstr);
	if(wlen>0){
		int alen = wlen * 3 + 32;
		result = new char[alen];
		alen = WideCharToMultiByte(CP_UTF8,0, wstr,wlen, result,alen-1, 0,0);
		if(alen>0){
			result[alen]='\0';
		}
		else{
			delete [] result;
			result=0;
		}
	}
	return result;
}

static BSTR  char_to_bstr(const char* astr){
	BSTR result = 0;
	int alen = (int) strlen(astr);
	if(alen>0){
		int wlen = alen+32;
		result = SysAllocStringLen(0, wlen);
		if(result){
			wlen = MultiByteToWideChar(CP_UTF8,0, astr,alen, result,wlen);
			if(wlen>0){
				result[wlen]='\0';
				((UINT*)result)[-1] = wlen * sizeof(WCHAR);
			}
			else{
				SysFreeString(result);
				result=0;
			}
		}
	}
	return result;
}

void set_cygwin_env(BSTR name, BSTR value){
	char* aname  = bstr_to_char(name);
	char* avalue = bstr_to_char(value);
	if(aname){
		cyg_setenv(aname, avalue ? avalue : "", 1);
	}
	if(aname)  delete [] aname;
	if(avalue) delete [] avalue;
}

BSTR get_cygwin_env(BSTR name){
	BSTR result = 0;
	char* aname  = bstr_to_char(name);
	if(aname){
		char* avalue = cyg_getenv(aname);
		if(avalue){
			result = char_to_bstr(avalue);
		}
		delete [] aname;
	}
	return result;
}

BSTR get_cygwin_current_directory(){
	BSTR result=0;
	char buf [MAX_PATH+32];
	if(cyg_getcwd(buf, MAX_PATH+32)){
		buf[MAX_PATH+31] = '\0';
		result = char_to_bstr(buf);
	}
	return result;
}

void set_cygwin_current_directory(BSTR cygpath){
	char* apath = bstr_to_char(cygpath);
	if(apath){
		cygwin::chdir(apath);
		delete [] apath;
	}
}

BSTR  to_cygwin_path(BSTR bstr){
	BSTR result = 0;
	int  wlen = SysStringLen(bstr);
	if(wlen>0){
		int   alen = wlen * 6 + 32;
		char* abuf = new (std::nothrow) char[alen];
		if(abuf){
			if( cygwin::cygwin_conv_path( cygwin::CCP_WIN_W_TO_POSIX, bstr, abuf, sizeof(char)*alen ) == 0 ){
				result = char_to_bstr(abuf);
			}
			delete [] abuf;
		}
	}
	if(!result){
		result = SysAllocString(bstr);
	}
	return result;
}

BSTR  to_windows_path(BSTR bstr){
	BSTR result = 0;
	char*  abuf = bstr_to_char(bstr);
	if(abuf){
		int alen = (int) strlen(abuf);
		if(alen>0){
			int   wlen = alen * 6 + 32;
			BSTR  wbuf = SysAllocStringLen(0, wlen);
			if(wbuf){
				if( cygwin::cygwin_conv_path( cygwin::CCP_POSIX_TO_WIN_W, abuf, wbuf, sizeof(WCHAR)*wlen ) == 0 ){
					wlen = (int) wcslen(wbuf);
					((UINT*)wbuf)[-1] = wlen * sizeof(WCHAR);
					result = wbuf;
				}else{
					SysFreeString(wbuf);
				}
			}
		}
		delete [] abuf;
	}
	if(!result){
		result = SysAllocString(bstr);
	}
	return result;
}

BSTR  ucs4_to_bstr(UINT32* wbuf, UINT wlen){
	BSTR  result = SysAllocStringLen(0, wlen*2+32);
	if(result){
		WCHAR*  dst = result;
		for(UINT32 i=0; i < wlen; ++i){
			if(wbuf[i] <= 0xFFFF){
				*dst++ = (WCHAR) wbuf[i];
			}
			else if(0x10000 <= wbuf[i] && wbuf[i] <= 0x10FFFF){
				//surrogate pair
				*dst++ = (WCHAR)( (wbuf[i]-0x10000) / 0x400 + 0xD800 );
				*dst++ = (WCHAR)( (wbuf[i]-0x10000) % 0x400 + 0xDC00 );
			}
			else{
			}
		}
		*dst = '\0';
		((UINT*)result)[-1] = (UINT)( (BYTE*)dst - (BYTE*)result );
	}
	return result;
}

UINT  ucs4len(UINT32* wbuf){
	UINT  len = 0;
	for( ; *wbuf; ++len, ++wbuf);
	return len;
}

void  set_window_composition_grass_filter(HWND hwnd){
	if(hwnd){

		struct AccentPolicy { DWORD state; LONG flags, gradient, animationid; };
		struct WINCOMPATTRDATA { DWORD attribute; PVOID pData; ULONG dataSize; };
		typedef BOOL (WINAPI * tSetAttribute)(HWND, WINCOMPATTRDATA*);

		static tSetAttribute  pSetAttribute = 0;

		if(!pSetAttribute){
			HMODULE  dll = GetModuleHandleW(L"user32.dll");
			pSetAttribute = (tSetAttribute) GetProcAddress(dll, "SetWindowCompositionAttribute");
		}

		if(pSetAttribute){
			// ACCENT_ENABLE_BLURBEHIND = 3
			AccentPolicy  policy = { 3, 0, 0, 0 };
			// ACCENT_POLICY = 19
			WINCOMPATTRDATA  params = { 19, &policy, sizeof(AccentPolicy) };
			(*pSetAttribute)(hwnd, &params);
		}
	}
}


}//namespace Util
}//namespace Ck
//EOF
