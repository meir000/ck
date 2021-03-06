﻿var Keys ={
	ShiftL:0x01000000,
	ShiftR:0x02000000,
	Shift :0x01000000 | 0x02000000,
	CtrlL :0x04000000,
	CtrlR :0x08000000,
	Ctrl  :0x04000000 | 0x08000000,
	AltL  :0x10000000,
	AltR  :0x20000000,
	Alt   :0x10000000 | 0x20000000,
	None:0,
	Back:8,
	Tab:9,
	Clear:12,
	Return:13,
	Pause:19,
	KanaMode:21,
	JunjaMode:23,
	FinalMode:24,
	HanjaMode:25,
	Escape:27,
	IMEConvert:28,
	IMENonconvert:29,
	IMEAceept:30,
	IMEModeChange:31,
	Space:32,
	PageUp:33,
	PageDown:34,
	End:35,
	Home:36,
	Left:37,
	Up:38,
	Right:39,
	Down:40,
	Select:41,
	Print:42,
	Execute:43,
	PrintScreen:44,
	Insert:45,
	Delete:46,
	Help:47,
	D0:48,
	D1:49,
	D2:50,
	D3:51,
	D4:52,
	D5:53,
	D6:54,
	D7:55,
	D8:56,
	D9:57,
	A:65,
	B:66,
	C:67,
	D:68,
	E:69,
	F:70,
	G:71,
	H:72,
	I:73,
	J:74,
	K:75,
	L:76,
	M:77,
	N:78,
	O:79,
	P:80,
	Q:81,
	R:82,
	S:83,
	T:84,
	U:85,
	V:86,
	W:87,
	X:88,
	Y:89,
	Z:90,
	LWin:91,
	RWin:92,
	Apps:93,
	NumPad0:96,
	NumPad1:97,
	NumPad2:98,
	NumPad3:99,
	NumPad4:100,
	NumPad5:101,
	NumPad6:102,
	NumPad7:103,
	NumPad8:104,
	NumPad9:105,
	Multiply:106,
	Add:107,
	Separator:108,
	Subtract:109,
	Decimal:110,
	Divide:111,
	F1:112,
	F2:113,
	F3:114,
	F4:115,
	F5:116,
	F6:117,
	F7:118,
	F8:119,
	F9:120,
	F10:121,
	F11:122,
	F12:123,
	F13:124,
	F14:125,
	F15:126,
	F16:127,
	F17:128,
	F18:129,
	F19:130,
	F20:131,
	F21:132,
	F22:133,
	F23:134,
	F24:135,
	Scroll:145,
	BrowserBack:166,
	BrowserForward:167,
	BrowserRefresh:168,
	BrowserStop:169,
	BrowserSearch:170,
	BrowserFavorites:171,
	BrowserHome:172,
	VolumeMute:173,
	VolumeDown:174,
	VolumeUp:175,
	MediaNextTrack:176,
	MediaPreviousTrack:177,
	MediaStop:178,
	MediaPlayPause:179,
	LaunchMail:180,
	SelectMedia:181,
	LaunchApplication1:182,
	LaunchApplication2:183,
	Oem1:186,
	Oemplus:187,
	Oemcomma:188,
	OemMinus:189,
	OemPeriod:190,
	OemQuestion:191,
	Oemtilde:192,
	OemOpenBrackets:219,
	Oem5:220,
	Oem6:221,
	Oem7:222,
	Oem8:223,
	OemBackslash:226,
	ProcessKey:229,
	Packet:231,
	Attn:246,
	Crsel:247,
	Exsel:248,
	EraseEof:249,
	Play:250,
	Zoom:251,
	OemClear:254
};

var Encoding ={
	EUCJP	:0x01,
	SJIS	:0x02,
	UTF8	:0x04
};

var Priv ={
	Backspace	:0x00000100,
	ScrollTtyOut	:0x00002000,
	ScrollTtyKey	:0x00004000,
	UseBell		:0x20000000,
	CjkWidth	:0x40000000
};

var WinZOrder ={
	Normal	:0,
	Top	:1,
	Bottom	:2
};

var MouseCmd ={
	None	:0,
	Select	:1,
	Paste	:2,
	Menu	:3
};

var Place ={
	Scale	:0,
	Zoom	:1,
	Contain	:1,
	Cover	:2,
	Repeat	:3,
	NoRepeat:4
};

var Align ={
	Near	:0,
	Center	:1,
	Far	:2
};

