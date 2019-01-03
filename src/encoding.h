#pragma once
namespace Ck{
namespace Enc{

struct _encoding_table_a{ WORD  e,s; const WCHAR* p; };
struct _encoding_table_u{ WCHAR e,s; const WORD*  p; };

inline UINT32 sjis_to_ucs (WORD n){
 extern const struct _encoding_table_a  _sjis_to_ucs_table [];
 const struct _encoding_table_a * t = _sjis_to_ucs_table + (n>>8);
 return (t->e >= n && n >= t->s) ? t->p[n - t->s] : 0;
}

inline WORD ucs_to_sjis (UINT32 n){
 if(n <= 0xFFFF){
  extern const struct _encoding_table_u  _ucs_to_sjis_table [];
  const struct _encoding_table_u * t = _ucs_to_sjis_table + (n>>8);
  if(t->e >= n && n >= t->s) return t->p[n - t->s];
 }
 return 0;
}

inline UINT32 eucj_to_ucs (WORD n){
 extern const struct _encoding_table_a  _eucj_to_ucs_table [];
 const struct _encoding_table_a * t = _eucj_to_ucs_table + (n>>8);
 return (t->e >= n && n >= t->s) ? t->p[n - t->s] : 0;
}

inline WORD ucs_to_eucj (UINT32 n){
 if(n <= 0xFFFF){
  extern const struct _encoding_table_u  _ucs_to_eucj_table [];
  const struct _encoding_table_u * t = _ucs_to_eucj_table + (n>>8);
  if(t->e >= n && n >= t->s) return t->p[n - t->s];
 }
 return 0;
}

Encoding  detect_encoding(const BYTE* src, int srclen, Encoding mask);

void utf8_to_ucs(const BYTE* src, int* srclen, UINT32* dst, int* dstlen);
void eucj_to_ucs(const BYTE* src, int* srclen, UINT32* dst, int* dstlen);
void sjis_to_ucs(const BYTE* src, int* srclen, UINT32* dst, int* dstlen);

bool is_dblchar(UINT32 n);
bool is_dblchar_cjk(UINT32 n);

}//namespace Enc
}//namespace Ck
//EOF
