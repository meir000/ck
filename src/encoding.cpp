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
#include <WinNls.h>

//#import  "interface.tlb" raw_interfaces_only
#include "interface.tlh"
#include "encoding.h"
#pragma comment(lib, "Normaliz.lib")


namespace Ck{
namespace Enc{

Encoding  detect_encoding(const BYTE* src, int srclen, Encoding mask){
	if(mask==Encoding_EUCJP ||
	   mask==Encoding_SJIS ||
	   mask==Encoding_UTF8)
		return mask;

	int sjis_count = 0;
	int utf8_count = 0;

	if((mask & Encoding_SJIS) && (mask & Encoding_EUCJP)){
		for(int i=0; i < srclen; i++){
			if((src[i] & 0x80) && (i+1 < srclen)){
				if(eucj_to_ucs((src[i]<<8) | src[i+1]))
					sjis_count--;
				else
					sjis_count++;
			}
		}
	}
	if(mask & Encoding_UTF8){//utf8
		for(int i=0; i < srclen; ){
			if(src[i] & 0x80){
				if((src[i] & 0xE0)==0xC0){
					if(i+1 >= srclen) break;
					if((src[i+1] & 0xC0)==0x80){
						utf8_count++;
						i+=2;
						continue;
					}
				}
				else if((src[i] & 0xF0)==0xE0){
					if(i+2 >= srclen) break;
					if((src[i+1] & 0xC0)==0x80 &&
					   (src[i+2] & 0xC0)==0x80){
						utf8_count++;
						i+=3;
						continue;
					}
				}
				else if((src[i] & 0xF8)==0xF0){
					if(i+3 >= srclen) break;
					if((src[i+1] & 0xC0)==0x80 &&
					   (src[i+2] & 0xC0)==0x80 &&
					   (src[i+3] & 0xC0)==0x80){
						utf8_count++;
						i+=4;
						continue;
					}
				}
				else if((src[i] & 0xFC)==0xF8){
					if(i+4 >= srclen) break;
					if((src[i+1] & 0xC0)==0x80 &&
					   (src[i+2] & 0xC0)==0x80 &&
					   (src[i+3] & 0xC0)==0x80 &&
					   (src[i+4] & 0xC0)==0x80){
						utf8_count++;
						i+=5;
						continue;
					}
				}
				else if((src[i] & 0xFE)==0xFC){
					if(i+5 >= srclen) break;
					if((src[i+1] & 0xC0)==0x80 &&
					   (src[i+2] & 0xC0)==0x80 &&
					   (src[i+3] & 0xC0)==0x80 &&
					   (src[i+4] & 0xC0)==0x80 &&
					   (src[i+5] & 0xC0)==0x80){
						utf8_count++;
						i+=6;
						continue;
					}
				}
				utf8_count--;
			}
			i++;
		}
	}

	if(utf8_count>0)
		return Encoding_UTF8;
	if(!(mask & Encoding_EUCJP) || sjis_count>0)
		return Encoding_SJIS;
	return Encoding_EUCJP;
}


void utf8_to_ucs(const BYTE* src, int* srclen, UINT32* dst, int* dstlen){
	int  si=0, di=0;
	const int  smax = *srclen;
	const int  dmax = *dstlen;
	WCHAR*  wbuf = new WCHAR[dmax*2];
	UINT32  ucs;

	while(si < smax && di < dmax){

		if((src[si] & 0x80) == 0){
			//1byte U+0 ~ U+7F
			wbuf[di++] = src[si++];
			continue;
		}
		else if((src[si] & 0xE0) == 0xC0){
			//2byte U+80 ~ U+7FF
			if(si+1 >= smax) break;
			if((src[si+1] & 0xC0) == 0x80){
				ucs = ((src[si+0] & 0x1F)<<6)
				    | ((src[si+1] & 0x3F)   );
				if(0x80 <= ucs && ucs <= 0x7FF){
					wbuf[di++] = (WCHAR) ucs;
					si += 2;
					continue;
				}
			}
		}
		else if((src[si] & 0xF0) == 0xE0){
			//3byte U+0x800 ~ 0xFFFF
			if(si+2 >= smax) break;
			if((src[si+1] & 0xC0) == 0x80 &&
			   (src[si+2] & 0xC0) == 0x80){
				ucs = ((src[si+0] & 0x0F)<<12)
				    | ((src[si+1] & 0x3F)<< 6)
				    | ((src[si+2] & 0x3F)    );
				if(0x800 <= ucs && ucs <= 0xFFFF){
					if(ucs == 0xFEFF/*bom*/){
					}else{
						wbuf[di++] = (WCHAR) ucs;
					}
					si += 3;
					continue;
				}
			}
		}
		else if((src[si] & 0xF8) == 0xF0){
			//4byte U+10000 ~ U+1FFFFF
			if(si+3 >= smax) break;
			if((src[si+1] & 0xC0) == 0x80 &&
			   (src[si+2] & 0xC0) == 0x80 &&
			   (src[si+3] & 0xC0) == 0x80){
				ucs = ((src[si+0] & 0x07)<<18)
				    | ((src[si+1] & 0x3F)<<12)
				    | ((src[si+2] & 0x3F)<< 6)
				    | ((src[si+3] & 0x3F)    );
				//surrogate pair U+10000 ~ U+10FFFF
				if(0x10000 <= ucs && ucs <= 0x10FFFF){
					wbuf[di++] = (WCHAR)( ((ucs - 0x10000) / 0x400) + 0xD800 );
					wbuf[di++] = (WCHAR)( ((ucs - 0x10000) % 0x400) + 0xDC00 );
					si += 4;
					continue;
				}
			}
		}
		#if 0
		else if((src[si] & 0xFC) == 0xF8){
			//5byte U+03FFFFFF
			if(si+4 >= smax) break;
			if((src[si+1] & 0xC0) == 0x80 &&
			   (src[si+2] & 0xC0) == 0x80 &&
			   (src[si+3] & 0xC0) == 0x80 &&
			   (src[si+4] & 0xC0) == 0x80){
				wbuf[di++] = ((src[si+0] & 0x03)<<24)
					   | ((src[si+1] & 0x3F)<<18)
					   | ((src[si+2] & 0x3F)<<12)
					   | ((src[si+3] & 0x3F)<< 6)
					   | ((src[si+4] & 0x3F)    );
				si += 5;
				continue;
			}
		}
		else if((src[si] & 0xFE) == 0xFC){
			//6byte U+7FFFFFFF
			if(si+5 >= smax) break;
			if((src[si+1] & 0xC0) == 0x80 &&
			   (src[si+2] & 0xC0) == 0x80 &&
			   (src[si+3] & 0xC0) == 0x80 &&
			   (src[si+4] & 0xC0) == 0x80 &&
			   (src[si+5] & 0xC0) == 0x80){
				wbuf[di++] = ((src[si+0] & 0x01)<<30)
					   | ((src[si+1] & 0x3F)<<24)
					   | ((src[si+2] & 0x3F)<<18)
					   | ((src[si+3] & 0x3F)<<12)
					   | ((src[si+4] & 0x3F)<< 6)
					   | ((src[si+5] & 0x3F)    );
				si += 6;
				continue;
			}
		}
		#endif

		//error
		wbuf[di++] = src[si++];
	}

	if(di>0){
		// normalize
		WCHAR*  ws;
		int  wlen = NormalizeString(NormalizationC, wbuf+0, di, wbuf+dmax, dmax);
		if(wlen>0){
			ws = wbuf+dmax;
		}else{
			ws = wbuf+0;
			wlen = di;
		}

		// ucs2 -> ucs4
		di = 0;
		for(int i=0; i < wlen; ){
			if(i+1 < wlen &&
			   0xD800 <= ws[i+0] && ws[i+0] <= 0xDBFF &&
			   0xDC00 <= ws[i+1] && ws[i+1] <= 0xDFFF){
				dst[di++] = 0x10000 + ((ws[i+0]-0xD800)*0x400) + (ws[i+1]-0xDC00);
				i+=2;
			}else{
				dst[di++] = ws[i];
				i+=1;
			}
		}
	}

	delete [] wbuf;

	*srclen = si;
	*dstlen = di;
}

void eucj_to_ucs(const BYTE* src, int* srclen, UINT32* dst, int* dstlen){
	int si=0, di=0;
	const int smax = *srclen;
	const int dmax = *dstlen;

	while(di < dmax && si+1 < smax){
		BYTE   b = src[si];
		UINT32 ucs = eucj_to_ucs((b<<8)|src[si+1]);
		if(ucs){
			si+=2;
			dst[di++] = ucs;
		}
		else{
			si++;
			dst[di++] = eucj_to_ucs(b);
		}
	}
	if(di < dmax && si < smax){
		BYTE  b = src[si];
		if((b==0x8E) || (0xA1<=b && b<=0xF4)){
		}
		else{
			si++;
			dst[di++] = eucj_to_ucs(b);
		}
	}
	*srclen = si;
	*dstlen = di;
}

void sjis_to_ucs(const BYTE* src, int* srclen, UINT32* dst, int* dstlen){
	int si=0, di=0;
	const int smax = *srclen;
	const int dmax = *dstlen;

	while(di < dmax && si+1 < smax){
		BYTE   b = src[si];
		UINT32 ucs = sjis_to_ucs((b<<8)|src[si+1]);
		if(ucs){
			si+=2;
			dst[di++] = ucs;
		}
		else{
			si++;
			dst[di++] = sjis_to_ucs(b);
		}
	}
	if(di < dmax && si < smax){
		BYTE  b = src[si];
		if((0x81<=b && b<=0x9F) || (0xE0<=b && b<=0xEA) || (0xF0<=b && b<=0xFC)){
		}
		else{
			si++;
			dst[di++] = sjis_to_ucs(b);
		}
	}
	*srclen = si;
	*dstlen = di;
}


static const UINT32  _is_dblchar_table []={
	0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,
	0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0x7FFFFFFF,0x000003FF,0xFFFFFFFF,
	0xFFFFFFFF,0x00000000,0x00000000,0x00000000,0x00000000,0xFFFF0000,0xFFFFFFFF,0xFFFFFFFF,
	0x0001FFFF,0x00000000,0x00000000,0x00000000,0x007F0000,0x00000000,0x00000000,0x00000000,
	0x00000000,0xC9000000,0x207BEFA2,0xA1E0C080,0x012BC69B,0x60040500,0x8BC08704,0x60000617,
	0x00000004,0x00000000,0x00000000,0x000AAAA0,0x00000000,0x00000000,0x00000000,0x01000100,
	0x00000000,0x00000000,0x48000000,0x00578097,0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0xFDFFFF00,0xFDFFFF01,0x00000001,0x01000000,0xFFFFFF80,0xFFFFFFFF,0xEE66F37F,
	0x00905A01,0x00000000,0x3D002000,0x00000000,0x00000020,0x00000000,0x50000000,0x8C009004,
	0x00000010,0xFEF03000,0x0007FE1F,0x0007FE04,0x00060000,0x00002800,0x1A000001,0x53C84513,
	0x0061E0BF,0xE6000822,0x98000199,0x40044001,0x01000000,0x00000000,0x00000000,0x00000800,
	0x0000000C,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0xFE000000,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFF7,0xFFFFFFFF,
	0xFFFFFFFF,0xFFFFFE1F,0xFE001FFF,0xF60079FF,0x86619807,0x78000793,0xC0000100,0x00A06184,
	0x0A000000,0x76000000,0x0000016F,0x01800000,0xE1800000,0x17FFFF7F,0x01FFFFFE,0x00000000,
	0x00400000,0x00000000,0xFFFF8000,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,
	0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,
	0xFEFFFFFF,0x0007FFFF,0xFFFFFE00,0xFFFFFFFF,0x000001FF,0x00000000,0x00000000,0x00000000,
	0xFE000000,0xFFFFFFFF,0xFFFFFFFF,0x03FFFFFF,0x00000000,0x00000000,0x00000000,0xFE000000,
	0x00400000,
};

inline bool _is_dblchar(int idx){
	return ( _is_dblchar_table[idx>>5] & 1<<(idx&31) ) ? true : false;
}

bool  is_dblchar(UINT32 n){
	if(n <= 0x004DFF){
		if(n <= 0x0010FF) return false;	/* U+000000 ~ U+0010FF (4352)*/
		if(n <= 0x00115F) return true;	/* U+001100 ~ U+00115F (96)*/
		if(n <= 0x002328) return false;	/* U+001160 ~ U+002328 (4553)*/
		if(n <= 0x00232A) return true;	/* U+002329 ~ U+00232A (2)*/
		if(n <= 0x002E7F) return false;	/* U+00232B ~ U+002E7F (2901)*/
		if(n <= 0x00303F) return _is_dblchar(n-0x002E80+0);	/* U+002E80 ~ U+00303F (448)*/
		if(n <= 0x003247) return true;	/* U+003040 ~ U+003247 (520)*/
		if(n <= 0x00324F) return false;	/* U+003248 ~ U+00324F (8)*/
		if(n <= 0x004DBF) return true;	/* U+003250 ~ U+004DBF (7024)*/
		if(n <= 0x004DFF) return false;	/* U+004DC0 ~ U+004DFF (64)*/
	}
	else  if(n <= 0x00FFE6){
		if(n <= 0x00A4CF) return true;	/* U+004E00 ~ U+00A4CF (22224)*/
		if(n <= 0x00A95F) return false;	/* U+00A4D0 ~ U+00A95F (1168)*/
		if(n <= 0x00A97C) return true;	/* U+00A960 ~ U+00A97C (29)*/
		if(n <= 0x00ABFF) return false;	/* U+00A97D ~ U+00ABFF (643)*/
		if(n <= 0x00D7A3) return true;	/* U+00AC00 ~ U+00D7A3 (11172)*/
		if(n <= 0x00F8FF) return false;	/* U+00D7A4 ~ U+00F8FF (8540)*/
		if(n <= 0x00FAFF) return true;	/* U+00F900 ~ U+00FAFF (512)*/
		if(n <= 0x00FE0F) return false;	/* U+00FB00 ~ U+00FE0F (784)*/
		if(n <= 0x00FFE6) return _is_dblchar(n-0x00FE10+448);	/* U+00FE10 ~ U+00FFE6 (471)*/
	}
	else{
		if(n <= 0x01AFFF) return false;	/* U+00FFE7 ~ U+01AFFF (45081)*/
		if(n <= 0x01B001) return true;	/* U+01B000 ~ U+01B001 (2)*/
		if(n <= 0x01F1FF) return false;	/* U+01B002 ~ U+01F1FF (16894)*/
		if(n <= 0x01F2FF) return true;	/* U+01F200 ~ U+01F2FF (256)*/
		if(n <= 0x01FFFF) return false;	/* U+01F300 ~ U+01FFFF (3328)*/
		if(n <= 0x02FFFD) return true;	/* U+020000 ~ U+02FFFD (65534)*/
		if(n <= 0x02FFFF) return false;	/* U+02FFFE ~ U+02FFFF (2)*/
		if(n <= 0x03FFFD) return true;	/* U+030000 ~ U+03FFFD (65534)*/
		if(n <= 0x10FFFF) return false;	/* U+03FFFE ~ U+10FFFF (851970)*/
	}
	return false;
}

bool  is_dblchar_cjk(UINT32 n){
	if(n <= 0x004DFF){
		if(n <= 0x000451) return _is_dblchar(n-0x000000+919);	/* U+000000 ~ U+000451 (1106)*/
		if(n <= 0x0010FF) return false;	/* U+000452 ~ U+0010FF (3246)*/
		if(n <= 0x00115F) return true;	/* U+001100 ~ U+00115F (96)*/
		if(n <= 0x00200F) return false;	/* U+001160 ~ U+00200F (3760)*/
		if(n <= 0x00277F) return _is_dblchar(n-0x002010+2025);	/* U+002010 ~ U+00277F (1904)*/
		if(n <= 0x002B54) return false;	/* U+002780 ~ U+002B54 (981)*/
		if(n <= 0x002B59) return true;	/* U+002B55 ~ U+002B59 (5)*/
		if(n <= 0x002E7F) return false;	/* U+002B5A ~ U+002E7F (806)*/
		if(n <= 0x00303F) return _is_dblchar(n-0x002E80+3929);	/* U+002E80 ~ U+00303F (448)*/
		if(n <= 0x004DBF) return true;	/* U+003040 ~ U+004DBF (7552)*/
		if(n <= 0x004DFF) return false;	/* U+004DC0 ~ U+004DFF (64)*/
	}
	else if(n <= 0x00FFFD){
		if(n <= 0x00A4CF) return true;	/* U+004E00 ~ U+00A4CF (22224)*/
		if(n <= 0x00A95F) return false;	/* U+00A4D0 ~ U+00A95F (1168)*/
		if(n <= 0x00A97C) return true;	/* U+00A960 ~ U+00A97C (29)*/
		if(n <= 0x00ABFF) return false;	/* U+00A97D ~ U+00ABFF (643)*/
		if(n <= 0x00D7A3) return true;	/* U+00AC00 ~ U+00D7A3 (11172)*/
		if(n <= 0x00DFFF) return false;	/* U+00D7A4 ~ U+00DFFF (2140)*/
		if(n <= 0x00FAFF) return true;	/* U+00E000 ~ U+00FAFF (6912)*/
		if(n <= 0x00FDFF) return false;	/* U+00FB00 ~ U+00FDFF (768)*/
		if(n <= 0x00FFFD) return _is_dblchar(n-0x00FE00+4377);	/* U+00FE00 ~ U+00FFFD (510)*/
	}
	else if(n <= 0x03FFFD){
		if(n <= 0x01AFFF) return false;	/* U+00FFFE ~ U+01AFFF (45058)*/
		if(n <= 0x01B001) return true;	/* U+01B000 ~ U+01B001 (2)*/
		if(n <= 0x01F0FF) return false;	/* U+01B002 ~ U+01F0FF (16638)*/
		if(n <= 0x01F77F) return true;	/* U+01F100 ~ U+01F77F (1664)*/
		if(n <= 0x01FFFF) return false;	/* U+01F780 ~ U+01FFFF (2176)*/
		if(n <= 0x02FFFD) return true;	/* U+020000 ~ U+02FFFD (65534)*/
		if(n <= 0x02FFFF) return false;	/* U+02FFFE ~ U+02FFFF (2)*/
		if(n <= 0x03FFFD) return true;	/* U+030000 ~ U+03FFFD (65534)*/
	}
	else{
		if(n <= 0x0E00FF) return false;	/* U+03FFFE ~ U+0E00FF (655618)*/
		if(n <= 0x0E01EF) return true;	/* U+0E0100 ~ U+0E01EF (240)*/
		if(n <= 0x0EFFFF) return false;	/* U+0E01F0 ~ U+0EFFFF (65040)*/
		if(n <= 0x0FFFFD) return true;	/* U+0F0000 ~ U+0FFFFD (65534)*/
		if(n <= 0x0FFFFF) return false;	/* U+0FFFFE ~ U+0FFFFF (2)*/
		if(n <= 0x10FFFD) return true;	/* U+100000 ~ U+10FFFD (65534)*/
		if(n <= 0x10FFFF) return false;	/* U+10FFFE ~ U+10FFFF (2)*/
	}
	return false;
}

}//namespace Enc
}//namespace Ck
//EOF