function CONFIG(){
	this.tty={
		execute_command	:"/bin/bash --login -i",
		title		:"ck",
		savelines	:5000,
		input_encoding	:Encoding.UTF8,
		display_encoding:Encoding.UTF8 | Encoding.SJIS | Encoding.EUCJP,
		scroll_key	:1,
		scroll_output	:0,
		bs_as_del	:0,
		use_bell	:0,
		cjk_width	:1
	};
	this.accelkey={
		new_shell	:Keys.ShiftL | Keys.CtrlL | Keys.N,
		new_window	:Keys.ShiftL | Keys.CtrlL | Keys.M,
		open_window	:Keys.ShiftL | Keys.CtrlL | Keys.O,
		close_window	:Keys.ShiftL | Keys.CtrlL | Keys.W,
		next_shell	:Keys.CtrlL  | Keys.Tab,
		prev_shell	:Keys.ShiftL | Keys.CtrlL | Keys.Tab,
		paste		:Keys.ShiftL | Keys.Insert,
		popup_menu	:Keys.ShiftL | Keys.F10,
		popup_sys_menu	:Keys.AltR   | Keys.Space,
		scroll_page_up	:Keys.ShiftL | Keys.PageUp,
		scroll_page_down:Keys.ShiftL | Keys.PageDown,
		scroll_line_up	:Keys.None,
		scroll_line_down:Keys.None,
		scroll_top	:Keys.ShiftL | Keys.Home,
		scroll_bottom	:Keys.ShiftL | Keys.End
	};
	this.window={
		position_x	:null,
		position_y	:null,
		cols		:80,
		rows		:24,
		scrollbar_show	:1,
		scrollbar_right	:1,
		blink_cursor	:1,
		zorder		:WinZOrder.Normal,
		linespace	:0,
		border_left	:4,
		border_top	:1,
		border_right	:0,
		border_bottom	:4,
		mouse_left	:MouseCmd.Select,
		mouse_middle	:MouseCmd.Paste,
		mouse_right	:MouseCmd.Menu,
		font_name	:"",
		font_size	:10.5,
		background_file	:"",
		background_repeat_x	:Place.Contain,
		background_repeat_y	:Place.Contain,
		background_align_x	:Align.Center,
		background_align_y	:Align.Center,
		color_foreground	:0x000000,
		color_background	:0xFFFFFFFF,
		color_selection		:0x663297FD,
		color_cursor		:0x00AA00,
		color_imecursor		:0xAA0000,
		color_scrollbar		:0x888888,
		color_alpha		:0xCC
	};
}
CONFIG.prototype ={
	copyTo_pty : function(pty){
		var n = pty.PrivMode;
		n = (this.tty.scroll_key)    ? (n | Priv.ScrollTtyKey) : (n & ~Priv.ScrollTtyKey);
		n = (this.tty.scroll_output) ? (n | Priv.ScrollTtyOut) : (n & ~Priv.ScrollTtyOut);
		n = (this.tty.bs_as_del)     ? (n | Priv.Backspace)  : (n & ~Priv.Backspace);
		n = (this.tty.use_bell)      ? (n | Priv.UseBell)    : (n & ~Priv.UseBell);
		n = (this.tty.cjk_width)     ? (n | Priv.CjkWidth)   : (n & ~Priv.CjkWidth);
		pty.PrivMode = n;
		pty.InputEncoding = this.tty.input_encoding;
		pty.DisplayEncoding = this.tty.display_encoding;
		pty.Savelines = this.tty.savelines;
		pty.Title = this.tty.title;
	},
	copyFrom_pty : function(pty){
		var n = pty.PrivMode;
		this.tty.scroll_key    = (n & Priv.ScrollTtyKey) ? 1 : 0;
		this.tty.scroll_output = (n & Priv.ScrollTtyOut) ? 1 : 0;
		this.tty.bs_as_del     = (n & Priv.Backspace)    ? 1 : 0;
		this.tty.use_bell      = (n & Priv.UseBell)      ? 1 : 0;
		this.tty.cjk_width     = (n & Priv.CjkWidth)     ? 1 : 0;
		this.tty.input_encoding = pty.InputEncoding;
		this.tty.display_encoding = pty.DisplayEncoding;
		this.tty.savelines = pty.Savelines;
		this.tty.workdir = pty.CurrentDirectory;
	},
	copyTo_window : function(window){
		for(var i=0; i <= 15; i++){
			var v = this.window["color_color"+i];
			if(v != null) window.Color(i) = v;
		}
		window.Color(256) = this.window.color_foreground;
		window.Color(257) = this.window.color_background;
		window.Color(258) = this.window.color_selection;
		window.Color(259) = this.window.color_cursor;
		window.Color(260) = this.window.color_imecursor;
		window.Color(261) = this.window.color_scrollbar;
		window.ColorAlpha = this.window.color_alpha;
		window.FontName = this.window.font_name;
		window.FontSize = this.window.font_size;
		window.BackgroundPlace =
			(this.window.background_repeat_y << 24) |
			(this.window.background_align_y  << 16) |
			(this.window.background_repeat_x << 8 ) |
			(this.window.background_align_x  << 0 );
		if(this.window.background_file != null &&
		   this.window.background_file != ""){
			app.trace("[ImageFile] \"" + this.window.background_file + "\"\n");
			window.BackgroundImage = this.window.background_file;
		}
		window.Border =
			(this.window.border_left   << 24) |
			(this.window.border_top    << 16) |
			(this.window.border_right  << 8) |
			(this.window.border_bottom << 0);
		window.LineSpace = this.window.linespace;
		window.ZOrder = this.window.zorder;
		window.Scrollbar = (this.window.scrollbar_show ? 1 : 0) | (this.window.scrollbar_right ? 0 : 2);
		window.BlinkCursor = this.window.blink_cursor;
		window.MouseLBtn = this.window.mouse_left;
		window.MouseMBtn = this.window.mouse_middle;
		window.MouseRBtn = this.window.mouse_right;
		window.Resize(this.window.cols, this.window.rows);
		if(this.window.position_x != null)
			window.Move(this.window.position_x, this.window.position_y);
	},
	copyFrom_window : function(window){
		for(var i=0; i <= 15; i++)
			this.window["color_color"+i] = window.Color(i);
		this.window.color_foreground = window.Color(256);
		this.window.color_background = window.Color(257);
		this.window.color_selection = window.Color(258);
		this.window.color_cursor = window.Color(259);
		this.window.color_imecursor = window.Color(260);
		this.window.color_scrollbar = window.Color(261);
		this.window.color_alpha = window.ColorAlpha;
		this.window.font_name = window.FontName;
		this.window.font_size = window.FontSize;
		var n = window.BackgroundPlace;
		this.window.background_repeat_y = (n>>24)&0xff;
		this.window.background_align_y  = (n>>16)&0xff;
		this.window.background_repeat_x = (n>>8 )&0xff;
		this.window.background_align_x  = (n>>0 )&0xff;
		this.window.background_file = window.BackgroundImage;
		n = window.Border;
		this.window.border_left   = (n>>24)&0xff;
		this.window.border_top    = (n>>16)&0xff;
		this.window.border_right  = (n>>8 )&0xff;
		this.window.border_bottom = (n>>0 )&0xff;
		this.window.linespace = window.LineSpace;
		this.window.zorder = window.ZOrder;
		n = window.Scrollbar;
		this.window.scrollbar_show  = (n&1) ? 1 : 0;
		this.window.scrollbar_right = (n&2) ? 0 : 1;
		this.window.blink_cursor = window.BlinkCursor;
		this.window.mouse_left   = window.MouseLBtn;
		this.window.mouse_middle = window.MouseMBtn;
		this.window.mouse_right  = window.MouseRBtn;
	},
	copyFrom_args : function(opt){
		if(opt.fg != null) this.window.color_foreground = opt.fg;
		if(opt.bg != null) this.window.color_background = opt.bg;
		if(opt.cr != null) this.window.color_cursor = opt.cr;
		if(opt.bgbmp != null) this.window.background_file = opt.bgbmp;
		if(opt.fn != null) this.window.font_name = opt.fn;
		if(opt.fs != null) this.window.font_size = opt.fs;
		if(opt.lsp != null) this.window.linespace = opt.lsp;
		if(opt.sb != null) this.window.scrollbar_show = opt.sb ? 1 : 0;
		if(opt.posx != null) this.window.position_x = opt.posx;
		if(opt.posy != null) this.window.position_y = opt.posy;
		if(opt.cols != null) this.window.cols = opt.cols;
		if(opt.rows != null) this.window.rows = opt.rows;
		if(opt.si != null) this.tty.scroll_output = opt.si;
		if(opt.sk != null) this.tty.scroll_key = opt.sk;
		if(opt.sl != null) this.tty.savelines = opt.sl;
		if(opt.bs != null) this.tty.bs_as_del = opt.bs;
		if(opt.cjk != null)this.tty.cjk_width = opt.cjk;
		if(opt.km != null) this.tty.input_encoding = opt.km;
		if(opt.md != null) this.tty.display_encoding = opt.md;
		if(opt.cmd != null)this.tty.execute_command = opt.cmd;
		if(opt.title != null)this.tty.title = opt.title;
		if(opt.workdir != null)this.tty.workdir = opt.workdir;
	},
	clone : function(){
		var n = new CONFIG();
		for(var p in this.tty)      n.tty[p] = this.tty[p];
		for(var p in this.window)   n.window[p] = this.window[p];
		for(var p in this.accelkey) n.accelkey[p] = this.accelkey[p];
		return n;
	}
};

