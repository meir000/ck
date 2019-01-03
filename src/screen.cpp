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

//#import  "interface.tlb" raw_interfaces_only
#include "interface.tlh"
#include "screen.h"
#include "util.h"

namespace Ck{

void Screen_::_finalize(){
	if(m_buffer){
		Line* p = m_buffer;
		Line* e = m_buffer + m_maxsize;
		do{
			if(p->chars)
				free(p->chars);
			p++;
		}while(p<e);
		delete [] m_buffer;
	}
	if(m_tabstop){
		free(m_tabstop);
	}
}

Screen_::Screen_()
	: m_buffer(0),
	  m_maxsize(0),
	  m_head(0),
	  m_view_top(0),
	  m_page_top(0),
	  m_pageW(0),
	  m_pageH(0),
	  m_curX(0),
	  m_curY(0),
	  m_style((CharFlag)0),
	  m_rgn_top(0),
	  m_rgn_btm(0),
	  m_sel_y(-1),
	  m_sel_x(0),
	  m_sel_top(-1),
	  m_sel_topX(0),
	  m_sel_btm(-1),
	  m_sel_btmX(0),
	  m_save_curX(0),
	  m_save_curY(0),
	  m_save_style((CharFlag)0),
	  m_savelines(0),
	  m_feed_next(false),
	  m_insert_mode(false),
	  m_autowrap_mode(true),
	  m_origin_mode(false),
	  m_tabstop(0){
	//
	try{
		m_maxsize = 20;
		m_pageW = 80;
		m_pageH = 20;
		m_buffer = new Line[m_maxsize];
		memset(m_buffer, 0, sizeof(Line) * m_maxsize);
		for(int i=0; i < m_maxsize; ++i){
			_resize_line(m_buffer[i]);
		}
		_resize_tabs(0, m_pageW);
	}
	catch(...){
		_finalize();
		throw;
	}
}

bool Screen_::check_selection_x(int y, int l, int r){
	if(m_sel_btm >= 0){
		y += m_page_top;
		if(m_sel_top < m_sel_btm){
			if((m_sel_top < y && y < m_sel_btm) ||
			   (m_sel_top == y && r >= m_sel_topX) ||
			   (m_sel_btm == y && l < m_sel_btmX))
				return true;
		}
		else if(m_sel_btm == y){
			if((m_sel_topX <= l && l < m_sel_btmX) ||
			   (m_sel_topX <= r && r < m_sel_btmX) ||
			   (l <= m_sel_topX && m_sel_btmX <= r))
				return true;
		}
	}
	return false;
}

bool Screen_::check_selection_y(int t, int b){
	if(m_sel_btm >= 0){
		t += m_page_top;
		b += m_page_top;
		if((m_sel_top <= t && t <= m_sel_btm) ||
		   (m_sel_top <= b && b <= m_sel_btm) ||
		   (t <= m_sel_top && m_sel_btm <= b))
			return true;
	}
	return false;
}

void Screen_::clear_line(Line& p, int start, int end){
	for(; start < end; start++){
		p.chars[start].ch = '\0';
		p.chars[start].flags = CharFlag_Null;
	}
}

void Screen_::_resize_tabs(int old, int now){
	bool* tmp = (bool*) realloc(m_tabstop, sizeof(bool) * now);
	if(!tmp) throw std::bad_alloc();
	m_tabstop = tmp;
	for(int i=old; i < now; ++i){
		m_tabstop[i] = ((i&7)==0);
	}
}

void Screen_::_resize_line(Line& p){
	if(p.maxsize < m_pageW){
		Char* tmp = (Char*) realloc(p.chars, sizeof(Char) * m_pageW);
		if(!tmp) throw std::bad_alloc();
		p.chars = tmp;
		p.maxsize = m_pageW;
	}
	clear_line(p, p.count, m_pageW);
	p.count = m_pageW;
}

void Screen_::_resize(int newsize){
	if(m_maxsize < newsize){
		newsize += 200;
		Line* tmp = new Line[newsize];
		memcpy(tmp, m_buffer+m_head, sizeof(Line) * (m_maxsize-m_head));
		memcpy(tmp+m_maxsize-m_head, m_buffer, sizeof(Line) * m_head);
		memset(tmp+m_maxsize, 0, sizeof(Line) * (newsize-m_maxsize));
		delete [] m_buffer;
		m_buffer = tmp;
		m_maxsize = newsize;
		m_head = 0;
	}
}


void Screen_::scroll_up(int top, int btm, int n){
	if(n > btm-top) n = btm-top;
	if(n<1) return;

	Line* work = new Line[n];
	for(int i=0; i < n; i++){
		work[i] = get_line(top+i);
	}
	int remain = (btm-top) - n;
	for(int i=0; i < remain; i++){
		Line& p = get_line(top+n+i);
		get_line(top+i) = p;
	}
	for(int i=0; i < n; i++){
		Line& p = get_line(btm-n+i);
		p = work[i];
		clear_line(p, 0, p.count);
	}
	delete [] work;

	if(check_selection_y(top,btm))
		ClearSelection();
}
void Screen_::scroll_down(int top, int btm, int n){
	if(n > btm-top) n = btm-top;
	if(n<1) return;

	Line* work = new Line[n];
	for(int i=0; i < n; i++){
		work[i] = get_line(btm-1-i);
	}
	int remain = (btm-top) - n;
	for(int i=1; i <= remain; i++){
		Line& p = get_line(btm-i-n);
		get_line(btm-i) = p;
	}
	for(int i=0; i < n; i++){
		Line& p = get_line(top+i);
		p = work[i];
		clear_line(p, 0, p.count);
	}
	delete [] work;

	if(check_selection_y(top,btm))
		ClearSelection();
}

void Screen_::add_char(UINT32 w, CharFlag style){
	if(m_feed_next){
		m_feed_next = false;
		if(m_autowrap_mode){
			Feed();
			m_curX = 0;
		}
	}

	if(m_insert_mode){
		InsertChar(1);
		Line& p = get_line(m_curY);
		p.chars[m_curX].ch = w;
		p.chars[m_curX].flags = style;
	}
	else{
		Line& p = get_line(m_curY);
		p.chars[m_curX].ch = w;
		p.chars[m_curX].flags = style;
		if(check_selection_x(m_curY, m_curX, m_curX))
			ClearSelection();
	}

	if(++m_curX >= m_pageW){
		m_curX--;
		m_feed_next = true;
	}
}


void Screen_::Resize(int w, int h){
	if(w < 1 || h < 1 || (w==m_pageW && h==m_pageH))
		return;

	_resize(h + m_savelines);

	if(m_pageW != w){
		_resize_tabs(m_pageW, w);
		m_pageW = w;
		m_feed_next = false;
		if(m_curX >= w)
			m_curX = w-1;
		if(m_sel_btm >= 0){
			if(m_sel_top < m_sel_btm || m_sel_btmX > w)
				ClearSelection();
		}
	}

	if(m_pageH < h){
		int pos = m_head + m_page_top + m_pageH;
		int n = h - m_pageH;
		int move = (m_page_top < n) ? m_page_top : n;
		m_curY += move;
		m_page_top -= move;
		m_view_top -= move;
		if(m_view_top < 0){
			m_view_top = 0;
		}
		if(m_rgn_btm){
			m_rgn_top += move;
			m_rgn_btm += move;
		}
		for(; move < n; move++){
			if(pos >= m_maxsize)
				pos -= m_maxsize;
			Line* p = m_buffer + (pos++);
			p->count = 0;
		}
		m_pageH = h;
	}
	else if(m_pageH > h){
		int n = m_pageH - h;
		int move = (n) - (m_pageH - m_curY -1);
		if(move > 0){
			m_curY -= move;
			m_page_top += move;
			m_view_top += move;
			if(m_rgn_btm){
				m_rgn_top -= move;
				m_rgn_btm -= move;
				if(m_rgn_top < 0)
					m_rgn_top = m_rgn_btm = 0;
			}
			n = m_page_top - m_savelines;
			if(n > 0){
				m_head += n;
				if(m_head >= m_maxsize)
					m_head -= m_maxsize;
				m_page_top -= n;
				m_view_top -= n;
				if(m_view_top < 0)
					m_view_top = 0;
			}
		}
		if(m_rgn_btm){
			if(m_rgn_btm >= h-1)
				m_rgn_top = m_rgn_btm = 0;
		}
		if(m_sel_btm >= 0){
			if(m_sel_btm >= m_page_top+h)
				ClearSelection();
		}
		m_pageH = h;
	}

	int pos = m_head + m_page_top + 0;
	int n = m_pageH;
	do{
		if(pos >= m_maxsize)
			pos -= m_maxsize;
		_resize_line( m_buffer[pos++] );
	}while(--n);
}


void Screen_::Reset(bool full){
	SetRegion(0,0);
	ClearStyle(CharFlag_Styles);
	SetAddMode(false);
	SetAutoWrapMode(true);
	SetOriginMode(false);

	for(int i=0; i < m_pageW; ++i)
		m_tabstop[i] = ((i&7)==0);

	if(full){
		m_curX=0;
		m_curY=0;
		ErasePage(2);
	}
}


void Screen_::ErasePage(int mode){
	if(mode==0){
		{
			Line& p = get_line(m_curY);
			clear_line(p, m_curX, p.count);
		}
		for(int i=m_curY+1; i < m_pageH; i++){
			Line& p = get_line(i);
			clear_line(p, 0, p.count);
		}
		if(check_selection_y(m_curY+1, m_pageH-1) || check_selection_x(m_curY, m_curX,m_pageW-1))
			ClearSelection();
	}
	else if(mode==1){
		{
			Line& p = get_line(m_curY);
			clear_line(p, 0, m_curX+1);
		}
		for(int i=0; i < m_curY; i++){
			Line& p = get_line(i);
			clear_line(p, 0, p.count);
		}
		if(check_selection_y(0, m_curY-1) || check_selection_x(m_curY, 0, m_curX))
			ClearSelection();
	}
	else if(mode==2){
		for(int i=0 ; i < m_pageH; i++){
			Line& p = get_line(i);
			clear_line(p, 0, p.count);
		}
		if(check_selection_y(0, m_pageH-1))
			ClearSelection();
	}
	else{
		return;
	}
	m_feed_next = false;
}

void Screen_::EraseLine(int mode){
	if(mode==0){
		Line& p = get_line(m_curY);
		clear_line(p, m_curX, p.count);
		if(check_selection_x(m_curY, m_curX, m_pageW-1))
			ClearSelection();
	}
	else if(mode==1){
		Line& p = get_line(m_curY);
		clear_line(p, 0, m_curX+1);
		if(check_selection_x(m_curY, 0, m_curX))
			ClearSelection();
	}
	else if(mode==2){
		Line& p = get_line(m_curY);
		clear_line(p, 0, p.count);
		if(check_selection_y(m_curY, m_curY))
			ClearSelection();
	}
	else{
		return;
	}
	m_feed_next = false;
}

void Screen_::SetRegion(int top, int btm){
	if(top < 0 ||
	   top >= btm ||
	   top >= m_pageH-1 ||
	   (top==0 && btm >= m_pageH-1)){
		m_rgn_top = m_rgn_btm = 0;
	}
	else{
		if(btm >= m_pageH)
			btm = m_pageH-1;
		m_rgn_top = top;
		m_rgn_btm = btm;
	}
}

void Screen_::ScrollPage(int n){
	if(n==0) return;
	int top,btm;
	if(m_rgn_btm){
		top = m_rgn_top;
		btm = m_rgn_btm+1;
	}
	else{
		top = 0;
		btm = m_pageH;
	}
	if(n > 0) scroll_up(top,btm, n);
	else      scroll_down(top,btm, -n);
	m_feed_next = false;
}
void Screen_::InsertLine(int n){
	int btm;
	if(m_rgn_btm){
		if(m_curY <= m_rgn_btm)
			btm = m_rgn_btm+1;
		else
			return;
	}
	else{
		btm = m_pageH;
	}
	scroll_down(m_curY, btm, (n<1)? 1: n);
	m_feed_next = false;
}
void Screen_::DeleteLine(int n){
	int btm;
	if(m_rgn_btm){
		if(m_curY <= m_rgn_btm)
			btm = m_rgn_btm+1;
		else
			return;
	}
	else{
		btm = m_pageH;
	}
	scroll_up(m_curY, btm, (n<1)? 1: n);
	m_feed_next = false;
}

void Screen_::Feed(){
	if(m_rgn_btm && m_rgn_btm == m_curY){
		scroll_up(m_rgn_top, m_rgn_btm+1, 1);
	}
	else if(m_rgn_btm && m_rgn_btm <= m_curY && m_curY == m_pageH-1){
	}
	else if(m_curY < m_pageH-1){
		m_curY++;
	}
	else{
		if(m_page_top < m_savelines){
			if(m_page_top == m_view_top){
				m_view_top++;//auto scroll
			}
		}else{
			if(m_page_top != m_view_top && m_view_top>0){
				m_view_top--;//adjust
			}
		}

		if(m_page_top < m_savelines){
			m_page_top++;
		}else{
			if(++m_head >= m_maxsize)
				m_head -= m_maxsize;
			if(m_sel_y >= 0)
				--m_sel_y;//adjust
			if(m_sel_btm >= 0){
				--m_sel_btm;//adjust
				if(--m_sel_top < 0)
					ClearSelection();
			}
		}

		Line& p = get_line(m_pageH-1);
		p.count = 0;
		_resize_line(p);
	}
	m_feed_next = false;
}

void Screen_::FeedRev(){
	if(!m_rgn_btm && m_curY == 0){
		scroll_down(0, m_pageH, 1);
	}
	else if(m_rgn_btm && m_curY == m_rgn_top){
		scroll_down(m_rgn_top, m_rgn_btm+1, 1);
	}
	else if(m_curY > 0){
		m_curY--;
	}
	m_feed_next = false;
}

void Screen_::InsertChar(int n){
	if(n<1) n=1;
	if(n > m_pageW - m_curX)
		n = m_pageW - m_curX;
	Line& p = get_line(m_curY);
	for(int i=m_pageW-1; i >= m_curX+n; i--)
		p.chars[i] = p.chars[i-n];
	clear_line(p, m_curX, m_curX+n);

	if(check_selection_x(m_curY, m_curX, m_pageW-1))
		ClearSelection();
}
void Screen_::DeleteChar(int n){
	if(n<1) n=1;
	if(n > m_pageW - m_curX)
		n = m_pageW - m_curX;
	Line& p = get_line(m_curY);
	for(int i=m_curX; i < m_pageW-n; i++)
		p.chars[i] = p.chars[i+n];
	clear_line(p, m_pageW-n, m_pageW);

	if(check_selection_x(m_curY, m_curX, m_pageW-1))
		ClearSelection();
}
void Screen_::EraseChar(int n){
	if(n<1) n=1;
	if(n > m_pageW - m_curX)
		n = m_pageW - m_curX;
	Line& p = get_line(m_curY);
	clear_line(p, m_curX, m_curX+n);

	if(check_selection_x(m_curY, m_curX, m_curX+n-1))
		ClearSelection();
}

//

Snapshot*  Screen_::GetSnapshot(bool rvideo, bool viscur){

	Snapshot* ss = (Snapshot*) new BYTE[ sizeof(Snapshot) + (sizeof(Char) * m_pageW * m_pageH) ];
	ss->width  = m_pageW;
	ss->height = m_pageH;
	ss->view   = m_view_top;
	ss->nlines = m_page_top+m_pageH;
	ss->cursor_x = -1;
	ss->cursor_y = -1;

	int viewy = m_view_top;
	int maxy = viewy + m_pageH;
	int pos = m_head + viewy;
	int i = 0;

	CharFlag inv = (CharFlag)(0);
	if(rvideo)
		inv = (CharFlag)(inv | CharFlag_Invert);
	if(m_sel_btm >= 0 && m_sel_top < viewy && viewy <= m_sel_btm)
		inv = (CharFlag)(inv | CharFlag_Select);

	for(; viewy < maxy ; viewy++, pos++){
		if(pos >= m_maxsize)
			pos -= m_maxsize;
		Line& line = m_buffer[pos];
		for(int x=0; x < m_pageW; x++, i++){
			if(inv & CharFlag_Select){
				if(m_sel_btm == viewy && m_sel_btmX==x)
					inv = (CharFlag)(inv & ~CharFlag_Select);
			}
			else{
				if(m_sel_top == viewy && m_sel_topX==x)
					inv = (CharFlag)(inv | CharFlag_Select);
			}
			if(x < line.count){
				ss->chars[i] = line.chars[x];
				ss->chars[i].flags = (CharFlag)(ss->chars[i].flags ^ inv);
			}
			else{
				ss->chars[i].ch = '\0';
				ss->chars[i].flags = (CharFlag)(CharFlag_Null ^ inv);
			}
		}
		if(viewy == m_sel_btm && m_pageW==m_sel_btmX){
			inv = (CharFlag)(inv & ~CharFlag_Select);
		}
	}

	if(viscur){
		i = m_page_top + m_curY;
		if(m_view_top <= i && i < maxy){
			ss->cursor_x = m_curX;
			ss->cursor_y = (i-m_view_top);
			i = (i-m_view_top) * m_pageW + m_curX;
			ss->chars[i].flags = (CharFlag)(ss->chars[i].flags | CharFlag_Cursor);
		}
	}

	return ss;
}

inline bool  is_break_char(UINT32 ucs){
	static const wchar_t  BREAK_CHARS[] = L"\"&()*,.-_/:;<=>@[\\]^`'{}~\x3000\x3001\x3002\x300C\x300D\x3010\x3011";

	if(ucs <= ' ')
		return true;

	for(int i=0; i < _countof(BREAK_CHARS); ++i){
		if(ucs == (UINT32) BREAK_CHARS[i])
			return true;
	}

	return false;
}

void Screen_::SetSelection(int view_x, int view_y, int mode){

	view_y += m_view_top;

	if(mode & 4){
		//click start
		m_sel_y = view_y;
		m_sel_x = view_x;
	}else{
		//mouse move
	}

	if(m_sel_y < view_y || (m_sel_y==view_y && m_sel_x < view_x)){
		m_sel_top  = m_sel_y;
		m_sel_topX = m_sel_x;
		m_sel_btm  = view_y;
		m_sel_btmX = view_x;
	}else{
		m_sel_top  = view_y;
		m_sel_topX = view_x;
		m_sel_btm  = m_sel_y;
		m_sel_btmX = m_sel_x;
	}

	int maxY = GetNumLines()-1;

	if(m_sel_top < 0)    m_sel_top = 0;
	if(m_sel_top > maxY) m_sel_top = maxY;

	if(m_sel_btm < 0)    m_sel_btm = 0;
	if(m_sel_btm > maxY) m_sel_btm = maxY;

	if(m_sel_topX < 0) m_sel_topX = 0;
	if(m_sel_topX >= m_pageW) m_sel_topX = m_pageW-1;

	if(m_sel_btmX < 0) m_sel_btmX = 0;
	if(m_sel_btmX >  m_pageW) m_sel_btmX = m_pageW;

	switch(mode & 3){
	case 2://expand line
		{
			m_sel_topX=0;
			m_sel_btmX=m_pageW;
		}
		break;
	case 1://expand word
		{
			const Line& p1 = get_line_glb(m_sel_top);
			if(m_sel_topX < p1.count){
				while(m_sel_topX > 0){
					const Char& c = p1.chars[m_sel_topX-1];
					if(is_break_char(c.ch))
						break;
					m_sel_topX--;
				}
			}
			const Line& p2 = get_line_glb(m_sel_btm);
			if(m_sel_btmX < p2.count){
				while(m_sel_btmX < m_pageW){
					const Char& c = p2.chars[m_sel_btmX];
					if(is_break_char(c.ch))
						break;
					m_sel_btmX++;
				}
			}
		}
		break;
	default://expand char
		{
			const Line& p1 = get_line_glb(m_sel_top);
			if(m_sel_topX < p1.count){
				if(m_sel_topX > 0){
					const Char& c = p1.chars[m_sel_topX-1];
					if(c.flags & CharFlag_MbTrail)
						m_sel_topX--;
				}
			}
			const Line& p2 = get_line_glb(m_sel_btm);
			if(m_sel_btmX < p2.count){
				if(m_sel_btmX < m_pageW){
					const Char& c = p2.chars[m_sel_btmX];
					if(c.flags & CharFlag_MbTrail)
						m_sel_btmX++;
				}
			}
		}
		break;
	}

	if(m_sel_top == m_sel_btm && m_sel_topX >= m_sel_btmX){
		ClearSelection();
	}
}


static void copy_chars(UINT32*& dst, const Line& p, int start, int max, bool addLF){
	if(max < start || max > p.count)
		max = p.count;
	while( max>start && p.chars[max-1].flags & CharFlag_Null )
		max--;
	for(; start < max ; start++){
		if(p.chars[start].flags & CharFlag_MbTrail){
		}
		else if(p.chars[start].ch >= ' '){
			*dst++ = p.chars[start].ch;
		}
		else{
			*dst++ = ' ';
		}
	}
	if(addLF && max < p.count){
		*dst++ = '\r';
		*dst++ = '\n';
	}
}

BSTR Screen_::GetSelection(){
	BSTR  result;
	if(m_sel_btm >= 0){
		UINT32*  wbuf;
		UINT32*  ws;
		if(m_sel_top < m_sel_btm){
			int nb = 1;
			int i = m_sel_top;
			for( ; i <= m_sel_btm; ++i){
				nb += get_line_glb(i).count + 2;
			}
			i = m_sel_top;
			ws = wbuf = new UINT32[ nb ];

			copy_chars(ws, get_line_glb(i), m_sel_topX, -1, true);
			for(++i; i < m_sel_btm; ++i){
				copy_chars(ws, get_line_glb(i), 0, -1, true);
			}
			copy_chars(ws, get_line_glb(i), 0, m_sel_btmX, false);
		}else{
			const Line&  line = get_line_glb(m_sel_top);
			ws = wbuf = new UINT32[ line.count+2 ];
			copy_chars(ws, line, m_sel_topX, m_sel_btmX, false);
		}

		result = Ck::Util::ucs4_to_bstr(wbuf, (UINT)(ws-wbuf));

		delete [] wbuf;
	}else{
		result = SysAllocStringLen(L"", 0);
	}
	return result;
}

}//namespace Ck
//EOF
