/*----------------------------------------------------------------------------
 * Copyright 2004-2018  Kazuo Ishii <kish@wb3.so-net.ne.jp>
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
#include <process.h>
#include <winternl.h>
#include <string>
#include <list>

//#import  "interface.tlb" raw_interfaces_only
#include "interface.tlh"
#include "util.h"
#include "encoding.h"
#include "screen.h"

#pragma comment(linker, "/nodefaultlib:OLDNAMES.lib") //chdir(),read()
#pragma comment(lib, "cygwin1_app.lib")

namespace cygwin{
	enum{
		SIGHUP  = 1,
		SIGINT  = 2,
		WNOHANG = 1,
		TIOCSWINSZ = ('T'<<8)|2,
		CW_CYGWIN_PID_TO_WINPID = 18,
	};
	struct winsize{
		unsigned short	ws_row, ws_col, ws_xpixel, ws_ypixel;
	};
	struct pollfd{
		int    fd;
		short  events, revents;
	};
	enum{
		POLLIN  = 1,
		POLLHUP = 16,
	};
	extern "C"{
		__declspec(dllimport) int  close(int);
		__declspec(dllimport) unsigned long  cygwin_internal(int, ...);
		__declspec(dllimport) int  ioctl(int fd, int cmd, ...);
		__declspec(dllimport) int  kill(int,int);
		__declspec(dllimport) int  read(int,void*,int);
		__declspec(dllimport) int  poll(struct pollfd*, unsigned int, int);
		__declspec(dllimport) int  waitpid(int,int*,int);
		__declspec(dllimport) int  write(int,void*,int);
	}
}


namespace Ck{
namespace Util{
#pragma comment(lib, "ntdll.lib")

#ifdef _M_IX86
#pragma pack(push,4)
typedef struct{
	ULONGLONG	Reserved1;
	ULONGLONG	PebBaseAddress;
	ULONGLONG	Reserved2[2];
	ULONGLONG	UniqueProcessId;
	ULONGLONG	Reserved3;
} PROCESS_BASIC_INFORMATION64;
#pragma pack(pop)

typedef NTSTATUS (NTAPI * tNtWow64QueryInformationProcess64)(HANDLE, PROCESSINFOCLASS, void*, ULONG, PULONG);
typedef NTSTATUS (NTAPI * tNtWow64ReadVirtualMemory64)(HANDLE, ULONGLONG, PVOID, ULONGLONG, PULONGLONG);

static HMODULE  _ntdll =0;
static tNtWow64QueryInformationProcess64  pNtWow64QueryInformationProcess64 =0;
static tNtWow64ReadVirtualMemory64        pNtWow64ReadVirtualMemory64 =0;

static bool  ReadMemory64(HANDLE process, ULONGLONG address, void* buffer, DWORD size){
	ULONGLONG  n;
	NTSTATUS  rv = pNtWow64ReadVirtualMemory64(process, address, buffer, size, &n);
	return (rv == 0 && n == size);
}
#endif

static bool  ReadMemory(HANDLE process, void* address, void* buffer, DWORD size){
	SIZE_T  n;
	BOOL  rv = ReadProcessMemory(process, address, buffer, size, &n);
	return (rv && n == size);
}

static BSTR  get_child_cwd(HANDLE process){
	BSTR		result = 0;
	NTSTATUS	rv;
	ULONG		n =0;
	ULONG_PTR	peb32 =0;
	BYTE		tmp[16];
	WCHAR		path[MAX_PATH+8];

	rv = NtQueryInformationProcess(process, ProcessWow64Information, &peb32, sizeof(peb32), &n);
	if(rv >= 0 && n >= sizeof(peb32)){

		#ifdef _M_AMD64
		PROCESS_BASIC_INFORMATION  info;
		rv = NtQueryInformationProcess(process, ProcessBasicInformation, &info, sizeof(info), &n);
		if(rv>=0 && n >= sizeof(info)){
			//trace("Native PEB=%p\n", info.PebBaseAddress);
			//trace("Wow64  PEB=%p\n", peb32);
			if(peb32){
				if(ReadMemory(process, ((BYTE*)peb32)+0x10, tmp, 4)){
					BYTE*  pProcessParameters = (BYTE*) (intptr_t) (*(DWORD*)(tmp));
					if(ReadMemory(process, pProcessParameters+0x24, tmp, 8)){
						USHORT  wlen = (USHORT) (*(USHORT*)(tmp+0)) / sizeof(WCHAR);
						void*   wstr = (void*) (intptr_t) (*(DWORD*)(tmp+4));
						if(wstr && 1 <= wlen && wlen < MAX_PATH){
							if(ReadMemory(process, wstr, result, sizeof(WCHAR)*wlen)){
								result = SysAllocStringLen((const OLECHAR*)path, wlen);
							}
						}
					}
				}
			}else{
				if(ReadMemory(process, ((BYTE*)info.PebBaseAddress)+0x20, tmp, 8)){
					BYTE*  pProcessParameters = *(BYTE**)(tmp);
					if(ReadMemory(process, pProcessParameters+0x38, tmp, 16)){
						USHORT  wlen = (USHORT) (*(USHORT*)(tmp+0)) / sizeof(WCHAR);
						void*   wstr = (void*) (intptr_t) (*(ULONGLONG*)(tmp+8));
						if(wstr && 1 <= wlen && wlen < MAX_PATH){
							if(ReadMemory(process, wstr, path, sizeof(WCHAR)*wlen)){
								result = SysAllocStringLen((const OLECHAR*)path, wlen);
							}
						}
					}
				}
			}
		}
		#endif

		#ifdef _M_IX86
		BOOL  _32bit = TRUE;
		if(!peb32){
			if(!_ntdll){
				_ntdll = GetModuleHandleW(L"ntdll.dll");
				if(_ntdll){
					pNtWow64QueryInformationProcess64 = (tNtWow64QueryInformationProcess64) GetProcAddress(_ntdll, "NtWow64QueryInformationProcess64");
					pNtWow64ReadVirtualMemory64       = (tNtWow64ReadVirtualMemory64)       GetProcAddress(_ntdll, "NtWow64ReadVirtualMemory64");
				}
			}
			if(pNtWow64QueryInformationProcess64 && pNtWow64ReadVirtualMemory64){
				_32bit = FALSE;
			}
		}
		if(_32bit){
			PROCESS_BASIC_INFORMATION  info;
			rv = NtQueryInformationProcess(process, ProcessBasicInformation, &info, sizeof(info), &n);
			if(rv >= 0 && n >= sizeof(info)){
				if(ReadMemory(process, ((BYTE*)info.PebBaseAddress)+0x10, tmp, 4)){
					BYTE*  pProcessParameters = (BYTE*)( *(DWORD*)(tmp) );
					if(ReadMemory(process, pProcessParameters+0x24, tmp, 8)){
						USHORT  wlen = (USHORT )( *(USHORT* )(tmp+0) ) / sizeof(WCHAR);
						void*   wstr = (void*  )( *(DWORD*  )(tmp+4) );
						if(wstr && 1 <= wlen && wlen < MAX_PATH){
							if(ReadMemory(process, wstr, path, sizeof(WCHAR)*wlen)){
								result = SysAllocStringLen((const OLECHAR*)path, wlen);
							}
						}
					}
				}
			}
		}else{
			PROCESS_BASIC_INFORMATION64  info;
			rv = pNtWow64QueryInformationProcess64(process, ProcessBasicInformation, &info, sizeof(info), &n);
			if(rv>=0 && n >= sizeof(info)){
				if(ReadMemory64(process, (info.PebBaseAddress)+0x20, tmp, 8)){
					ULONGLONG  pProcessParameters = *(ULONGLONG*)(tmp);
					if(ReadMemory64(process, pProcessParameters+0x38, tmp, 16)){
						USHORT     wlen = (USHORT   )( *(USHORT*   )(tmp+0) ) / sizeof(WCHAR);
						ULONGLONG  wstr = (ULONGLONG)( *(ULONGLONG*)(tmp+8) );
						if(wstr && 1 <= wlen && wlen < MAX_PATH){
							if(ReadMemory64(process, wstr, path, sizeof(WCHAR)*wlen)){
								result = SysAllocStringLen((const OLECHAR*)path, wlen);
							}
						}
					}
				}
			}
		}
		#endif
	}

	return result;
}

static BSTR get_child_current_directory(int cyg_pid){
	BSTR result = 0;
	int win_pid = cygwin_internal(cygwin::CW_CYGWIN_PID_TO_WINPID, cyg_pid);
	//trace("cygpid=%d, winpid=%d\n", cyg_pid, win_pid);
	if(win_pid >= 0){
		HANDLE process = OpenProcess(
			PROCESS_CREATE_THREAD| PROCESS_QUERY_INFORMATION| PROCESS_VM_OPERATION| PROCESS_VM_READ| PROCESS_VM_WRITE,
			FALSE, win_pid);
		if(process){
			BSTR win_path = get_child_cwd(process);
			if(win_path){
				result = Util::to_cygwin_path(win_path);
				SysFreeString(win_path);
			}
			CloseHandle(process);
		}
	}
	return result;
}

}//namespace Util
}//namespace Ck


namespace Ck{

template<class T> class WrQueue{
private:
	T*  m_buf;
	int m_size;
	int m_max;
	int m_wincx;
	int m_wincy;
	CRITICAL_SECTION m_cs;

	void _finalize(){
		if(m_buf) free(m_buf);
		DeleteCriticalSection(&m_cs);
	}
public:
	~WrQueue<T>(){ _finalize();}
	WrQueue<T>(): m_buf(0),m_size(0),m_max(0),m_wincx(0),m_wincy(0){
		InitializeCriticalSection(&m_cs);
		try{
			m_max = 256;
			m_buf = (T*) malloc(sizeof(T) * m_max);
			if(!m_buf) throw std::bad_alloc();
		}
		catch(...){
			_finalize();
		}
	}
	void Lock()  { EnterCriticalSection(&m_cs);}
	void Unlock(){ LeaveCriticalSection(&m_cs);}
	void SetWinSize(int x, int y){
		Lock();
		m_wincx=x; m_wincy=y;
		Unlock();
	}
	void Clear(){
		Lock();
		m_size = 0;
		Unlock();
	}
	void Push(T* p, int n){
		if(n<1) return;
		Lock();
		if(m_size+n > m_max){
			T* tmp = (T*) realloc(m_buf, sizeof(T) * (m_size+n));
			if(!tmp){ Unlock(); throw std::bad_alloc(); }
			m_buf = tmp;
			m_max = m_size+n;
		}
		memcpy(m_buf+m_size, p, sizeof(T)*n);
		m_size += n;
		Unlock();
	}
	//
	void GetWinSize(int* x, int* y){
		*x=m_wincx; *y=m_wincy;
	}
	void Pop(int n){
		if(n>0){
			if(n >= m_size)
				m_size = 0;
			else{
				memmove(m_buf, m_buf+n, sizeof(T)*(m_size-n));
				m_size -= n;
			}
		}
	}
	int Peek(T* p, int n){
		if(n>m_size) n=m_size;
		if(n>0) memcpy(p, m_buf, sizeof(T)*n);
		return n;
	}
};

static const char* ANSWERBACK_MESSAGE = "";
static const char* FIRMWARE_VERSION   = "\x1B[>0;0036;0c"; // xterm;
static const char* DEVICE_ATTRIBUTE   = "\x1B[?1;2c";
static const size_t  TITLE_STACK_MAX = 64;

//----------------------------------------------------------------------------

typedef enum{
	UpdateMask_Title = 0x01,
	UpdateMask_Font  = 0x02,
	UpdateMask_Pos   = 0x04,
	UpdateMask_Size  = 0x08,
} UpdateMask;

typedef struct{
	UpdateMask mask;
	int	font;
	int	posx;
	int	posy;
	int	sizex;
	int	sizey;
} UpdateStore;

class IPtyNotify_{
public:
	STDMETHOD(OnClosed)() = 0;
	STDMETHOD(OnUpdateTitle)() = 0;
	STDMETHOD(OnReqFont)(int id) = 0;
	STDMETHOD(OnReqMove)(int x, int y) = 0;
	STDMETHOD(OnReqResize)(int x, int y) = 0;
};

//----------------------------------------------------------------------------

class Pty_{
private:
	int  m_pid;
	int  m_fd;
	IPtyNotify_* const  m_notify;
	HANDLE	m_rd_thread;
	HANDLE	m_wr_thread;
	HANDLE	m_wr_event;
	WrQueue<BYTE> m_wr_buf;
	Screen   m_screen;
	int      m_screen_update;
	PrivMode m_priv;
	Encoding m_input_encoding;
	Encoding m_display_encoding;
	int  m_jis_mode;
	std::wstring	m_title;
	std::list<std::wstring>	m_titleStack;
protected:
	static UINT CALLBACK read_proc(LPVOID lp);
	static UINT CALLBACK write_proc(LPVOID lp);
	void _process_sequence_window_ops(int n, int arg[], UpdateStore& upp);
	void _process_sequence_sgr(int n, int arg[], UpdateStore& upp);
	void _process_sequence_term_mode(char priv, int n, int arg[], UpdateStore& upp);
	void _process_sequence_ascii(char ch, UpdateStore& upp);
	bool _process_sequence_csi(UINT32* input, int& idx, int max, UpdateStore& upp);
	bool _process_sequence_osc(UINT32* input, int& idx, int max, UpdateStore& upp);
	bool _process_sequence_dcs(UINT32* input, int& idx, int max, UpdateStore& upp);
	bool _process_sequence_esc(UINT32* input, int& idx, int max, UpdateStore& upp);
	bool _process_sequence_vt52(UINT32* input, int& idx, int max, UpdateStore& upp);
	bool _process_sequence(UINT32* input, int& idx, int max, UpdateStore& upp);
	bool _get_priv(PrivMode mask){ return (m_priv & mask)? true: false;}
	bool _set_priv(char mode, PrivMode mask){
		if(mode=='t') mode = (m_priv & mask) ? 'l' : 'h';
		if(mode=='h' || mode=='s'){ m_priv=(PrivMode)(m_priv | mask); return true;}
		if(mode=='l' || mode=='r'){ m_priv=(PrivMode)(m_priv & ~mask); return false;}
		return _get_priv(mask);
	}
	void _reset(bool full){
		if(full) m_wr_buf.Clear();
		m_priv = (PrivMode)(Priv_VisibleCursor | Priv_ScrollTtyKey | Priv_AutoWrap | Priv_CjkWidth);
		m_jis_mode = 0;
		m_screen_update++;
		m_screen.Fore().Reset(full);
		m_screen.Back().Reset(full);
		m_screen.Set(false);
	}
	void put_char(char ch){
		m_wr_buf.Push((BYTE*)&ch, 1);
		SetEvent(m_wr_event);
	}
	void put_string(const char* str){
		int len = (int)strlen(str);
		if(len>0){
			m_wr_buf.Push((BYTE*)str, len);
			SetEvent(m_wr_event);
		}
	}
	void put_vstring(const char* fmt, ...){
		char buf[128];
		va_list va;
		va_start(va,fmt);
		size_t n=128;
		if(SUCCEEDED(StringCchVPrintfExA(buf, 128, NULL, &n, 0, fmt, va))){
			m_wr_buf.Push((BYTE*)buf, (int)(128-n));
			SetEvent(m_wr_event);
		}
		va_end(va);
	}

	void _finalize();
public:
	~Pty_(){ _finalize();}
	Pty_(IPtyNotify_* cb, BSTR cmdline);
	//
	STDMETHOD(get_PageWidth)(int* p){ *p = m_screen.Current().GetPageWidth(); return S_OK;}
	STDMETHOD(get_PageHeight)(int* p){ *p = m_screen.Current().GetPageHeight(); return S_OK;}
	STDMETHOD(get_CursorPosX)(int* p){ *p = m_screen.Current().GetCurX(); return S_OK;}
	STDMETHOD(get_CursorPosY)(int* p){ *p = m_screen.Current().GetCurY(); return S_OK;}
	//
	STDMETHOD(get_Savelines)(int* p){ *p = m_screen.Fore().GetSavelines(); return S_OK;}
	STDMETHOD(put_Savelines)(int n){
		m_screen.Lock();
		m_screen.Fore().SetSavelines(n);
		m_screen_update++;
		m_screen.Unlock();
		return S_OK;
	}
	STDMETHOD(get_ViewPos)(int* p){ *p = m_screen.Current().GetViewPos(); return S_OK;}
	STDMETHOD(put_ViewPos)(int n){
		m_screen.Lock();

		int bak = m_screen.Current().GetViewPos();
		m_screen.Current().SetViewPos(n);
		int now = m_screen.Current().GetViewPos();
		if(bak != now) m_screen_update++;

		m_screen.Unlock();
		return S_OK;
	}
	STDMETHOD(get_InputEncoding)(Encoding* p){ *p = m_input_encoding; return S_OK;}
	STDMETHOD(put_InputEncoding)(Encoding n){ m_input_encoding = n; return S_OK;}
	STDMETHOD(get_DisplayEncoding)(Encoding* p){ *p = m_display_encoding; return S_OK;}
	STDMETHOD(put_DisplayEncoding)(Encoding n){ m_display_encoding = n; return S_OK;}
	STDMETHOD(get_PrivMode)(PrivMode* p){ *p = m_priv; return S_OK;}
	STDMETHOD(put_PrivMode)(PrivMode n){ m_screen.Lock(); m_priv=n; m_screen.Unlock(); return S_OK;}
	STDMETHOD(get_Title)(BSTR* pp){
		m_screen.Lock();
		*pp = SysAllocStringLen(&m_title[0], (UINT)m_title.size());
		m_screen.Unlock();
		return S_OK;
	}
	STDMETHOD(put_Title)(BSTR p){
		m_screen.Lock();
		if(p){
			m_title = p;
		}else{
			m_title.clear();
		}
		m_screen.Unlock();
		return S_OK;
	}
	STDMETHOD(get_CurrentDirectory)(BSTR* pp){
		*pp = Ck::Util::get_child_current_directory(m_pid);
		return S_OK;
	}
	STDMETHOD(_screen_tick)(int* p){
		m_screen.Lock();
		*p = m_screen_update;
		m_screen.Unlock();
		return S_OK;
	}
	//
	STDMETHOD(_new_snapshot)(Snapshot** pp);
	STDMETHOD(Resize)(int w, int h);
	STDMETHOD(Reset)(VARIANT_BOOL full){
		m_screen.Lock();
		_reset(full ? true : false);
		m_screen.Unlock();
		return S_OK;
	}
	STDMETHOD(PutKeyboard)(ModKey key);
	STDMETHOD(PutMouse)(int x, int y, ModKey key, int nclicks, VARIANT_BOOL* handled);
	STDMETHOD(PutString)(BSTR str);
	STDMETHOD(SetSelection)(int x, int y, int mode){
		m_screen.Lock();
		m_screen.Current().SetSelection(x, y, mode);
		m_screen_update++;
		m_screen.Unlock();
		return S_OK;
	}
	STDMETHOD(get_SelectedString)(BSTR* pp){
		m_screen.Lock();
		*pp = m_screen.Current().GetSelection();
		m_screen.Unlock();
		return S_OK;
	}
};

//----------------------------------------------------------------------------

STDMETHODIMP  Pty_::_new_snapshot(Snapshot** pp){
	*pp = 0;
	m_screen.Lock();
	try{
		*pp = m_screen.Current().GetSnapshot(_get_priv(Priv_ReverseVideo), _get_priv(Priv_VisibleCursor));
	}
	catch(...){
	}
	m_screen.Unlock();
	return S_OK;
}

STDMETHODIMP  Pty_::Resize(int w, int h){
	if(m_screen.Fore().GetPageWidth() != w || m_screen.Fore().GetPageHeight() != h){
		m_screen.Lock();
		m_screen.Fore().Resize(w,h);
		m_screen.Back().Resize(w,h);
		m_screen_update++;
		m_screen.Unlock();

		m_wr_buf.SetWinSize(w,h);
		SetEvent(m_wr_event);
	}
	return S_OK;
}

STDMETHODIMP  Pty_::PutKeyboard(ModKey key){
	DWORD vk = (DWORD)(key & ModKey_Key);

	if(key & ModKey_Shift) {
		if(VK_F1 <= vk && vk <= VK_F10) {
			vk += VK_F11 - VK_F1;
			key = (ModKey)(key & ~ModKey_Shift);
		}
	}

	int	number = 0;
	char	cursor = 0;

	switch(vk) {
	case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
	case VK_SHIFT:   case VK_LSHIFT:   case VK_RSHIFT:
	case VK_MENU:    case VK_LMENU:    case VK_RMENU:
		return S_OK;

	default:
		{
			BYTE	state[256];
			WORD	chars[2];

			memset(state, 0, sizeof(state));
			state[VK_CAPITAL] = (key & ModKey_Caps ) ? 0xFF : 0x00;
			state[VK_SHIFT  ] = (key & ModKey_Shift) ? 0xFF : 0x00;

			if(ToAscii(vk, 0, state, chars, 0) != 1)
				return S_OK;

			BYTE ch = (BYTE) chars[0];

			if(key & ModKey_Ctrl) {
				if('3' <= ch && ch <= '7')
					ch = 0x1B + (ch - '3');
				else if(ch == '2' || ch == ' ')
					ch = 0x00;
				else if(ch == '8' || ch == '?')
					ch = 0x7F;
				else if(ch == '-' || ch == '/')
					ch = 0x1F;
				else if(ch >= 0x40)
					ch &= 0x1F;
			}

			if(key & ModKey_Alt)
				put_vstring("\x1B%c", ch);
			else
				put_char(ch);

		}
		break;

	case VK_BACK:
		{
			bool  del = (_get_priv(Priv_Backspace) ^ ((key & ModKey_Ctrl) != 0));
			if(key & ModKey_Alt)
				put_vstring("\x1B%c", del ? '\x7F' : '\x08');
			else
				put_char(del ? '\x7F' : '\x08');
		}
		break;

	case VK_RETURN:
		{
			if(_get_priv(Priv_NewLineMode)){
				put_string("\r\n");
			}else{
				put_char('\r');
			}
		}
		break;

	case VK_HOME:   number = 1;  break;
	case VK_INSERT: number = 2;  break;
	case VK_DELETE: number = 3;  break;
	case VK_END:    number = 4;  break;
	case VK_PRIOR:  number = 5;  break;
	case VK_NEXT:   number = 6;  break;
	case VK_F1:     number = 11; break;
	case VK_F2:     number = 12; break;
	case VK_F3:     number = 13; break;
	case VK_F4:     number = 14; break;
	case VK_F5:     number = 15; break;
	case VK_F6:     number = 17; break;
	case VK_F7:     number = 18; break;
	case VK_F8:     number = 19; break;
	case VK_F9:     number = 20; break;
	case VK_F10:    number = 21; break;
	case VK_F11:    number = 23; break;
	case VK_F12:    number = 24; break;
	case VK_F13:    number = 25; break;
	case VK_F14:    number = 26; break;
	case VK_F15:    number = 28; break;
	case VK_F16:    number = 29; break;
	case VK_F17:    number = 31; break;
	case VK_F18:    number = 32; break;
	case VK_F19:    number = 33; break;
	case VK_F20:    number = 34; break;
	case VK_F21:    number = 35; break;
	case VK_F22:    number = 36; break;
	case VK_F23:    number = 37; break;
	case VK_F24:    number = 38; break;

	case VK_UP:     cursor = 'a'; break;
	case VK_DOWN:   cursor = 'b'; break;
	case VK_RIGHT:  cursor = 'c'; break;
	case VK_LEFT:   cursor = 'd'; break;
	}

	if(number) {
		char mod = (key & ModKey_Shift) ? (key & ModKey_Ctrl) ? '@' : '$' : '~';
		if(key & ModKey_Alt)
			put_vstring("\x1B\x1B[%d%c", number, mod);
		else
			put_vstring("\x1B[%d%c", number, mod);
	}
	else if(cursor) {
		char	seq;
		if(key & ModKey_Shift)         { seq = '['; }
		else if(key & ModKey_Ctrl)     { seq = 'O'; }
		else if(_get_priv(Priv_AplCUR)){ seq = 'O'; cursor -= 0x20; }
		else                           { seq = '['; cursor -= 0x20; }

		if(key & ModKey_Alt)
			put_vstring("\x1B\x1B%c%c", seq, cursor);
		else
			put_vstring("\x1B%c%c", seq, cursor);
	}

	if(_get_priv(Priv_ScrollTtyKey)) {
		int bak = m_screen.Current().GetViewPos();
		m_screen.Current().SetViewPos( INT_MAX );
		if(bak != m_screen.Current().GetViewPos()){
			m_screen_update++;
		}
	}

	return S_OK;
}

STDMETHODIMP  Pty_::PutMouse(int x, int y, ModKey key, int nclicks, VARIANT_BOOL* handled){
	if(_get_priv(Priv_MouseProtocol_X10) || _get_priv(Priv_MouseProtocol_X11)){
		*handled = TRUE;
		int w = m_screen.Current().GetPageWidth();
		int h = m_screen.Current().GetPageHeight();
		x = (x<0)? 1: (x<w)? x+1: w;
		y = (y<0)? 1: (y<h)? y+1: h;
		int v = 0x03;
		if(nclicks > 0){
			switch(key & ModKey_Key){
			case VK_LBUTTON:  v=0x00;break;
			case VK_MBUTTON:  v=0x01;break;
			case VK_RBUTTON:  v=0x02;break;
			case VK_XBUTTON1: v=0x40;break;
			case VK_XBUTTON2: v=0x41;break;
			}
		}
		if(key & ModKey_Shift) v |= 0x04;
		if(key & ModKey_Alt)   v |= 0x08;
		if(key & ModKey_Ctrl)  v |= 0x10;
		put_vstring("\x1B[M%c%c%c", 0x20+v, 0x20+x, 0x20+y);
	}
	else{
		*handled = FALSE;
	}
	return S_OK;
}

STDMETHODIMP  Pty_::PutString(BSTR str){
	int len = (int) SysStringLen(str);
	if(len<1)return S_OK;

	BYTE* tmp;
	BYTE* p;
	if(m_input_encoding & Encoding_UTF8){
		p=tmp = new BYTE[len*6];
		for(int i=0; i < len; ){
			UINT32  ucs;
			if(i+1 < len &&
			   0xD800 <= str[i+0] && str[i+0] <= 0xDBFF &&
			   0xDC00 <= str[i+1] && str[i+1] <= 0xDFFF){
				ucs = 0x10000 + ((str[i+0]-0xD800)*0x400) + (str[i+1]-0xDC00);
				i+=2;
			}else{
				ucs = str[i];
				i+=1;
			}

			if(ucs > 0x7FFFFFFF){
			}
			else if(ucs > 0x03FFFFFF){
				*p++ = 0xFC | (BYTE) ((ucs & 0x40000000) >> 30);// 1111110x       1 000000 000000 000000 000000 000000
				*p++ = 0x80 | (BYTE) ((ucs & 0x3F000000) >> 24);//                  111111 000000 000000 000000 000000
				*p++ = 0x80 | (BYTE) ((ucs & 0x00FC0000) >> 18);//                         111111 000000 000000 000000
				*p++ = 0x80 | (BYTE) ((ucs & 0x0003F000) >> 12);//                                111111 000000 000000
				*p++ = 0x80 | (BYTE) ((ucs & 0x00000FC0) >>  6);//                                       111111 000000
				*p++ = 0x80 | (BYTE) ((ucs & 0x0000003F) >>  0);//                                              111111
			}
			else if(ucs > 0x001FFFFF){
				*p++ = 0xF8 | (BYTE) ((ucs & 0x03000000) >> 24);// 111110xx      11 000000 000000 000000 000000
				*p++ = 0x80 | (BYTE) ((ucs & 0x00FC0000) >> 18);//                  111111 000000 000000 000000
				*p++ = 0x80 | (BYTE) ((ucs & 0x0003F000) >> 12);//                         111111 000000 000000
				*p++ = 0x80 | (BYTE) ((ucs & 0x00000FC0) >>  6);//                                111111 000000
				*p++ = 0x80 | (BYTE) ((ucs & 0x0000003F) >>  0);//                                       111111
			}
			else if(ucs > 0xFFFF){
				*p++ = 0xF0 | (BYTE) ((ucs & 0x001C0000) >> 18);// 11110xxx     111 000000 000000 000000
				*p++ = 0x80 | (BYTE) ((ucs & 0x0003F000) >> 12);//                  111111 000000 000000
				*p++ = 0x80 | (BYTE) ((ucs & 0x00000FC0) >>  6);//                         111111 000000
				*p++ = 0x80 | (BYTE) ((ucs & 0x0000003F) >>  0);//                                111111
			}
			else if(ucs > 0x07FF){
				*p++ = 0xE0 | (BYTE) ((ucs & 0x0000F000) >> 12);// 1110xxxx    1111 000000 000000
				*p++ = 0x80 | (BYTE) ((ucs & 0x00000FC0) >>  6);//                  111111 000000
				*p++ = 0x80 | (BYTE) ((ucs & 0x0000003F) >>  0);//                         111111
			}
			else if(ucs > 0x7F){
				*p++ = 0xC0 | (BYTE) ((ucs & 0x000007C0) >>  6);// 110xxxxx   11111 000000
				*p++ = 0x80 | (BYTE) ((ucs & 0x0000003F) >>  0);//                  111111
			}
			else{
				*p++ = (BYTE) ucs;
			}
		}
	}
	else if(m_input_encoding & Encoding_SJIS){
		p=tmp = new BYTE[len*2];
		int i=0;
		do{
			unsigned short c = Enc::ucs_to_sjis(str[i]);
			if(c>>8) *p++ = (BYTE)(c>>8);
			*p++ = (BYTE)(c);
		}while(++i < len);
	}
	else{
		p=tmp = new BYTE[len*2];
		int i=0;
		do{
			unsigned short c = Enc::ucs_to_eucj(str[i]);
			if(c>>8) *p++ = (BYTE)(c>>8);
			*p++ = (BYTE)(c);
		}while(++i < len);
	}

	m_wr_buf.Push(tmp, (int)((BYTE*)p - (BYTE*)tmp));
	SetEvent(m_wr_event);

	delete [] tmp;
	return S_OK;
}


//----------------------------------------------------------------------------


void Pty_::_finalize(){
	trace("Pty_::dtor\n");
	if(m_fd != -1){
		cygwin::close(m_fd);
		//
		if(m_wr_thread){
			BYTE buf[1] = {' '};
			m_wr_buf.Push(buf,1);
			SetEvent(m_wr_event);
		}
	}
	if(m_rd_thread){
		if(WaitForSingleObject(m_rd_thread, 5000)==WAIT_TIMEOUT)
			TerminateThread(m_rd_thread, 0);
		CloseHandle(m_rd_thread);
	}
	if(m_wr_thread){
		if(WaitForSingleObject(m_wr_thread, 5000)==WAIT_TIMEOUT)
			TerminateThread(m_wr_thread, 0);
		CloseHandle(m_wr_thread);
	}
	if(m_wr_event){
		CloseHandle(m_wr_event);
	}
	if(m_pid > 0){
		if(cygwin::waitpid(m_pid, 0, cygwin::WNOHANG)==0){
			cygwin::kill(m_pid, cygwin::SIGHUP);
			cygwin::kill(m_pid, cygwin::SIGINT);
		}
	}
}

Pty_::Pty_(IPtyNotify_* cb, BSTR cmdline)
	: m_pid(0),
	  m_fd(-1),
	  m_notify(cb),
	  m_rd_thread(0),
	  m_wr_thread(0),
	  m_wr_event(0),
	  m_priv((PrivMode)(Priv_VisibleCursor | Priv_ScrollTtyKey | Priv_AutoWrap | Priv_CjkWidth)),
	  m_input_encoding(Encoding_SJIS),
	  m_display_encoding((Encoding)(Encoding_EUCJP | Encoding_SJIS | Encoding_UTF8)),
	  m_jis_mode(0),
	  m_title(),
	  m_titleStack(){
	//
	trace("Pty_::ctor\n");
	try{
		m_title = L"ck";

		int pid,fd;
		HRESULT hr = cyg_execpty(cmdline, &pid, &fd);
		if(FAILED(hr)) throw hr;

		trace(" forkpty pid=%d fd=%d\n", pid,fd);
		m_pid = pid;
		m_fd = fd;

		m_wr_event = CreateEvent(0,FALSE,FALSE,0);
		if(!m_wr_event) throw static_cast<HRESULT>(E_FAIL);

		m_rd_thread = (HANDLE) _beginthreadex(0,0, read_proc,this, 0,0);
		m_wr_thread = (HANDLE) _beginthreadex(0,0, write_proc,this, 0,0);
		//no check
	}
	catch(...){
		_finalize();
		throw;
	}
}


UINT CALLBACK Pty_::read_proc(LPVOID lp){
	Pty_* p = (Pty_*) lp;
	const int RAWSIZE = 32768;
	const int WCSIZE  = 32768;
	BYTE*   rawbuf = new BYTE[RAWSIZE];
	UINT32* wcbuf  = new UINT32[WCSIZE];
	int rawsize = 0;
	int wcsize = 0;
	UpdateStore upp;

	for(;;){
		int n = cygwin::read(p->m_fd, rawbuf+rawsize, RAWSIZE-rawsize);
		if(n<=0) goto EXIT;
		rawsize += n;

		while(rawsize < RAWSIZE-512){
			struct cygwin::pollfd pfd = { p->m_fd, cygwin::POLLIN, 0 };
			n = cygwin::poll(&pfd, 1, 0);
			if(!(pfd.revents & cygwin::POLLIN)) break;

			n = cygwin::read(p->m_fd, rawbuf+rawsize, RAWSIZE-rawsize);
			if(n<=0) goto EXIT;
			rawsize += n;
		}
		//trace("N=%d\n", rawsize);

		//convert byte to wchar
		int rl = rawsize;
		int wl = WCSIZE - wcsize;
		switch( Enc::detect_encoding(rawbuf, rl, p->m_display_encoding)){
		case Encoding_UTF8:
			Enc::utf8_to_ucs(rawbuf, &rl, wcbuf+wcsize, &wl);
			break;
		case Encoding_SJIS:
			Enc::sjis_to_ucs(rawbuf, &rl, wcbuf+wcsize, &wl);
			break;
		default:
			Enc::eucj_to_ucs(rawbuf, &rl, wcbuf+wcsize, &wl);
			break;
		}
		if(wl<=0) continue;
		if(rawsize > rl)
			memmove(rawbuf, rawbuf+rl, rawsize-rl);
		rawsize -= rl;//remove
		wcsize += wl;//append

		#if 0
		for(int i=0; i < wcsize; i++){
			if(0x20 <= wcbuf[i] && wcbuf[i] <= 0x7E){
				fputc((char)wcbuf[i], stdout);
			}
			else{
				fprintf(stdout, "\\x%x", wcbuf[i]);
			}
		}
		fputc('\n', stdout);
		fflush(stdout);
		#endif

		upp.mask = (UpdateMask)0;
		wl=0;
		p->m_screen.Lock();
		for(;;){
			int bak=wl;
			if(!p->_process_sequence(wcbuf, wl, wcsize, upp)){
				wl=bak;
				break;
			}
		}
		if(p->_get_priv(Priv_ScrollTtyOut)){
			p->m_screen.Current().SetViewPos( INT_MAX );
		}
		p->m_screen_update++;
		p->m_screen.Unlock();
		if(wcsize > wl)
			memmove(wcbuf, wcbuf+wl, sizeof(UINT32)*(wcsize-wl));
		wcsize -= wl;
		if(wcsize > 4096){
			//error
			wcsize = 0;
		}
		//

		if(upp.mask & UpdateMask_Title) p->m_notify->OnUpdateTitle();
		if(upp.mask & UpdateMask_Font)  p->m_notify->OnReqFont(upp.font);
		if(upp.mask & UpdateMask_Pos)   p->m_notify->OnReqMove(upp.posx, upp.posy);
		if(upp.mask & UpdateMask_Size)  p->m_notify->OnReqResize(upp.sizex, upp.sizey);
	}

	EXIT:

	delete [] rawbuf;
	delete [] wcbuf;
	p->m_notify->OnClosed();
	trace("rd_thread EXIT\n");
	return 0;
}

UINT CALLBACK Pty_::write_proc(LPVOID lp){
	Pty_* p = (Pty_*) lp;
	const int IOCTL_WAIT = 200; //200ms
	const int RAWSIZE = 512;
	BYTE  rawbuf [RAWSIZE];
	DWORD timeout = INFINITE;
	DWORD last_time = GetTickCount();
	int   last_x = 0;
	int   last_y = 0;
	int   x,y,n;
	DWORD now,dw;
	for(;;){
		dw = WaitForSingleObject(p->m_wr_event, timeout);
		if(dw != WAIT_OBJECT_0 && dw != WAIT_TIMEOUT)
			break;
		n = 0;
		do{
			p->m_wr_buf.Lock();
			p->m_wr_buf.GetWinSize(&x,&y);
			p->m_wr_buf.Pop(n);
			n = p->m_wr_buf.Peek(rawbuf, RAWSIZE);
			p->m_wr_buf.Unlock();
			//
			if(n>0){
				RETRY:
				int wn = cygwin::write(p->m_fd, rawbuf, n);
				if(wn==0 && *(cyg_errno()) == 0){
					Sleep(16);
					goto RETRY;
				}
				if(wn <= 0){
					goto EXIT;
				}
				n = wn;
			}
			//
			if(x == last_x && y == last_y){
				timeout = INFINITE;
			}
			else{
				now = GetTickCount();
				dw = (now > last_time)? now-last_time: last_time - ~now +1;
				if(dw < IOCTL_WAIT){
					timeout = IOCTL_WAIT - dw;
				}
				else{
					timeout = INFINITE;
					struct cygwin::winsize ws={(unsigned short)y, (unsigned short)x,0,0};
					cygwin::ioctl(p->m_fd, cygwin::TIOCSWINSZ, &ws);
					last_time = now;
					last_x = x;
					last_y = y;
				}
			}
		}while(n>0);
	}

	EXIT:
	trace("wr_thread EXIT\n");
	return 0;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

static bool get_str_arg(UINT32 dst[], UINT32* input, int& idx, int max){
	int n=0;
	for(;;){
		if(idx >= max) return false;
		UINT32 c = input[idx];
		if(c=='\x1B'){
			if(idx+1 >= max)return false;
			if(input[idx+1]=='\x5C') idx+=2;//7bitST(\x1B\x5C)
			break;
		}
		else if(c=='\t'){
			c=' ';
		}
		else if(c<='\x1F' || c==L'\x9C'){
			//BEL(\07), ST(\9C)
			break;
		}
		idx++;
		dst[n++] = c;
		if(n>=255)break;
	}
	dst[n] = '\0';
	return true;
}

// \x1B [ (int;int;int;...) t
void Pty_::_process_sequence_window_ops(int n, int arg[], UpdateStore& upp){
	if(n<1) return;
	switch(arg[0]){
	case 11://Report state of window (normal/iconified)
		put_string("\x1B[1t");
		break;
	case 13://Report position of window (pixels)
		put_string("\x1B[3;0;0t");
		break;
	case 14://Report size of window (pixels)
		put_vstring("\x1B[4;%d;%dt", m_screen.Current().GetPageHeight()*12, m_screen.Current().GetPageWidth()*6);
		break;
	case 18://Report size of window (chars)
		put_vstring("\x1B[8;%d;%dt", m_screen.Current().GetPageHeight(), m_screen.Current().GetPageWidth());
		break;
	case 20://Report icon label
	case 21://Report window label
		break;
	case 22://Push Title
		if(n<2) arg[1]=0;
		switch(arg[1]){
		case 1://icon
			break;
		case 0://icon and title
		case 2://title
			while(m_titleStack.size() >= TITLE_STACK_MAX){
				m_titleStack.pop_front();
			}
			m_titleStack.push_back(m_title);
			break;
		}
		break;
	case 23://Pop Title
		if(n<2) arg[1]=0;
		switch(arg[1]){
		case 1://icon
			break;
		case 0://icon and title
		case 2://title
			if(m_titleStack.size() > 0){
				upp.mask = (UpdateMask)(upp.mask | UpdateMask_Title);
				m_title = m_titleStack.back();
				m_titleStack.pop_back();
			}
		}
		break;
	case 1://deiconify
	case 2://iconify
	case 4://set size (pixel)
	case 5://raise window
	case 6://lower window
	case 7://refresh window
		break;
	case 3://set position (pixel)
		if(n >= 3){
			upp.mask = (UpdateMask)(upp.mask | UpdateMask_Pos);
			upp.posx = arg[2];
			upp.posy = arg[1];
		}
		break;
	case 8://set size (chars)
		if(n >= 2){
			if(arg[2]<1) arg[2] = m_screen.Current().GetPageWidth();
			if(arg[1]<1) arg[1] = m_screen.Current().GetPageHeight();
			upp.mask = (UpdateMask)(upp.mask | UpdateMask_Size);
			upp.sizex = arg[2];
			upp.sizey = arg[1];
		}
		break;
	default://arg >=24  set sizeY (chars)
		break;
	}
}

//\x1B [ (int;int;int;...) m
void Pty_::_process_sequence_sgr(int n, int arg[], UpdateStore& upp){
	(void) upp;
	if(n<1) m_screen.Current().ClearStyle(CharFlag_Styles);
	for(int i=0; i < n; i++){
		switch(arg[i]){
		case 0: m_screen.Current().ClearStyle(CharFlag_Styles);break;
		case 1: m_screen.Current().SetStyle(CharFlag_Bold);break;
		case 4: m_screen.Current().SetStyle(CharFlag_Uline);break;
		case 5: m_screen.Current().SetStyle(CharFlag_Blink);break;
		case 7: m_screen.Current().SetStyle(CharFlag_Invert);break;
		case 22: m_screen.Current().ClearStyle(CharFlag_Bold);break;
		case 24: m_screen.Current().ClearStyle(CharFlag_Uline);break;
		case 25: m_screen.Current().ClearStyle(CharFlag_Blink);break;
		case 27: m_screen.Current().ClearStyle(CharFlag_Invert);break;
		case 30:case 31:case 32:case 33:
		case 34:case 35:case 36:case 37:
			m_screen.Current().SetStyleFG( (BYTE)(arg[i] - 30) );
			break;
		case 40:case 41:case 42:case 43:
		case 44:case 45:case 46:case 47:
			m_screen.Current().SetStyleBG( (BYTE)(arg[i] - 40) );
			break;
		case 90:case 91:case 92:case 93:
		case 94:case 95:case 96:case 97:
			m_screen.Current().SetStyleFG( (BYTE)(arg[i] - 90 + 8) );
			break;
		case 100:case 101:case 102:case 103:
		case 104:case 105:case 106:case 107:
			m_screen.Current().SetStyleBG( (BYTE)(arg[i] - 100 + 8) );
			break;
		case 38:
			if(i+2 >= n)return;
			if(arg[++i]==5)
				m_screen.Current().SetStyleFG( (BYTE) arg[++i] );
			break;
		case 48:
			if(i+2 >= n)return;
			if(arg[++i]==5)
				m_screen.Current().SetStyleBG( (BYTE) arg[++i] );
			break;
		case 39: m_screen.Current().ClearStyle(CharFlag_FG);break;
		case 49: m_screen.Current().ClearStyle(CharFlag_BG);break;
		}
	}
}

//\x1B [ ? (int;int;int;...) h,l
void Pty_::_process_sequence_term_mode(char priv, int n, int arg[], UpdateStore& upp){
	for(int i=0; i < n; i++){
		switch(arg[i]){
		case 1://DECCKM application cursor keys
			_set_priv(priv, Priv_AplCUR);
			break;
		case 2://DECANM set VT52 mode
			if(priv == 'l'){
				_set_priv('h', Priv_VT52);
			}
			break;
		case 3://DECCOLM 132 colmn mode
			#if 0
			_set_priv(priv, Priv_Columns132);
			m_screen.Fore().ErasePage(2);
			#else
			if(_set_priv(priv, Priv_Columns132)){
				upp.mask = (UpdateMask)(upp.mask | UpdateMask_Size);
				upp.sizex = 132;
				upp.sizey = 24;
				m_screen.Fore().Resize(132,24);
				m_screen.Back().Resize(132,24);
				_reset(true);
			}else{
				upp.mask = (UpdateMask)(upp.mask | UpdateMask_Size);
				upp.sizex = 80;
				upp.sizey = 24;
				m_screen.Fore().Resize(80,24);
				m_screen.Back().Resize(80,24);
				_reset(true);
			}
			#endif
			break;
		case 4://DECSCLM smooth scroll
			_set_priv(priv, Priv_SmoothScroll);
			break;
		case 5://DECSCNM reverse video
			_set_priv(priv, Priv_ReverseVideo);
			break;
		case 6://DECOM relative/absolute origin mode
			m_screen.Current().SetOriginMode( _set_priv(priv, Priv_RelativeOrigin) );
			break;
		case 7://DECAWM autowrap
			m_screen.Current().SetAutoWrapMode( _set_priv(priv, Priv_AutoWrap) );
			break;
		case 8://DECARM auto repeat keys
			break;
		case 9://mouse X10
			_set_priv(priv, Priv_MouseProtocol_X10);
			break;
		case 12://att610 blinking cursor
		case 18://DECPFF print from feed
		case 19://DECPEX print extent
			break;
		case 25://cursor visible
			_set_priv(priv, Priv_VisibleCursor);
			break;
		case 30://rxvt toggle scrollbar?
			break;
		case 35:
			_set_priv(priv, Priv_ShiftKey);
			break;
		case 38://DECTEK tektronix mode
			break;
		case 40://allow 80 <-> 132 mode
			_set_priv(priv, Priv_Allow132);
			break;
		case 41://cursor fix
		case 42://DECNRCM national chaset (VT220)
		case 44://margin bell
		case 45://reverse wraparound mode
		case 46://start/stop logging
		case 47://normal/alternate screen buffer
			m_screen.Set( _set_priv(priv, Priv_Screen) );
			break;
		case 1047://secondary screen clearing
		case 1049://secondary buffer & cursor
			m_screen.Set( _set_priv(priv, Priv_Screen) );
			m_screen.Back().ErasePage(2);
			break;
		case 66://DECNKM
			_set_priv(priv, Priv_AplKP);
			break;
		case 67://DECBKM
			_set_priv(priv, Priv_Backspace);
			break;
		case 1000://X11 mouse reporting (VT200 mouse)
		case 1001://X11 mouse highliting (VT200 highlight mouse)
		case 1002://X11 button event mouse
		case 1003://X11 any event mouse
			_set_priv(priv, Priv_MouseProtocol_X11);
			break;
		case 1010://scroll to bottom TTY output inhibit
			_set_priv(priv, Priv_ScrollTtyOut);
			break;
		case 1011://scroll to bottom on key press
			_set_priv(priv, Priv_ScrollTtyKey);
			break;
		case 1035://numlock
		case 1036://meta sends escape
		case 1037://delete is del
		case 1048://alternative cursor save
		case 1051://keyboard type SUN
		case 1052://keyboard type HP
		case 1053://keyboard type SCO
		case 1060://keyboard type legacy
		case 1061://keyboard type VT220
			break;
		case 8840://switch cjk width mode
			_set_priv(priv, Priv_CjkWidth);
			break;
		}
	}
}

// \x1B [ (...)
bool Pty_::_process_sequence_csi(UINT32* input, int& idx, int max, UpdateStore& upp){
	if(idx >= max) return false;
	char priv = '\0';
	switch(input[idx]){
	case '<':
	case '>':
	case '=':
	case '?':
	case '!':
		priv = (char) input[idx++];
		break;
	}

	int arg[32];
	int n=0;
	{
		int value = -1;
		for(;;){
			if(idx >= max) return false;
			if('0' <= input[idx] && input[idx] <= '9'){
				if(value>=0){
					value = (value*10) + (input[idx]-'0');
				}else{
					value = (0)        + (input[idx]-'0');
				}
			}
			else{
				if(input[idx] == ';' && value < 0){
					value = 0;
				}
				if(value>=0){
					if(n<32){
						arg[n++] = value;
						value = -1;
					}
				}

				if(input[idx] == ';'){
				}
				else if(input[idx] <= ' '){
					_process_sequence_ascii((char)input[idx], upp);
				}
				else{
					break;
				}
			}
			++idx;
		}
	}

	//if(idx >= max)return false;
	if(priv){
		if(priv=='>' && input[idx]=='c'){//DA
			put_string(FIRMWARE_VERSION);
			idx++;
		}
		else if(priv=='=' && input[idx]=='c'){//DA
			idx++;
		}
		else if(priv=='?'){
			priv = (char)input[idx++];  // \x1B[?12h \x1B[?12l
			if(priv == '$') priv = (char)input[idx++]; //DECRQM \x1B[?12$p
			_process_sequence_term_mode(priv, n, arg, upp);
		}
		else if(priv=='!'){
			idx++;
		}
		return true;
	}

	int arg0 = (n>=1) ? arg[0]: 0;
	int arg1 = (n>=2) ? arg[1]: 0;
	UINT32  ch = input[idx++];

	switch(ch){
	case 'F'://CPL cursor pending line
		if(arg0==0) arg0=1;
		m_screen.Current().MoveCurY(-arg0);
		m_screen.Current().SetCurX(0);
		break;
	case 'E'://CNL cursor next line
		if(arg0==0) arg0=1;
		m_screen.Current().MoveCurY(+arg0);
		m_screen.Current().SetCurX(0);
		break;
	case 'A'://CUU cursor up
	case 'e'://VPR line position forward
		if(arg0==0) arg0=1;
		m_screen.Current().MoveCurY(-arg0);
		break;
	case 'B'://CUD cursor down
	case 'k'://VPB line position backward
		if(arg0==0) arg0=1;
		m_screen.Current().MoveCurY(+arg0);
		break;
	case 'C'://CUF cursor right
	case 'a'://HPR character position forward
		if(arg0==0) arg0=1;
		m_screen.Current().MoveCurX(+arg0);
		break;
	case 'D'://CUB cursor left
	case 'j'://HPB character position backward
		if(arg0==0) arg0=1;
		m_screen.Current().MoveCurX(-arg0);
		break;
	case 'G'://CHA cursor character absolute
	case '`'://HPA cursor position absolute
		if(arg0==0) arg0=1;
		m_screen.Current().SetCurX(arg0-1);
		break;
	case 'd'://VPA line position absolute
		if(arg0==0) arg0=1;
		m_screen.Current().SetCurY(arg0-1);
		break;
	case 'H'://CUP cursor position
	case 'f'://HVP character and line position
		if(arg0==0) arg0=1;
		if(arg1==0) arg1=1;
		m_screen.Current().SetCurY(arg0-1);
		m_screen.Current().SetCurX(arg1-1);
		break;
	case 'Z'://CBT cursor backward tabulation
		if(n==0 && arg0==0) arg0=1;
		m_screen.Current().MoveCurTab(-arg0);
		break;
	case 'I'://CBT cursor forward tabulation
		if(n==0 && arg0==0) arg0=1;
		m_screen.Current().MoveCurTab(+arg0);
		break;
	case 'J'://ED erase in page
		m_screen.Current().ErasePage(arg0);
		break;
	case 'K'://EL erase in line
		m_screen.Current().EraseLine(arg0);
		break;
	case 'L'://IL insert line
		m_screen.Current().InsertLine(arg0);
		break;
	case 'M'://DL delete line
		m_screen.Current().DeleteLine(arg0);
		break;
	case 'X'://ECH erase character
		m_screen.Current().EraseChar(arg0);
		break;
	case '@'://ICH insert character
		m_screen.Current().InsertChar(arg0);
		break;
	case 'P'://DCH delete character
		m_screen.Current().DeleteChar(arg0);
		break;
	case 'T'://SD scroll down
		m_screen.Current().ScrollPage(-arg0);
		break;
	case 'S'://SU scroll up
		m_screen.Current().ScrollPage(arg0);
		break;
	case 'g'://TBC tabulation clear
		if(arg0 == 0)
			m_screen.Current().SetTabstop(false);
		if(arg0 == 3)
			m_screen.Current().ClearTabstop();
		break;
	case 'W'://CTC cursor tabulation control
		break;
	case 'l'://RM reset mode
	case 'h'://SM set mode
		switch(arg0){
		case 4://IRM insert/reset mode
			m_screen.Current().SetAddMode(ch=='h');
			break;
		case 20://LNM linefeed/newline mode
			_set_priv((char)ch, Priv_NewLineMode);
			break;
		}
		break;
	case 's'://73
		m_screen.Current().SaveCur();
		break;
	case 'u'://75
		m_screen.Current().RestoreCur();
		break;
	case 'r'://72
		if(arg0==0) arg0=1;
		if(arg1==0) arg1=1;
		if(n < 2) arg1 = m_screen.Current().GetPageHeight();
		if(arg0>=arg1) arg0=arg1=1;
		m_screen.Current().SetCurY(arg0-1);
		m_screen.Current().SetCurX(0);
		m_screen.Current().SetRegion(arg0-1, arg1-1);
		break;
	case 'c'://DA device attribute
		put_string(DEVICE_ATTRIBUTE);
		break;
	case 'm'://SGR select graphic rendition
		_process_sequence_sgr(n,arg,upp);
		break;
	case 'n'://DSR device status report
		switch(arg0){
		case 5:
			put_string("\x1B[0n");
			break;
		case 6:
			put_vstring("\x1B[%d;%dR", m_screen.Current().GetCurY()+1, m_screen.Current().GetCurX()+1);
			break;
		}
		break;
	case 'x'://DECREQTPARM terminal parameters report
		switch(arg0){
		case 0:
			put_string("\x1B[2;1;1;128;128;1;0x");
			break;
		case 1:
			put_string("\x1B[3;1;1;128;128;1;0x");
			break;
		}
		break;
	case 't'://74
		_process_sequence_window_ops(n, arg, upp);
		break;
	}
	return true;
}

// \x1B ] (int;string) \x07
bool Pty_::_process_sequence_osc(UINT32* input, int& idx, int max, UpdateStore& upp){
	int opt = 0;
	while(idx<max && '0'<=input[idx] && input[idx]<='9')
		opt = (opt*10) + input[idx++] - '0';
	if(idx >= max)return false;
	if(input[idx] != ';')return true;
	UINT32 arg [256];
	if(! get_str_arg(arg, input, ++idx, max))
		return false;
	//
	switch(opt){
	case 0://xterm title and icon name
	case 2://xterm title
		{
			BSTR bstr = Ck::Util::ucs4_to_bstr(arg, Ck::Util::ucs4len(arg));
			if(bstr){
				if(m_title != bstr){
					upp.mask = (UpdateMask)(upp.mask | UpdateMask_Title);
					m_title = bstr;
				}
				SysFreeString(bstr);
			}
		}
		break;
	case 50://xterm font, ck-custom "#ck0" ~ "#ck3"
		if((arg[0]=='#') &&
		   (arg[1]=='C' || arg[1]=='c') &&
		   (arg[2]=='K' || arg[2]=='k') &&
		   ('0'<=arg[3] && arg[3]<='9')){
			int n = 0;
			for(int i=3; '0'<=arg[i] && arg[i]<='9'; i++)
				n = (n * 10) + arg[i] - '0';
			upp.mask = (UpdateMask)(upp.mask | UpdateMask_Font);
			upp.font = n;
		}
		break;
	case 1://xterm icon name
	case 4://xterm color
	case 12://xterm color cursor
	case 13://xterm color pointer
	case 17://xterm color rvideo
	case 18://xterm color border
	case 19://xterm color uline
	case 10://xterm menu
	case 46://xterm log file
	case 20://xterm pixmap
	case 39://xterm restore fg
	case 49://xterm restore bg
	case 55://xterm dump screen
		break;
	}
	return true;
}

// \x1B P (...) \x07
bool Pty_::_process_sequence_dcs(UINT32* input, int& idx, int max, UpdateStore& upp){
	(void) upp;
	UINT32 arg [256];
	if(! get_str_arg(arg, input, idx, max))
		return false;
	//
	return true;
}

// \x1B (...)
bool Pty_::_process_sequence_esc(UINT32* input, int& idx, int max, UpdateStore& upp){
	if(idx >= max) return false;
	switch(input[idx++]){
	case '#':
		switch(input[idx++]){
		case '3'://DECDHL 2x top
		case '4'://DECDHL 2x bottom
		case '5'://DECSWL normal
		case '6'://DECDWL 2x
			break;
		case '8'://DECALN fills screen
			m_screen.Current().FillChar('E', (CharFlag)0);
			break;
		}
		break;
	case '('://charset0
		if(idx >= max) return false;
		switch(input[idx++]){
		case 'B':
		case 'J':m_jis_mode=0;break;
		case 'I':m_jis_mode=1;break;
		}
		break;
	case ')'://charset1
	case '*'://charset2
	case '+'://charset3
		if(idx >= max)return false;
		idx++;
		break;
	case '$'://charset -2 multi charset
		if(idx >= max)return false;
		switch(input[idx++]){
		case '@': m_jis_mode=1;break;
		case 'B': m_jis_mode=2;break;
		case '(':
			if(idx >= max)return false;
			switch(input[idx++]){
			case 'D'://sup?
			case 'O'://0213 map1?
			case 'P'://0213 map2?
				break;
			}
			break;
		}
		break;
	case '@'://C1_40
		if(idx >= max)return false;
		idx++;
		break;
	case '7'://save cursor
		m_screen.Current().SaveCur();
		break;
	case '8'://restore cursor
		m_screen.Current().RestoreCur();
		break;
	case '=':
		_set_priv('h', Priv_AplKP);
		break;
	case '>':
		_set_priv('l', Priv_AplKP);
		break;
	case 'D'://C1_44
		m_screen.Current().Feed();
		break;
	case 'E'://NEL next line
		m_screen.Current().SetCurX(0);
		m_screen.Current().Feed();
		break;
	case 'G'://ESA kidnapped escape sequence
		if(idx >= max)return false;
		switch(input[idx++]){
		case 'Q'://query graphics
			put_string("\x1BG0");//no graphics
			break;
		}
		break;
	case 'H'://HTS character tabulation set
		m_screen.Current().SetTabstop(true);
		break;
	case 'M'://RI reverse line feed
		m_screen.Current().FeedRev();
		break;
	case 'Z'://SCI single character introducer
		break;
	case 'c'://RIS reset to internal state
		_reset(true);
		break;
	case 'n'://charset choose 2
	case 'o'://charset choose 3
		break;
	case 'P'://DCS device control string
		return _process_sequence_dcs(input, idx, max, upp);
	case '['://CSI control sequence introducer
		return _process_sequence_csi(input, idx, max, upp);
	case ']'://OSC operating system command
		return _process_sequence_osc(input, idx, max, upp);
	}
	return true;
}

//VT52 \x1B (...)
bool Pty_::_process_sequence_vt52(UINT32* input, int& idx, int max, UpdateStore& upp){
	(void) upp;
	if(idx >= max) return false;
	switch(input[idx++]){
	case 'A':
		m_screen.Current().MoveCurY(-1);
		break;
	case 'B':
		m_screen.Current().MoveCurY(+1);
		break;
	case 'C':
		m_screen.Current().MoveCurX(+1);
		break;
	case 'D':
		m_screen.Current().MoveCurX(-1);
		break;
	case 'H':
		m_screen.Current().SetCurY(0);
		m_screen.Current().SetCurX(0);
		break;
	case 'I':
		m_screen.Current().FeedRev();
		break;
	case 'J':
		m_screen.Current().ErasePage(0);
		break;
	case 'K':
		m_screen.Current().EraseLine(0);
		break;
	case 'Y':
		if(idx+2 >= max) return false;
		m_screen.Current().SetCurY( input[idx++] - ' ' );
		m_screen.Current().SetCurX( input[idx++] - ' ' );
		break;
	case 'Z'://identity terminal type, VT52
		put_string("\x1B/Z");
		break;
	case '<'://turn off VT52 mode
		_set_priv('l', Priv_VT52);
		break;
	case 'F'://use special graphics character set
	case 'G'://use regular character set
	case '='://use alternate keypad mode
	case '>'://use regular keypad mode
		break;
	}
	return true;
}

void Pty_::_process_sequence_ascii(char ch, UpdateStore& upp){
	(void) upp;
	switch(ch){
	case '\x05'://ENQ
		put_string(ANSWERBACK_MESSAGE);
		break;
	case '\x07'://BEL
		if(_get_priv(Priv_UseBell))
			MessageBeep(0);
		break;
	case '\x08'://BS backspace
		m_screen.Current().MoveCurX(-1);
		break;
	case '\x09'://HT horiz tab
		m_screen.Current().MoveCurTab(1);
		break;
	case '\x0A'://LT linefeed
	case '\x0B'://VT vert tab
	case '\x0C'://FF,CL
		m_screen.Current().Feed();
		break;
	case '\x0D'://CR carriage return
		m_screen.Current().SetCurX(0);
		break;
	case '\x00':
	case '\x01'://SOH
	case '\x02'://STX
	case '\x03'://ETX
	case '\x04'://EOT
	case '\x06'://ACK
	case '\x0E'://SO
	case '\x0F'://SI
	case '\x10'://DLE
	case '\x11'://DC1
	case '\x12'://DC2
	case '\x13'://DC3
	case '\x14'://DC4
	case '\x15'://NAC
	case '\x16'://SYN
	case '\x17'://ETB
	case '\x18'://CAN
	case '\x19'://EM
	case '\x1A'://SUB
	case '\x1B'://C0_ESC
	case '\x1C'://FS
	case '\x1D'://GS
	case '\x1E'://RS
	case '\x1F'://US
		break;
	}
}

bool Pty_::_process_sequence(UINT32* input, int& idx, int max, UpdateStore& upp){
	if(idx >= max) return false;

	UINT32 ch = input[idx++];
	if(ch == 0x1B){
		if(_get_priv(Priv_VT52))
			return _process_sequence_vt52(input, idx, max, upp);
		else
			return _process_sequence_esc(input, idx, max, upp);
	}
	else if(ch <= 0x1F){
		_process_sequence_ascii((char)ch, upp);
	}
	else{
		if(m_jis_mode==1 && '\x21' <= ch && ch <= '\x5F'){//kana
			ch += 0xff61 - 0x21;
		}
		else if(m_jis_mode==2 && '\x21' <= ch && ch <= '\x7E'){//kanji
			if(idx >= max) return false;
			UINT32 tmp = Enc::eucj_to_ucs((WORD)( (ch<<8) | input[idx] | 0x8080 ));
			if(tmp){
				ch = tmp;
				idx++;
			}
		}

		bool (*isdbl)(UINT32) = _get_priv(Priv_CjkWidth) ? Enc::is_dblchar_cjk : Enc::is_dblchar;
		if(isdbl(ch)){
			m_screen.Current().AddCharMB(ch);
		}else{
			m_screen.Current().AddChar(ch);
		}
	}
	return true;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

class Pty: public IDispatchImpl3<IPty>, public IPtyNotify_{
protected:
	Pty_*  m_obj;
	IPtyNotify* const  m_notify;
	//
	void _finalize(){
		if(m_obj){ delete m_obj; m_obj=0; }
	}
	virtual ~Pty(){
		trace("Pty::dtor\n");
		_finalize();
	}
public:
	Pty(IPtyNotify* cb, BSTR cmdline): m_obj(0), m_notify(cb){
		trace("Pty::ctor\n");
		m_obj = new Pty_(this,cmdline);
	}
	//
	STDMETHOD(OnClosed)()			{ return m_notify->OnClosed(this); }
	STDMETHOD(OnUpdateTitle)()		{ return m_notify->OnUpdateTitle(this); }
	STDMETHOD(OnReqFont)(int id)		{ return m_notify->OnReqFont(this,id); }
	STDMETHOD(OnReqMove)(int x, int y)	{ return m_notify->OnReqMove(this,x,y); }
	STDMETHOD(OnReqResize)(int x, int y)	{ return m_notify->OnReqResize(this,x,y); }
	//
	STDMETHOD(Dispose)(){
		trace("Pty::dispose\n");
		_finalize();
		return S_OK;
	}
	//
	STDMETHOD(get_PageWidth)(int* p) { return m_obj->get_PageWidth(p);}
	STDMETHOD(get_PageHeight)(int* p){ return m_obj->get_PageHeight(p);}
	STDMETHOD(get_CursorPosX)(int* p){ return m_obj->get_CursorPosX(p);}
	STDMETHOD(get_CursorPosY)(int* p){ return m_obj->get_CursorPosY(p);}
	STDMETHOD(get_Savelines)(int* p){ return m_obj->get_Savelines(p);}
	STDMETHOD(put_Savelines)(int n) { return m_obj->put_Savelines(n);}
	STDMETHOD(get_ViewPos)(int* p){ return m_obj->get_ViewPos(p);}
	STDMETHOD(put_ViewPos)(int n) { return m_obj->put_ViewPos(n);}
	STDMETHOD(get_InputEncoding)(Encoding* p){ return m_obj->get_InputEncoding(p);}
	STDMETHOD(put_InputEncoding)(Encoding n) { return m_obj->put_InputEncoding(n);}
	STDMETHOD(get_DisplayEncoding)(Encoding* p){ return m_obj->get_DisplayEncoding(p);}
	STDMETHOD(put_DisplayEncoding)(Encoding n) { return m_obj->put_DisplayEncoding(n);}
	STDMETHOD(get_PrivMode)(PrivMode* p){ return m_obj->get_PrivMode(p);}
	STDMETHOD(put_PrivMode)(PrivMode n) { return m_obj->put_PrivMode(n);}
	STDMETHOD(get_Title)(BSTR* pp){ return m_obj->get_Title(pp);}
	STDMETHOD(put_Title)(BSTR p) { return m_obj->put_Title(p);}
	STDMETHOD(get_CurrentDirectory)(BSTR* pp){ return m_obj->get_CurrentDirectory(pp);}
	STDMETHOD(_screen_tick)(int* p){ return m_obj->_screen_tick(p);}
	STDMETHOD(_new_snapshot)(Snapshot** pp){ return m_obj->_new_snapshot(pp);}
	STDMETHOD(_del_snapshot)(Snapshot* p){ if(p) delete [] (BYTE*)p; return S_OK;}
	STDMETHOD(Resize)(int w, int h){ return m_obj->Resize(w,h);}
	STDMETHOD(Reset)(VARIANT_BOOL full){ return m_obj->Reset(full);}
	STDMETHOD(PutString)(BSTR str){ return m_obj->PutString(str);}
	STDMETHOD(PutKeyboard)(ModKey keycode){ return m_obj->PutKeyboard(keycode);}
	STDMETHOD(PutMouse)(int x, int y, ModKey key, int nclicks, VARIANT_BOOL* handled){ return m_obj->PutMouse(x,y,key,nclicks,handled);}
	STDMETHOD(SetSelection)(int x, int y, int mode){ return m_obj->SetSelection(x,y,mode);}
	STDMETHOD(get_SelectedString)(BSTR* pp){ return m_obj->get_SelectedString(pp);}
};

}//namespace Ck


extern "C" __declspec(dllexport) HRESULT CreatePty(Ck::IPtyNotify* cb, BSTR cmdline, Ck::IPty** pp){
	if(!pp) return E_POINTER;
	if(!cb) return E_INVALIDARG;

	HRESULT hr;
	Ck::Pty* p = 0;

	try{
		p = new Ck::Pty(cb,cmdline);
		p->AddRef();
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
	*pp = p;
	return hr;
}

//EOF