function ARGS(vbarr){
	var args  = new VBArray(vbarr).toArray();
	var count = args.length;
	for(var i=1; i < count; ++i){
		var s = args[i];
		if(s=="-share"){}
		else if(s=="-fg")   { if(++i < count) Helper.parse_color(this,'fg',args[i]);}
		else if(s=="-bg")   { if(++i < count) Helper.parse_color(this,'bg',args[i]);}
		else if(s=="-cr")   { if(++i < count) Helper.parse_color(this,'cr',args[i]);}
		else if(s=="-bgbmp"){ if(++i < count) this.bgbmp = args[i];}
		else if(s=="-fn")   { if(++i < count) this.fn = args[i];}
		else if(s=="-fs")   { if(++i < count) Helper.parse_flt(this,'fs', args[i], 8.0, 72.0);}
		else if(s=="-lsp")  { if(++i < count) Helper.parse_int(this,'lsp',args[i], -100, +100);}
		else if(s=="-sl")   { if(++i < count) Helper.parse_int(this,'sl', args[i], 0, 10000);}
		else if(s=="-si"  || s=="+si"){ this.si = (s.charAt(0)=='-') ? 1 : 0;}
		else if(s=="-sk"  || s=="+sk"){ this.sk = (s.charAt(0)=='-') ? 1 : 0;}
		else if(s=="-sb"  || s=="+sb"){ this.sb = (s.charAt(0)=='-') ? 1 : 0;}
		else if(s=="-bs"  || s=="+bs"){ this.bs = (s.charAt(0)=='-') ? 1 : 0;}
		else if(s=="-cjk" || s=="+cjk"){ this.cjk = (s.charAt(0)=='-') ? 1 : 0;}
		else if(s=="-km")   { if(++i < count) this.km = Helper.get_encoding(args[i]);}
		else if(s=="-md")   { if(++i < count) this.md = Helper.get_encoding(args[i]);}
		else if(s=="-title"){ if(++i < count) this.title   = args[i];}
		else if(s=="-f")    { if(++i < count) this.script  = args[i];}
		else if(s=="-dir")  { if(++i < count) this.workdir = args[i];}
		else if(s=="-g"){
			if(++i < count){
				if(args[i].match(/^(\d+)x(\d+)([\+\-])(\d+)([\+\-])(\d+)/)){
					s = parseInt(RegExp.$4);
					this.posx = (RegExp.$3 == '-') ? -s-1 : s;
					s = parseInt(RegExp.$6);
					this.posy = (RegExp.$5 == '-') ? -s-1 : s;
					this.cols = parseInt(RegExp.$1);
					this.rows = parseInt(RegExp.$2);
				}
				else if(args[i].match(/^(\d+)x(\d+)/)){
					this.cols = parseInt(RegExp.$1);
					this.rows = parseInt(RegExp.$2);
				}
			}
		}
		else if(s=="-e"){
			if(++i < count){
				this.cmd = "";
				while(i < count){
					this.cmd += "\"" + args[i++] + "\" ";
				}
			}
		}
	}
}

