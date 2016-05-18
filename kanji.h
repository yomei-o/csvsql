/*
Copyright (c) 2016, Yomei Otani <yomei.otani@gmai.com>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the FreeBSD Project.
*/

#ifndef _KANJI_H_
#define  _KANJI_H_


#ifdef __cplusplus
extern "C"
{
#endif


unsigned short kj_cp932_to_unicode_c(unsigned short src);
unsigned short  kj_unicode_to_cp932_c(unsigned short src);

unsigned short  kj_jis_to_euc_c(unsigned short c);
unsigned short  kj_euc_to_jis_c(unsigned short c);

unsigned short  kj_jis_to_sjis_c(unsigned short c);
unsigned short  kj_sjis_to_jis_c(unsigned short c);

unsigned short kj_euc_to_unicode_c(unsigned short src);
unsigned short kj_jis_to_unicode_c(unsigned short src);
unsigned short kj_sjis_to_unicode_c(unsigned short src);

unsigned short kj_unicode_to_euc_c(unsigned short src);
unsigned short kj_unicode_to_jis_c(unsigned short src);
unsigned short kj_unicode_to_sjis_c(unsigned short src);

int kj_cp932_to_unicode(unsigned char* src,unsigned short *dst);
int kj_cp932_to_unicode_n(unsigned char* src,unsigned short *dst, int sz);
int kj_euc_to_unicode(unsigned char* src,unsigned short *dst);
int kj_jis_to_unicode(unsigned char* src,unsigned short *dst);
int kj_jis_to_unicode_m(unsigned char* src,unsigned short *dst);
int kj_sjis_to_unicode_n(unsigned char* src,unsigned short *dst,int sz);
int kj_sjis_to_unicode(unsigned char* src,unsigned short *dst);

int kj_unicode_to_cp932_n(unsigned short* src,unsigned char *dst, int sz);
int kj_unicode_to_cp932(unsigned short* src,unsigned char *dst);
int kj_unicode_to_euc(unsigned short* src,unsigned char *dst);
int kj_unicode_to_jis(unsigned short* src,unsigned char *dst);
int kj_unicode_to_sjis_n(unsigned short* src,unsigned char *dst, int sz);
int kj_unicode_to_sjis(unsigned short* src,unsigned char *dst);
int kj_utf8_to_unicode_n(unsigned char* src,unsigned short* dst, int sz);
int kj_utf8_to_unicode(unsigned char* src ,unsigned short* dst);
int kj_unicode_to_utf8(unsigned short* src,unsigned char* dst);

unsigned short kj_utf8_to_unicode_c(int src);
int kj_unicode_to_utf8_c(unsigned short src);


int kj_unicode_to_utf8(unsigned short* src,unsigned char* dst);
int kj_utf8_to_unicode(unsigned char* src,unsigned short* dst);
int kj_unicode_to_utf8_n(unsigned short* src,unsigned char* dst,int n);

unsigned char *kj_utf8strncpyz(unsigned char* dst, unsigned char* src, int sz);


#ifdef __cplusplus
}
#endif


#endif /* _KANJI_H*/