function ACCEL(key,func,menu){
	this.key  = key;
	this.exec = func;
	this.menu = menu;
}
ACCEL.prototype ={
	checkmodifier : function(input, mask){
		var a = (this.key & mask) != 0;
		var b = (input    & mask) != 0;
		return a==b;
	},
	match : function(input){
		return (this.key & 0x00FFFFFF) == (input & 0x00FFFFFF) &&
			this.checkmodifier(input, Keys.Ctrl ) &&
			this.checkmodifier(input, Keys.Shift) &&
			this.checkmodifier(input, Keys.Alt  );
	}
};



//------------------

var HomeDir = app.Env("HOME"); //$HOME is set with cygwin_dll_init()

var Config = new CONFIG();

var PtyList = [];
var WndList = [];
var AccelKeys = [];

AccelKeys.add = function(key, cmd, menu){
	if(cmd != null && key != null && (key != Keys.None || menu != null))
		this.push(new ACCEL(key, cmd, menu));
}

//------------------

var Helper ={
	parse_flt : function(obj, prop, arg, min, max){
		var value = parseFloat(arg);
		if(!isNaN(value)){
			obj[prop] = (value<min) ? min : (value<max) ? value : max;
		}
	},
	parse_int : function(obj, prop, arg, min, max){
		var value = parseInt(arg);
		if(!isNaN(value)){
			obj[prop] = (value<min) ? min : (value<max) ? value : max;
		}
	},
	parse_color : function(obj, prop, arg){
		var value = parseInt(arg,16);
		if(!isNaN(value)){
			obj[prop] = (value<0) ? 0 : (value<0xFFFFFFFF) ? value : 0xFFFFFFFF;
		}
	},
	set_input_encoding : function(window, bit){
		var p = window.Pty;
		if(p) p.InputEncoding = bit;
	},
	toggle_display_encoding : function(window, bit){
		var p = window.Pty;
		if(p){
			var e = p.DisplayEncoding ^ bit;
			if(e) p.DisplayEncoding = e;
		}
	},
	toggle_priv_mode : function(window, bit){
		var p = window.Pty;
		if(p) p.PrivMode = p.PrivMode ^ bit;
	},
	scroll_absolute : function(window, n){
		var p = window.Pty;
		if(p) p.ViewPos = n;
	},
	scroll_relative : function(window, page, line){
		var p = window.Pty;
		if(p){
			n = (p.ViewPos) + (p.PageHeight * page) + (line);
			if(n<0) n=0;
			p.ViewPos = n;
		}
	},
	get_encoding : function(str){
		var ar = str.replace(/\s/, '').split(',');
		var n = 0;
		for(var i=0; i < ar.length; i++){
			if(ar[i]=="sjis" || ar[i]=="shiftjis")   n |= Encoding.SJIS;
			else if(ar[i]=="eucj" || ar[i]=="eucjp") n |= Encoding.EUCJP;
			else if(ar[i]=="utf8" || ar[i]=="utf-8") n |= Encoding.UTF8;
		}
		return (n != 0) ? n : null;
	},
	get_active_window : function(){
		var active = app.ActiveWindow;
		for(var i= WndList.length-1; i >= 0; --i){
			var w = WndList[i];
			if(w.Handle == active)
				return w;
		}
		return null;
	},
	create_shell : function(cfg){
		if(cfg.tty.workdir != null)
			app.CurrentDirectory = cfg.tty.workdir;

		var n = app.NewPty( cfg.tty.execute_command );
		PtyList.push(n);

		if(cfg.tty.workdir != null)
			app.CurrentDirectory = HomeDir;

		cfg.copyTo_pty(n);

		for(var i=WndList.length-1; i >= 0; --i){
			Events.wnd_on_title_init( WndList[i] );
		}
		return n;
	},
	create_window : function(cfg){
		var w = app.NewWnd();
		WndList.push(w);

		cfg.copyTo_window(w);
		return w;
	}
};

var Commands ={
	wnd_next_shell : function(window){
		var numpty = PtyList.length;
		var pty = window.Pty;
		for(var i=0; i < numpty; ++i){
			if(PtyList[i] == pty){
				if(++i >= numpty) i=0;
				window.Pty = PtyList[i];
				break;
			}
		}
		return true;
	},
	wnd_prev_shell : function(window){
		var numpty = PtyList.length;
		var pty = window.Pty;
		for(var i=0; i < numpty; ++i){
			if(PtyList[i] == pty){
				if(--i < 0) i=numpty-1;
				window.Pty = PtyList[i];
				break;
			}
		}
		return true;
	},
	wnd_new_shell : function(window){
		var cfg = Config.clone();
		if(window != null){
			var pty = window.Pty;
			if(pty != null){
				cfg.copyFrom_pty(pty);
			}
		}
		window.Pty = Helper.create_shell(cfg);
		return true;
	},
	wnd_new_window : function(window){
		var cfg = Config.clone();
		if(window != null){
			var pty = window.Pty;
			if(pty != null){
				cfg.copyFrom_pty(pty);
			}
		}
		cfg.copyFrom_window(window);
		var w = Helper.create_window(cfg);
		var p = Helper.create_shell(cfg);
		w.Pty = p;
		w.Show();
		return true;
	},
	wnd_open_window : function(window){
		var cfg = Config.clone();
		cfg.copyFrom_window(window);
		var w = Helper.create_window(cfg);
		w.Pty = PtyList[0];
		w.Show();
		return true;
	},
	wnd_close_window : function(window){
		window.Close();
		return true;
	},
	tty_paste : function(window){
		var p = window.Pty;
		if(p){
			var text = app.Clipboard;
			if(text.length > 0){
				text = text.replace(/[\x00-\x08\x0A-\x0C\x0E-\x1F]/g, '').replace(/\x0D/g, '\n');
				p.PutString(text);
			}
		}
		return true;
	},
	tty_reset : function(window){
		var p = window.Pty;
		if(p){
			p.Reset(true);
		}
		return true;
	},
	tty_scroll_page_up : function(window)		{ Helper.scroll_relative(window, -1, 0); return true;},
	tty_scroll_page_down : function(window)		{ Helper.scroll_relative(window, +1, 0); return true;},
	tty_scroll_line_up : function(window)		{ Helper.scroll_relative(window, 0, -3); return true;},
	tty_scroll_line_down : function(window)		{ Helper.scroll_relative(window, 0, +3); return true;},
	tty_scroll_top : function(window)		{ Helper.scroll_absolute(window, 0); return true;},
	tty_scroll_bottom : function(window)		{ Helper.scroll_absolute(window, -1); return true;},
	tty_enc_set_input_sjis : function(window)	{ Helper.set_input_encoding(window, Encoding.SJIS); return true;},
	tty_enc_set_input_eucj : function(window)	{ Helper.set_input_encoding(window, Encoding.EUCJP); return true;},
	tty_enc_set_input_utf8 : function(window)	{ Helper.set_input_encoding(window, Encoding.UTF8); return true;},
	tty_enc_toggle_display_sjis : function(window)	{ Helper.toggle_display_encoding(window, Encoding.SJIS); return true;},
	tty_enc_toggle_display_eucj : function(window)	{ Helper.toggle_display_encoding(window, Encoding.EUCJP); return true;},
	tty_enc_toggle_display_utf8 : function(window)	{ Helper.toggle_display_encoding(window, Encoding.UTF8); return true;},
	tty_toggle_scroll_key : function(window)	{ Helper.toggle_priv_mode(window, Priv.ScrollTtyKey); return true;},
	tty_toggle_scroll_out : function(window)	{ Helper.toggle_priv_mode(window, Priv.ScrollTtyOut); return true;},
	tty_toggle_bs_as_del : function(window)		{ Helper.toggle_priv_mode(window, Priv.Backspace); return true;},
	tty_toggle_cjk_width : function(window)		{ Helper.toggle_priv_mode(window, Priv.CjkWidth); return true;},
	wnd_menu : function(window)			{ window.PopupMenu(false); return true;},
	wnd_sys_menu : function(window)			{ window.PopupMenu(true); return true;},
	wnd_toggle_scrollbar : function(window)		{ window.Scrollbar = window.Scrollbar ^ 1; return true;},
	wnd_set_zorder_normal : function(window)	{ window.ZOrder = WinZOrder.Normal; return true;},
	wnd_set_zorder_top : function(window)		{ window.ZOrder = WinZOrder.Top;    return true;},
	wnd_set_zorder_bottom : function(window)	{ window.ZOrder = WinZOrder.Bottom; return true;},
	wnd_inc_fontsize : function(window)		{ window.FontSize+=1.0; return true;},
	wnd_dec_fontsize : function(window)		{ window.FontSize-=1.0; return true;},
	app_toggle_console : function(window)		{ app.ShowConsole = app.ShowConsole ? false : true; return true;}
};


var Events ={
	wnd_on_closed : function(window){
		for(var i=WndList.length-1; i >= 0; --i){
			if(WndList[i] == window){
				WndList.splice(i,1);
				window.Dispose();
				break;
			}
		}
		if(WndList.length == 0){
			for(var i=PtyList.length-1; i >= 0; --i){
				PtyList.pop().Dispose();
			}
			app.Quit();
		}
	},
	wnd_on_key_down : function(window, key){
		for(var i=0; i < AccelKeys.length; ++i){
			if(AccelKeys[i].match(key) && AccelKeys[i].exec(window))
				return;
		}
		var p = window.Pty;
		if(p) p.PutKeyboard(key);
	},
	wnd_on_mouse_wheel : function(window, key, delta){
		if(key & Keys.Ctrl){
			window.FontSize += delta * 1.0;
		}
		else{
			delta = (key & Keys.Alt) ? (delta*1) : (delta*5);
			Helper.scroll_relative(window, 0, -delta);
		}
	},
	wnd_on_menu_init : function(window){
		var pty = window.Pty;
		{//category 0xA00
			var count=0;
			for(var i=0; i < AccelKeys.length; ++i){
				if(AccelKeys[i].menu){
					app.AddMenu(0xA00+i, AccelKeys[i].menu, false);
					count++;
				}
			}
			if(count>0){
				app.AddMenuSeparator();
			}
		}
		{//category 0xB00
			for(var i=0; i < PtyList.length; ++i){
				var p = PtyList[i];
				app.AddMenu(0xB00+i, (i+1)+" "+p.Title, (p==pty));
			}
			if(PtyList.length > 0){
				app.AddMenuSeparator();
			}
		}
		{//category 0xC00
			var ienc = 0;
			var denc = 0;
			var priv = 0;
			var zorder = window.ZOrder;
			if(pty){
				ienc = pty.InputEncoding;
				denc = pty.DisplayEncoding;
				priv = pty.PrivMode;
			}
			app.AddMenu(0xC00+0,  "&Input ShiftJIS",   ienc == Encoding.SJIS);
			app.AddMenu(0xC00+1,  "&Input EUC-JP",     ienc == Encoding.EUCJP);
			app.AddMenu(0xC00+2,  "&Input UTF8",       ienc == Encoding.UTF8);
			app.AddMenu(0xC00+3,  "&Display ShiftJIS", denc & Encoding.SJIS);
			app.AddMenu(0xC00+4,  "&Display EUC-JP",   denc & Encoding.EUCJP);
			app.AddMenu(0xC00+5,  "&Display UTF8",     denc & Encoding.UTF8);
			app.AddMenuSeparator();
			app.AddMenu(0xC00+20, "&Tty: Scroll Keypress", priv & Priv.ScrollTtyKey);
			app.AddMenu(0xC00+21, "&Tty: Scroll Output",   priv & Priv.ScrollTtyOut);
			app.AddMenu(0xC00+22, "&Tty: BS as DEL",       priv & Priv.Backspace);
			app.AddMenu(0xC00+23, "&Tty: CJK Width",       priv & Priv.CjkWidth);
			app.AddMenu(0xC00+24, "&Tty: Reset", false);
			app.AddMenuSeparator();
			app.AddMenu(0xC00+40, "&Win: Scrollbar",     window.Scrollbar & 1);
			app.AddMenu(0xC00+41, "&Win: ZOrder Normal", zorder==WinZOrder.Normal);
			app.AddMenu(0xC00+42, "&Win: ZOrder Top",    zorder==WinZOrder.Top);
			app.AddMenu(0xC00+43, "&Win: ZOrder Bottom", zorder==WinZOrder.Bottom);
			app.AddMenu(0xC00+44, "&Win: FontSize+", false);
			app.AddMenu(0xC00+45, "&Win: FontSize-", false);
			app.AddMenuSeparator();
			app.AddMenu(0xC00+60, "Show Console", app.ShowConsole);
		}
	},
	wnd_on_menu_execute : function(window, id){
		var category = id & 0xF00;
		var index    = id & 0x0FF;
		if(category == 0xA00){
			AccelKeys[index].exec(window);
		}
		else if(category == 0xB00){
			if(index < PtyList.length){
				window.Pty = PtyList[index];
			}
		}
		else if(category == 0xC00){
			switch(index){
			case 0:  Commands.tty_enc_set_input_sjis(window);break;
			case 1:  Commands.tty_enc_set_input_eucj(window);break;
			case 2:  Commands.tty_enc_set_input_utf8(window);break;
			case 3:  Commands.tty_enc_toggle_display_sjis(window);break;
			case 4:  Commands.tty_enc_toggle_display_eucj(window);break;
			case 5:  Commands.tty_enc_toggle_display_utf8(window);break;
			case 20: Commands.tty_toggle_scroll_key(window);break;
			case 21: Commands.tty_toggle_scroll_out(window);break;
			case 22: Commands.tty_toggle_bs_as_del(window);break;
			case 23: Commands.tty_toggle_cjk_width(window);break;
			case 24: Commands.tty_reset(window);break;
			case 40: Commands.wnd_toggle_scrollbar(window);break;
			case 41: Commands.wnd_set_zorder_normal(window);break;
			case 42: Commands.wnd_set_zorder_top(window);break;
			case 43: Commands.wnd_set_zorder_bottom(window);break;
			case 44: Commands.wnd_inc_fontsize(window);break;
			case 45: Commands.wnd_dec_fontsize(window);break;
			case 60: Commands.app_toggle_console(window);break;
			}
		}
	},
	wnd_on_title_init : function(window){
		var pty = window.Pty;
		var title = (pty) ? pty.Title : "(null)";
		var numpty = PtyList.length;
		if(numpty > 1){
			var i = 0;
			for(; i<numpty; i++){
				if(PtyList[i] == pty)
					break;
			}
			title = "[" + (i+1) + "/" + (numpty) + "] " + title;
		}
		window.Title = title;
	},
	wnd_on_drop : function(window, path, type, key){
		var pty = window.Pty;
		if(pty){
			if(type==1){//file path
				if(key & 0x02){//MK_RBUTTON
					path = app.ToCygwinPath(path);
					path = path.replace(/[\\ !$&,;<=>@`\"\'\*\|\?\^\(\)\[\]\{\}]/g, function(s){ return '\\'+s; }) + ' ';
				}else{
					path = '"' + path + '" ';
				}
			}else{//simple text
			}
			pty.PutString(path);
		}
	},
	wnd_on_paste_clipboard : function(window){
		Commands.tty_paste(window);
	},
	pty_on_closed : function(pty){
		for(var i=WndList.length-1; i >= 0; --i){
			var w = WndList[i];
			if(w.Pty == pty){
				w.Pty = null;
			}
		}
		for(var i=PtyList.length-1; i >= 0; --i){
			var p = PtyList[i];
			if(p == pty){
				PtyList.splice(i,1);
				p.Dispose();
				break;
			}
		}
		for(var i=0; i < WndList.length; ++i){
			var w = WndList[i];
			if(w.Pty == null){
				if(WndList.length >= 2 || PtyList.length <= 0){
					w.Close();
					continue;
				}
				w.Pty = PtyList[0];
			}
			Events.wnd_on_title_init(w);
		}
	},
	pty_on_update_title : function(pty){
		for(var i=WndList.length-1; i >= 0; --i){
			var w = WndList[i];
			if(w.Pty == pty){
				Events.wnd_on_title_init(w);
			}
		}
	},
	pty_on_req_font : function(pty, id){
		app.trace("pty_on_req_font " + id + "\n");
	},
	pty_on_req_move : function(pty, x, y){
		for(var i=WndList.length-1; i >= 0; --i){
			var w = WndList[i];
			if(w.Pty == pty){
				w.Move(x,y);
			}
		}
	},
	pty_on_req_resize : function(pty, x, y){
		for(var i=WndList.length-1; i >= 0; --i){
			var w = WndList[i];
			if(w.Pty == pty){
				w.Resize(x,y);
			}
		}
	},
	app_on_new_command : function(_args, _workdir){
		var opt = new ARGS(_args);
		var cfg = Config.clone();
		cfg.tty.workdir = _workdir;
		cfg.copyFrom_args(opt);

		var p = Helper.create_shell(cfg);

		//var act = Helper.get_active_window();
		//if(act != null){ act.Pty = p; return;}

		var w = Helper.create_window(cfg);
		w.Pty = p;
		w.Show();
	},
	app_on_startup : function(_args){
		{//env
			var ar = [];
			var uniq = {};
			var old = app.Env("PATH").split(":");
			for(var i=0; i < old.length; i++){
				var path = old[i];
				if(!uniq[path]){
					uniq[path] = 1;
					ar.push(path);
				}
			}
			//set
			app.Env("PATH") = ar.join(":");
			app.Env("TERM") = "xterm";
			app.Env("COLORTERM") = "xterm";
			app.Env("CHERE_INVOKING") = "1";
		}

		//args
		var opt = new ARGS(_args);

		//user script
		try{
			var path = (opt.script != null) ? opt.script : HomeDir+"/.ck.config.js";
			app.trace("[HomeDir] \"" + HomeDir + "\"\n");
			app.trace("[UserConfig] \"" + path + "\"\n");
			app.EvalFile(path);
		}catch(e){}

		//shortcut key
		AccelKeys.add(Config.accelkey.new_shell,	Commands.wnd_new_shell,		"&New Shell");
		AccelKeys.add(Config.accelkey.new_window,	Commands.wnd_new_window,	"&New Shell in New Window");
		AccelKeys.add(Config.accelkey.open_window,	Commands.wnd_open_window,	"&Open Window");
		AccelKeys.add(Config.accelkey.close_window,	Commands.wnd_close_window,	"Close &Window");
		AccelKeys.add(Config.accelkey.next_shell,	Commands.wnd_next_shell,	null);
		AccelKeys.add(Config.accelkey.prev_shell,	Commands.wnd_prev_shell,	null);
		AccelKeys.add(Config.accelkey.scroll_page_up,	Commands.tty_scroll_page_up,	null);
		AccelKeys.add(Config.accelkey.scroll_page_down,	Commands.tty_scroll_page_down,	null);
		AccelKeys.add(Config.accelkey.scroll_line_up,	Commands.tty_scroll_line_up,	null);
		AccelKeys.add(Config.accelkey.scroll_line_down,	Commands.tty_scroll_line_down,	null);
		AccelKeys.add(Config.accelkey.scroll_top,	Commands.tty_scroll_top,	null);
		AccelKeys.add(Config.accelkey.scroll_bottom,	Commands.tty_scroll_bottom,	null);
		AccelKeys.add(Config.accelkey.paste,		Commands.tty_paste,		"&Paste");
		AccelKeys.add(Config.accelkey.popup_menu,	Commands.wnd_menu,		null);
		AccelKeys.add(Config.accelkey.popup_sys_menu,	Commands.wnd_sys_menu,		null);

		//dup, set
		var cfg = Config.clone();
		cfg.copyFrom_args(opt);

		var w = Helper.create_window(cfg);
		var p = Helper.create_shell(cfg);
		w.Pty = p;
		w.Show();

		app.CurrentDirectory = HomeDir;
	}
};

//EOF
