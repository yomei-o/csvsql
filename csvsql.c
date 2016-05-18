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

//#define I_USE_EXCEL

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "csvsql.h"
#ifdef I_USE_EXCEL
#include "kanji.h"
#endif

#if defined(_WIN32) && !defined(__GNUC__)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#ifdef _MSC_VER
#if _MSC_VER >= 1400
#pragma warning( disable : 4996 )
#pragma warning( disable : 4819 )
#endif
#endif

#define SQL_PATH "excel"
//
// string
//
static void mystrlwr(char*key)
{
	if (key == NULL)return;
	while (*key){
		*key = tolower(*key);
		key++;
	}
}
static void mystrupr(char*key)
{
	if (key == NULL)return;
	while (*key){
		*key = toupper(*key);
		key++;
	}
}
static void replace_space(char *key){
	if (key == NULL)return;
	while (*key){
		if (*key >0 && *key < ' ')*key = ' ';
		key++;
	}

}

static int isnamec(char a)
{
	if (a == '_')return 1;
	if (a >= '0' && a<='9')return 1;
	if (a >= 'A' && a <= 'Z')return 1;
	if (a >= 'a' && a <= 'z')return 1;
	return 0;
}

static int isnamestr(const char* str)
{
	if (str == NULL)return 0;
	while (*str){
		if (isnamec(*str) == 0)return 0;
		str++;
	}
	return 1;
}

static int isvalue(const char* str)
{
	if (str == NULL)return 0;
	if (*str==0)return 1;
	if (*str == '\"')return 1;
	while (*str){
		if (isdigit(*str) == 0)return 0;
		str++;
	}
	return 1;
}


static int isoperator(char c)
{
	switch (c){
	case '=':
	case '<':
	case '>':
	case '!':
	case '+':
	case '-':
	case '*':
	case '/':
	case '%':
	case '&':
	case '|':
	case '^':
	case '~':
	case '?':
		return 1;
	}
	return 0;
}

static const char* endof_operator(const char* str){
	if (str == NULL)return NULL;
	while (isoperator(*str))str++;
	return str;
}

static int ishensu(char c)
{
	if (c == '_')return 1;
	if (c >= '0' && c <= '9')return 1;
	if (c >= 'A' && c <= 'Z')return 1;
	if (c >= 'a' && c <= 'z')return 1;
	if (c < 0 || c>0x80)return 1;
	return 0;
}

static const char* endof_hensu(const char* str){
	if (str == NULL)return NULL;
	while (ishensu(*str))str++;
	return str;
}

static const char* endof_number(const char* str)
{
	const char *ret = NULL, *p = str;

	if (str == NULL)return ret;
	while (*p && *p > 0 && *p <= ' ')p++;
	if (*p < '0' || *p > '9')return ret;
	p++;
	while (*p >= '0' && *p <= '9')p++;
	return p;
}

//
// parse level1
//

static const char* endof_quot(const char* str)
{
	const char *ret = NULL,*p=str,*st=NULL,*ed=NULL;
	
	if (str == NULL)return ret;
	while (*p && *p > 0 && *p <= ' ')p++;
	if (*p != '\"' && *p != '\'')return ret;
	st = p++;
	while (*p){
		if (*p == '\"' && *(p + 1) == '\"'){
			p += 2;
			continue;
		}
		if (*p == *st){
			p++;
			return p;
		}
		p++;
	}
	return ret;
}

static const char* endof_quot_yen_escaped(const char* str)
{
	const char *ret = NULL, *p = str, *st = NULL, *ed = NULL;

	if (str == NULL)return ret;
	while (*p && *p > 0 && *p <= ' ')p++;
	if (*p != '\"' && *p != '\'')return ret;
	st = p++;
	while (*p){
		if (*p == '\\'){
			p++;
			if (*p == 0)return NULL;
			p++;
			continue;
		}
		if (*p == *st){
			p++;
			return p;
		}
		p++;
	}
	return ret;
}

static const char* endof_bracket(const char* str)
{
	const char *ret = NULL, *p = str, *st = NULL, *ed = NULL;

	if (str == NULL)return ret;
	while (*p && *p > 0 && *p <= ' ')p++;
	if (*p != '(')return ret;
	st = p++;
	while (*p){
		if (*p == '\"' || *p=='\''){
			p=endof_quot(p);
			if (p==NULL)return NULL;
			continue;
		}
		if (*p == ')'){
			p++;
			return p;
		}
		p++;
	}
	return ret;
}

static int gettagpos(const char* str, int n, char* out, int sz,const char** pos)
{
	int ret=-1;
	const unsigned char*p,*st,*ed;
	int ct = 0;
	int ss;
	if (str == NULL)return ret;
	if (out != NULL && sz < 1)return ret;

	p = (const unsigned char*)str;

	if(out)*out = 0;
	if (pos)*pos = NULL;

	while (1){
		st = NULL;
		ed = NULL;
		while (*p && *p!='\"' && *p!='\'' && *p!='(' && ((*p>0 && *p <= ' ') || *p==','))p++;
		if (*p == 0)break;
		if (*p == '\"' || *p == '\''){
			p = endof_quot(p);
			if (p == NULL)return ret;
			continue;
		}
		if (*p == '('){
			p = endof_bracket(p);
			if (p == NULL)return ret;
			continue;
		}
		st = p;
		while (*p && *p != '(' && *p!=',' && (*p<0 || *p >' '))p++;
		ed = p;
		if (ct == n){
			ss = ed - st;
			if (out){
				if (ss + 1 >= sz)return ret;
				memcpy(out, st, ss);
				out[ss] = 0;
				mystrupr(out);
			}
			if (pos)*pos = ed;
			ret = 0;
			return ret;
		}
		if (*p == 0)break;
		ct++;
	}

	return ret;
}


static void delete_space(char* str){
	char *src, *dst;
	char *p;

	src = dst = str;
	if (str == NULL)return;
	while (*src && *src>0 && *src <= ' ')src++;
	if (*src == 0){
		*dst = 0;
		return;
	}
	if (*src != '\"'){
		while (*src > ' ' || *src < 0)*dst++ = *src++;
		*dst = 0;
		return;
	}
	p = (char*)endof_quot(src);
	if (p == NULL){
		*dst = 0;
		return;
	}
	memmove(dst, src, p - src);
	dst[p - src] = 0;
}

//
// csv
//

static void csvread_deletecrlf(char* buf)
{
	int i, l;
	if (buf == NULL)return;
	l = strlen(buf);
	for (i = l - 1; i >= 0; i--){
		if (buf[i] == '\r' || buf[i] == '\n')buf[i] = 0;
		else break;
	}
	for (i = 0; i<l; i++){
		if (buf[i] >0 && buf[i]<' ')buf[i] = ' ';
	}
}

static int csvread_getseparatorindex(char* buf)
{
	int ret = -1;
	int f_quot = 0;
	char*p;
	p = buf;
	if (p == NULL)return ret;
	while (*p){
		if (*p == ',' && f_quot == 0)return  p - buf;
		if (*p == '\"')f_quot = 1 - f_quot;
		p++;
	}
	return p - buf;
}


static char* csvread_getnextpointer(char* buf)
{
	int idx;
	if (buf == NULL)return NULL;
	idx = csvread_getseparatorindex(buf);
	if (idx < 0)return buf;
	if (buf[idx] == ',')idx++;
	return buf + idx;
}

static void csvread_deletequote(char* buf)
{
	int l;
	if (buf == NULL)return;
	l = strlen(buf);
	if (l < 2)return;
	if (buf[0] == '\"' && buf[l - 1] == '\"'){
		memmove(buf, buf + 1, l - 2);
		buf[l - 2] = 0;
		return;
	}
}

static void csvread_replacequote(char* buf)
{
	char* p;

	if (buf == NULL)return;
	p = buf;
	while (*buf != 0){
		if (*buf == '\"' && *(buf + 1) == '\"')buf++;
		*p++ = *buf++;
	}
	*p = 0;
}


static void csvread_copynstring(char* buf, char* buf2, int sz, int n)
{
	int i, idx;
	char* p;

	if (buf == NULL || buf2 == NULL || sz<1)return;
	buf2[0] = 0;
	if (n < 0)return;

	csvread_deletecrlf(buf);
	for (i = 0, p = buf; i <= n; i++){
		idx = csvread_getseparatorindex(p);
		if (idx + 1<sz && i == n){
			memcpy(buf2, p, idx);
			buf2[idx] = 0;
			csvread_deletequote(buf2);
			csvread_replacequote(buf2);
			return;
		}
		p = csvread_getnextpointer(p);
	}
}


//
// parse level2
//

static int sql_get_tag(const char* str, int n, char* out, int sz)
{
	return gettagpos(str, n, out, sz, NULL);
}

static int sql_get_tag_num(const char* str)
{
	int ret = 0,r;

	if (str == NULL)return -1;
	while (1){
		r = sql_get_tag(str, ret, NULL, 0);
		if (r < 0)break;
		ret++;
	}
	return ret;
}

static int sql_is_tag(const char* str,const char* tag_)
{
	int ret = 0, r, ct = 0;
	char tmp[128];
	char tag[128];

	if (str == NULL || tag_==NULL)return ret;
	if (strlen(tag_) > 127)return ret;
	strcpy(tag, tag_);
	mystrupr(tag);

	while (1){
		r = sql_get_tag(str, ct,tmp, 128);
		if (r < 0)break;
		if (strcmp(tmp, tag) == 0){
			ret = 1;
			break;
		}
		ct++;
	}
	return ret;
}

static const char* sql_get_tag_pos(const char* str,const char* tag_)
{
	char tag[128];
	char tmp[128];
	int  r,ct=0;
	const char* pos = NULL;
	if (str == NULL || tag_ == NULL)return NULL;
	if (strlen(tag_) > 127)return NULL;
	strcpy(tag, tag_);
	mystrupr(tag);

	while (1){
		r = gettagpos(str,ct, tmp, 128,&pos);
		if (r < 0)break;
		if (strcmp(tag, tmp) == 0){
			return pos;
		}
		ct++;
	}
	return NULL;
}

static int get_a_is_b(const char* str, char* a, int asz, char *b, int bsz)
{
	int ret = -1;
	const char *st, *ed;

	if (str == NULL || a == NULL || b == NULL || asz < 1 || bsz < 1)return ret;
	a[0] = 0;
	b[0] = 0;

	while (*str && *str>0 && *str <= ' ')str++;
	if (ishensu(*str) == 0 || isdigit(*str&255) != 0)return ret;
	st = str;
	ed = endof_hensu(str);
	if (ed == NULL)return ret;
	if (ed - st + 1 >= asz)return ret;
	memcpy(a, st, ed - st);
	a[ed - st] = 0;
	mystrupr(a);
	str = ed;

	while (*str && *str>0 && *str <= ' ')str++;
	if (isoperator(*str) == 0)return ret;
	st = str;
	ed = endof_operator(str);
	if (ed == NULL)return ret;
	if (ed - st  !=1)return ret;
	if (*str != '=')return ret;
	str = ed;

	while (*str && *str>0 && *str <= ' ')str++;
	st = str;
	if (*str == '\'' || *str == '\"'){
		ed = endof_quot(str);
	}
	else{
		if (isdigit(*str & 255) == 0)return ret;
		ed = endof_number(str);
	}
	if (ed == NULL)return ret;
	if (ed - st + 1 >= bsz)return ret;
	memcpy(b, st, ed - st);
	b[ed - st] = 0;
	csvread_deletequote(b);
	csvread_replacequote(b);
	return 0;
}

//
// hikisu
//

static int sql_get_function(const char* str, char* out, int sz)
{
	int ret = -1;
	const unsigned char *st, *ed;
	int ss;
	if (str == NULL || out == NULL || sz<1)return ret;

	*out = 0;

	st = strchr(str, '(');
	if (st == NULL)return ret;
	ed = endof_bracket(st);
	if (ed == NULL)return ret;

	st++;
	ed--;

	ss = ed - st;
	if (ss + 1 >= sz)return ret;
	strncpy(out, st, ss);
	out[ss] = 0;
	ret = 0;
	return ret;
}


static int sql_get_param(const char* str, int n, char* out, int sz)
{
	int ret = -1;
	const unsigned char*p, *st, *ed;
	int ct = 0;
	int ss;
	if (str == NULL)return ret;
	if (out != NULL && sz < 1)return ret;

	p = (const unsigned char*)str;

	if (out)*out = 0;

	while (1){
		st = NULL;
		ed = NULL;

		st = p;
		while (*p &&  *p != ','){
			if (*p == '\'' || *p == '\"'){
				p = endof_quot(p);
				if (p == NULL)return ret;
				continue;
			}
			p++;
		}
		ed = p;
		if (ct == n){
			ss = ed - st;
			if (out){
				if (ss + 1 >= sz)return ret;
				strncpy(out, st, ss);
				out[ss] = 0;
			}
			ret = 0;
			return ret;
		}
		if (*p == 0)break;
		p++;
		ct++;
	}

	return ret;
}

static int sql_get_param_num(const char* str)
{
	int ret = 0, r;

	if (str == NULL)return -1;
	while (1){
		r = sql_get_param(str, ret, NULL, 0);
		if (r < 0)break;
		ret++;
	}
	return ret;
}
//
// file open
//

static FILE* sql_zengo_open(const char* file, const char* mode, const char* path)
{
	FILE* ret = NULL;
	char buf[4096];
	if (file == NULL || mode == NULL || path == NULL)return ret;

	if (strlen(file) > 1000)return ret;
	if (strlen(path) > 1000)return ret;

	// 1
	sprintf(buf, "%s/%s", path, file);
	ret = fopen(buf, mode);
	if (ret != NULL)return ret;

	// 2
	sprintf(buf, "../%s/%s", path, file);
	ret = fopen(buf, mode);
	if (ret != NULL)return ret;

	// 3
	sprintf(buf, "../../%s/%s", path, file);
	ret = fopen(buf, mode);
	if (ret != NULL)return ret;

	// 4
	ret = fopen(file, mode);
	if (ret != NULL)return ret;

	// 5
	sprintf(buf, "../%s", file);
	ret = fopen(buf, mode);
	if (ret != NULL)return ret;

	return ret;
}


static int sql_zengo_remove(const char* file, const char* path)
{
	int ret = -1;
	char buf[4096];
	if (file == NULL  || path == NULL)return ret;

	if (strlen(file) > 1000)return ret;
	if (strlen(path) > 1000)return ret;

	// 1
	sprintf(buf, "%s/%s", path, file);
	ret = remove(buf);
	if (ret ==0 )return ret;

	// 2
	sprintf(buf, "../%s/%s", path, file);
	ret = remove(buf);
	if (ret == 0)return ret;

	// 3
	sprintf(buf, "../../%s/%s", path, file);
	ret = remove(buf);
	if (ret == 0)return ret;

	// 4
	ret = remove(file);
	if (ret == 0)return ret;

	// 5
	sprintf(buf, "../%s", file);
	ret = remove(buf);
	if (ret == 0)return ret;

	return ret;
}



//
// command
//

static int cmd_dummy(const char* str,void* out,int sz)
{
	int ret = -1;
	return ret;
}

static int cmd_create(const char* str, void* out, int sz)
{
	int ret = -1, r, ct = 2,exist = 0,i;
	const char*p;
	char tag[256];
	char buf[256];
	char param[64];
	char col[MAX_COL][64];
	FILE* fp;

	r = sql_get_tag(str, 1, tag, sizeof(tag));
	if (r < 0)return ret;

	if (strcmp(tag, "DATABASE") == 0)return 0;

	if (strcmp(tag, "TABLE") != 0)return ret;
	while(1){
		r = sql_get_tag(str, ct, tag, sizeof(tag));
		if (r < 0)return ret;
		if (strcmp(tag, "IF") == 0){
			ct++;
			continue;
		}
		if (strcmp(tag, "NOT") == 0){
			ct++;
			continue;
		}
		if (strcmp(tag, "EXISTS") == 0){
			ct++;
			exist = 1;
			continue;
		}
		break;
	}
	// succes

	p=sql_get_tag_pos(str, tag);
	if (p == NULL)return ret;
	r = sql_get_function(str, buf, sizeof(buf));
	if (r < 0)return ret;
	ct = sql_get_param_num(buf);
	if (ct < 1)return ret;
	if (ct>MAX_COL)return ret;
	mystrlwr(tag);
	if (strlen(tag)>200)return ret;
	strcat(tag, ".csv");

	memset(col, 0, sizeof(col));
	for (i = 0; i < ct; i++){
		param[0] = 0;
		r = sql_get_param(buf, i, param, sizeof(param));
		if (r < 0)return ret;
		sscanf(param, "%s", col[i]);
		mystrupr(col[i]);
		if (isnamestr(col[i]) == 0)return ret;
	}

	//for (i = 0; i < ct; i++){
	//	printf("col[%d]=>>%s<<\n",i, col[i]);
	//}

	fp = sql_zengo_open(tag, "rb", SQL_PATH);
	if (fp){
		fclose(fp);
		if (exist)return 0;
		return -1;
	}
	fp = sql_zengo_open(tag, "wt", SQL_PATH);
	if (fp == NULL)return ret;

	for (i = 0; i < ct; i++){
		if (i != 0)fprintf(fp, ",");
#ifdef I_USE_EXCEL
		{
			unsigned short aaa[64];
			kj_utf8_to_unicode_n(col[i], aaa, sizeof(aaa));
			kj_unicode_to_cp932_n(aaa, col[i], sizeof(col[i]));
		}
#endif
		fprintf(fp, "%s", col[i]);
	}
	fprintf(fp,"\n");
	fclose(fp);
	ret = 0;
	return ret;
}

static int cmd_drop(const char* str, void* out, int sz)
{
	int ret = -1, r, ct = 2, exist = 0;
	const char*p;
	char tag[256];
	FILE* fp;

	r = sql_get_tag(str, 1, tag, sizeof(tag));
	if (r < 0)return ret;

	if (strcmp(tag, "DATABASE") == 0)return 0;

	if (strcmp(tag, "TABLE") != 0)return ret;
	while (1){
		r = sql_get_tag(str, ct, tag, sizeof(tag));
		if (r < 0)return ret;
		if (strcmp(tag, "IF") == 0){
			ct++;
			continue;
		}
		if (strcmp(tag, "EXISTS") == 0){
			ct++;
			exist = 1;
			continue;
		}
		break;
	}
	// succes

	p = sql_get_tag_pos(str, tag);
	if (p == NULL)return ret;

	mystrlwr(tag);
	if (strlen(tag)>200)return ret;
	strcat(tag, ".csv");

	fp = sql_zengo_open(tag, "rb", SQL_PATH);
	if (fp==NULL){
		if (exist)return 0;
		return -1;
	}
	fclose(fp);
	
	ret=sql_zengo_remove(tag, SQL_PATH);

	return ret;
}


static int cmd_insert(const char* str, void* out, int sz)
{
	int ret = -1, r, ct = 2, exist = 0, i,ct2;
	const char *p;
	char tag[256];
	char buf[4096];
	FILE* fp;
	char tmp[4096];

	r = sql_get_tag(str, 1, tag, sizeof(tag));
	if (r < 0)return ret;
	if (strcmp(tag, "INTO") != 0)return ret;

	r = sql_get_tag(str, 2, tag, sizeof(tag));
	if (r < 0)return ret;

	p = sql_get_tag_pos(str, "VALUES");
	if (p == NULL)return ret;

	r = sql_get_function(str, buf, sizeof(buf));
	if (r < 0)return ret;


	ct = sql_get_param_num(buf);
	if (ct < 1)return ret;
	if (ct>MAX_COL)return ret;
	mystrlwr(tag);
	if (strlen(tag)>200)return ret;
	strcat(tag, ".csv");

	fp = sql_zengo_open(tag, "rb", SQL_PATH);
	if (fp==NULL){
		return -1;
	}

	tmp[0] = 0;
	fgets(tmp, sizeof(tmp), fp);
	fclose(fp);

	ct2 = sql_get_param_num(tmp);

	if (ct < 1 || ct2 < 1)return ret;
	if (ct != ct2)return ret;

	fp = sql_zengo_open(tag, "at", SQL_PATH);
	if (fp == NULL){
		return -1;
	}

	for (i = 0; i < ct; i++){
		tmp[0] = 0;
		sql_get_param(buf, i, tmp, sizeof(tmp));
		if (strlen(tmp)>1024)goto err;
		delete_space(tmp);
		if (isvalue(tmp) == 0)goto err;
	}

	for (i = 0; i < ct; i++){
		tmp[0] = 0;
		sql_get_param(buf,i, tmp, sizeof(tmp));
		delete_space(tmp);
#ifdef I_USE_EXCEL
		{
			unsigned short aaa[1024];
			kj_utf8_to_unicode_n(tmp, aaa,sizeof(aaa));
			kj_unicode_to_cp932_n(aaa, tmp,sizeof(tmp));
		}
#endif
		if (i != 0)fprintf(fp, ",");
		fprintf(fp,"%s",tmp);
	}
	fprintf(fp,"\n");
	ret = 0;

err:
	if(fp)fclose(fp);

	return ret;
}


static int cmd_select_max(const char* str, void* out, int sz)
{
	int ret = -1, ct = 2, r, i, pos;
	struct sqlcsv_handle* h;
	char tag[256];
	char col[64];
	char buf[4096];
	int maxv = 0;
	FILE* fp;
	const char* p;


	if (out == NULL)return ret;
	h = (struct sqlcsv_handle*)out;


	if (h->child->times == 1)return 0;
	if (h->child->times > 1)return -1;

	r = sql_get_tag(str, 1, tag, sizeof(tag));
	if (r < 0)return -1;
	if (strcmp(tag, "MAX") != 0)return ret;
	p = sql_get_tag_pos(str, "MAX");
	if (p == NULL)return ret;
	r = sql_get_function(p, col, sizeof(col));

	if (r < 0)return ret;
	delete_space(col);
	mystrupr(col);

	// for the first time
	while (1){
		r = sql_get_tag(str, ct, tag, sizeof(tag));
		if (r < 0)return -1;
		if (strcmp(tag, "FROM") == 0)break;
		ct++;
	}
	r = sql_get_tag(str, ct + 1, tag, sizeof(tag));
	if (r < 0)return -1;
	if (strlen(tag)>200)return ret;
	mystrlwr(tag);
	strcat(tag, ".csv");

	fp = sql_zengo_open(tag, "rb", SQL_PATH);
	if (fp == NULL)return ret;

	buf[0] = 0;
	if (fgets(buf, sizeof(buf), fp) == NULL){
		goto end;
	}
#ifdef I_USE_EXCEL
	{
		unsigned short uni[4096];
		kj_sjis_to_unicode_n(buf, uni,sizeof(uni));
		kj_unicode_to_utf8_n(uni, buf,sizeof(buf));
	}
#endif

	r = sql_get_param_num(buf);
	if (r < 1){
		goto end;
	}

	pos = -1;
	for (i = 0; i < r; i++){
		tag[0] = 0;
		csvread_copynstring(buf, tag, sizeof(tag), i);
		mystrupr(tag);
		if (strcmp(tag, col) == 0){
			pos = i;
			break;
		}
	}
	if (pos == -1)return ret;

	while (fgets(buf, sizeof(buf), fp)){
		tag[0] = 0;
#ifdef I_USE_EXCEL
		{
			unsigned short uni[4096];
			kj_sjis_to_unicode_n(buf, uni,sizeof(uni));
			kj_unicode_to_utf8_n(uni, buf,sizeof(buf));
		}
#endif
		csvread_copynstring(buf, tag, sizeof(tag), i);
		r = -1;
		sscanf(tag, "%d", &r);
		if (r > maxv)maxv = r;
	}

	sprintf(h->child->buf, "%d", maxv);

	ret = 0;
end:
	if (fp)fclose(fp);

	return ret;
}


static int cmd_select_asta(const char* str, void* out, int sz)
{
	int ret = -1,ct=2,r;
	struct sqlcsv_handle* h;
	char tag[256];
	char buf[4096];
	FILE* fp;

	if (out == NULL)return ret;
	h = (struct sqlcsv_handle*)out;

	r = sql_get_tag(str, 1, tag, sizeof(tag));
	if (r < 0)return -1;
	if (strcmp(tag, "*") != 0)return ret;

	// for the first time
	while (1){
		r = sql_get_tag(str, ct, tag, sizeof(tag));
		if (r < 0)return -1;
		if (strcmp(tag, "FROM") == 0)break;
		ct++;
	}
	r = sql_get_tag(str, ct+1, tag, sizeof(tag));
	if (r < 0)return -1;
	if (strlen(tag)>200)return ret;
	mystrlwr(tag);
	strcat(tag, ".csv");

	fp = sql_zengo_open(tag, "rb", SQL_PATH);
	if (fp == NULL)return ret;
	h->child->fp = fp;

	// first time
	buf[0] = 0;
	if (fgets(buf, sizeof(buf), fp) == NULL)return ret;
	return 0;
}

static int cmd_select_asta_next(const char* str, void* out, int sz)
{
	int ret = -1;
	struct sqlcsv_handle* h;
	FILE* fp;
	char buf[4096];

	if (out == NULL)return ret;
	h = (struct sqlcsv_handle*)out;

	fp = h->child->fp;
	buf[0] = 0;

	if (fgets(buf, sizeof(buf), fp) == NULL)return ret;
#ifdef I_USE_EXCEL
	{
		unsigned short uni[4096];
		kj_sjis_to_unicode_n(buf, uni,sizeof(uni));
		kj_unicode_to_utf8_n(uni, buf,sizeof(buf));
	}
#endif
	strcpy(h->child->buf, buf);
	ret = 0;
	return ret;
}


static int cmd_select_asta_where(const char* str, void* out, int sz)
{
	int ret = -1, ct = 2, r;
	struct sqlcsv_handle* h;
	char tag[256];
	char buf[4096];
	char eq[1024];
	FILE* fp;
	char *p;
	char col[256];
	char val[1024];
	int coln = -1,i;


	if (out == NULL)return ret;
	h = (struct sqlcsv_handle*)out;

	r = sql_get_tag(str, 1, tag, sizeof(tag));
	if (r < 0)return -1;
	if (strcmp(tag, "*") != 0)return ret;

	// for the first time
	while (1){
		r = sql_get_tag(str, ct, tag, sizeof(tag));
		if (r < 0)return -1;
		if (strcmp(tag, "FROM") == 0)break;
		ct++;
	}
	r = sql_get_tag(str, ct + 1, tag, sizeof(tag));
	if (r < 0)return -1;
	if (strlen(tag)>200)return ret;
	mystrlwr(tag);
	strcat(tag, ".csv");

	p = (char*)sql_get_tag_pos(str, "where");
	if (p == NULL)return ret;
	if (strlen(p) > 1000)return ret;
	strcpy(eq, p);
	replace_space(eq);

	col[0] = 0;
	val[0] = 0;
	r = get_a_is_b(eq, col, sizeof(col), val, sizeof(val));
	if (r==0)h->child->mode_where = 1;
	if (h->child->mode_where == 0){
		// TODO analize where
	}
	if (h->child->mode_where == 0)return ret;

	fp = sql_zengo_open(tag, "rb", SQL_PATH);
	if (fp == NULL)return ret;
	h->child->fp = fp;

	buf[0] = 0;
	if (fgets(buf, sizeof(buf), fp) == NULL)return ret;
#ifdef I_USE_EXCEL
	{
		unsigned short uni[4096];
		kj_sjis_to_unicode_n(buf, uni,sizeof(uni));
		kj_unicode_to_utf8_n(uni, buf,sizeof(buf));
	}
#endif
	if (h->child->mode_where == 1){
		ct = sql_get_param_num(buf);
		coln = -1;
		for (i = 0; i < ct; i++){
			tag[0] = 0;
			csvread_copynstring(buf, tag, sizeof(tag), i);
			mystrupr(tag);
			if (strcmp(tag, col) == 0){
				coln = i;
				break;
			}
		}
		if (coln == -1)return ret;
		if (strlen(val) + 1 > sizeof(h->child->val))return ret;
		if (strlen(col) + 1 > sizeof(h->child->key))return ret;

		h->child->key_pos = coln;
		strcpy(h->child->key, col);
		strcpy(h->child->val, val);
		return 0;
	}
	else if (h->child->mode_where == 2){
		return -1;
	}
	return -1;
}

static int cmd_select_asta_where_next(const char* str, void* out, int sz)
{
	int ret = -1;
	struct sqlcsv_handle* h;
	FILE* fp;
	char buf[4096];
	char val[1024];
	if (out == NULL)return ret;
	h = (struct sqlcsv_handle*)out;

	fp = h->child->fp;
	buf[0] = 0;


	while (1){
		if (fgets(buf, sizeof(buf), fp) == NULL)return ret;
#ifdef I_USE_EXCEL
		{
			unsigned short uni[4096];
			kj_sjis_to_unicode_n(buf, uni,sizeof(uni));
			kj_unicode_to_utf8_n(uni, buf,sizeof(buf));
		}
#endif
		if (h->child->mode_where == 1){
			val[0] = 0;
			csvread_copynstring(buf, val, sizeof(val), h->child->key_pos);
			if (strcmp(val, h->child->val) != 0)continue;
			strcpy(h->child->buf, buf);
			ret = 0;
			return ret;
		}
		else if (h->child->mode_where == 2){
			return -1;
		}
	}
	return -1;
}


static int cmd_select(const char* str, void* out, int sz)
{
	int ret = -1,r;
	char tag[64];
	struct sqlcsv_handle* h;


	if (out == NULL)return ret;
	h = (struct sqlcsv_handle*)out;


	r = sql_get_tag(str, 1, tag, sizeof(tag));
	if (r < 0)return ret;
	if (strcmp(tag, "MAX") == 0){
		h->child->f = cmd_select_max;
		ret = cmd_select_max(str, out, sz);
	}
	else if (strcmp(tag, "*") == 0){
		const char* p;
		p = sql_get_tag_pos(str,"WHERE");
		if (p == NULL){
			h->child->f = cmd_select_asta_next;
			ret = cmd_select_asta(str, out, sz);
		}
		else{
			h->child->f = cmd_select_asta_where_next;
			ret = cmd_select_asta_where(str, out, sz);
		}
	}
	return ret;
}

static int path_copy(const char* fa,const char* fb)
{
	int ret = -1;
	int sz,wt;
	char buf[1024 * 64];
	FILE *fi = NULL, *fo = NULL;
	fi = sql_zengo_open(fa, "rb", SQL_PATH);
	fo= sql_zengo_open(fb, "wb", SQL_PATH);
	while (1){
		sz = fread(buf, 1, 1024*64, fi);
		if (sz < 1){
			ret = 0;
			break;
		}
		wt = fwrite(buf, 1, sz, fo);
		if (wt != sz)break;
	}
	if (fi)fclose(fi);
	if (fo)fclose(fo);
	return ret;
}



static int cmd_delete(const char* str, void* out, int sz)
{
	int ret = -1, ct = 1, r;
	char tag[256];
	char eq[1024];
	char file1[256];
	char file2[256];
	char buf[4096];
	char buf2[4096];
	char col[256];
	char val1[1024];
	char val2[1024];
	int ok=0;
	int mode_where = 0;

	FILE *fp = NULL, *fq = NULL;
	char *p;
	int coln = -1, i;

 	while (1){
		r = sql_get_tag(str, ct, tag, sizeof(tag));
		if (r < 0)return -1;
		if (strcmp(tag, "FROM") == 0)break;
		ct++;
	}
	r = sql_get_tag(str, ct + 1, tag, sizeof(tag));
	if (r < 0)return -1;
	if (strlen(tag)>200)return ret;
	mystrlwr(tag);
	strcpy(file1,tag);
	strcpy(file2, tag);
	strcat(file1, ".csv");
	strcat(file2, "_.csv");


	p = (char*)sql_get_tag_pos(str, "where");
	if (p == NULL)return ret;
	if (strlen(p) > 1000)return ret;
	strcpy(eq, p);
	replace_space(eq);


	col[0] = 0;
	val1[0] = 0;
	r = get_a_is_b(eq, col, sizeof(col), val1, sizeof(val1));
	if (r == 0)mode_where = 1;
	if (mode_where == 0){
		// TODO analize where
	}
	if (mode_where == 0)return ret;


	fp = sql_zengo_open(file1, "rb", SQL_PATH);
	if (fp == NULL)goto err;
	fq = sql_zengo_open(file2, "wb", SQL_PATH);
	if (fq == NULL)goto err;


	buf[0] = 0;
	if (fgets(buf, sizeof(buf), fp) == NULL)goto err;
	r = fputs(buf, fq);
	if(r< 0)goto err;

#ifdef I_USE_EXCEL
	{
		unsigned short uni[4096];
		kj_sjis_to_unicode_n(buf, uni,sizeof(uni));
		kj_unicode_to_utf8_n(uni, buf,sizeof(buf));
	}
#endif
	ct = sql_get_param_num(buf);

	if (mode_where == 1){
		coln = -1;
		for (i = 0; i < ct; i++){
			tag[0] = 0;
			csvread_copynstring(buf, tag, sizeof(tag), i);
			mystrupr(tag);
			if (strcmp(tag, col) == 0){
				coln = i;
				break;
			}
		}

		if (coln == -1)goto err;
	}
	else if (mode_where == 2){
		return -1;
	}
	while (1){
		buf[0] = 0;
		buf2[0] = 0;
		if (fgets(buf, sizeof(buf), fp) == NULL){
			break;
		}
		if (mode_where == 1){
			val2[0] = 0;
			strcpy(buf2, buf);
			csvread_copynstring(buf2, val2, sizeof(val2), coln);
			if (strcmp(val1, val2) == 0){
				ok = 1;
				continue;
			}
		}
		else if (mode_where == 2){
			goto err;
		}

		r = fputs(buf, fq);
		if (r < 0)goto err;
	}
	if (ok == 0)goto err;

	fclose(fp);
	fclose(fq);

	ret=path_copy(file2,file1);
	sql_zengo_remove(file2,SQL_PATH);
	return 0;


err:
	if (fp)fclose(fp);
	if (fq)fclose(fq);
	sql_zengo_remove(file2, SQL_PATH);
	return ret;
}


static int cmd_update(const char* str, void* out, int sz)
{
	int ret = -1, ct = 0, r;
	char tag[256];
	char eq[1024];
	char file1[256];
	char file2[256];
	char buf[4096];
	char buf2[4096];
	char col[256];
	char val1[1024];
	char val2[1024];
	char key[MAX_COL][256];
	char val[MAX_COL][1024];
	int key_col[MAX_COL];
	int key_rcol[MAX_COL];
	int mode_where = 0;

	int ok = 0;
	int ids = 0;

	FILE *fp = NULL, *fq = NULL;
	char *p, *st;
	int coln, i,j;

	for (i = 0; i < MAX_COL; i++){
		key_col[i] = -1;
		key_rcol[i] = -1;
	}
	memset(key, 0, sizeof(key));
	memset(val, 0, sizeof(val));

	r = sql_get_tag(str, 1, tag, sizeof(tag));
	if (r < 0)return -1;
	if (strlen(tag)>200)return ret;
	mystrlwr(tag);
	strcpy(file1, tag);
	strcpy(file2, tag);
	strcat(file1, ".csv");
	strcat(file2, "_.csv");

	st = (char*)sql_get_tag_pos(str, "set");
	if (st == NULL)return ret;

	while (1){
		eq[0] = 0;
		r = sql_get_param(st, ct, eq, sizeof(eq));
		if (r < 0)break;
		p = strstr(eq, "=");
		if (p == NULL)break;

		*p = 0;
		p++;

		if (strlen(st) + 1 >= sizeof(key[ct]))return ret;
		if (strlen(p) + 1 >= sizeof(val[ct]))return ret;

		strcpy(key[ct], eq);
		delete_space(key[ct]);
		mystrupr(key[ct]);
		strcpy(val[ct], p);
		delete_space(val[ct]);
		ct++;
	}
	ids = ct;
	if (ids<1)return ret;


	p = (char*)sql_get_tag_pos(str, "where");
	if (p == NULL)return ret;
	if (strlen(p) > 1000)return ret;
	strcpy(eq, p);
	replace_space(eq);

	col[0] = 0;
	val1[0] = 0;
	r = get_a_is_b(eq, col, sizeof(col), val1, sizeof(val1));
	if (r == 0)mode_where = 1;
	if (mode_where == 0){
		// TODO analize where
	}
	if (mode_where == 0)return ret;


	fp = sql_zengo_open(file1, "rb", SQL_PATH);
	if (fp == NULL)goto err;
	fq = sql_zengo_open(file2, "wb", SQL_PATH);
	if (fq == NULL)goto err;


	buf[0] = 0;
	if (fgets(buf, sizeof(buf), fp) == NULL)goto err;
	r = fputs(buf, fq);
	if (r < 0)goto err;

#ifdef I_USE_EXCEL
	{
		unsigned short uni[4096];
		kj_sjis_to_unicode_n(buf, uni,sizeof(uni));
		kj_unicode_to_utf8_n(uni, buf,sizeof(buf));
	}
#endif

	ct = sql_get_param_num(buf);

	if (mode_where == 1){
		for (i = 0; i < ct; i++){
			tag[0] = 0;
			csvread_copynstring(buf, tag, sizeof(tag), i);
			mystrupr(tag);
			if (strcmp(tag, col) == 0){
				coln = i;
				break;
			}
		}
		if (coln == -1)goto err;
	}
	else if (mode_where == 2){
		return ret;
	}

	for (j = 0; j < ids; j++){
		for (i = 0; i < ct; i++){
			tag[0] = 0;
			csvread_copynstring(buf, tag, sizeof(tag), i);
			mystrupr(tag);
			if (strcmp(tag, key[j]) == 0){
				key_col[j] = i;
				key_rcol[i] = j;
				break;
			}
		}
		if (key_col[j] == -1)goto err;
	}

	//for (i = 0; i < 2; i++){
	//	printf("key[%d]=%s\n", i, key[i]);
	//	printf("val[%d]=%s\n", i, val[i]);
	//	printf("key_col[%d]=%d\n", i, key_col[i]);
	//}

	//for (i = 0; i < 2; i++){
	//	printf("key_rcol[%d]=%d\n",i,key_rcol[i]);
	//}

	while (1){
		buf[0] = 0;
		buf2[0] = 0;
		if (fgets(buf, sizeof(buf), fp) == NULL){
			break;
		}
		if (mode_where == 1){
			val2[0] = 0;
			strcpy(buf2, buf);
			csvread_copynstring(buf2, val2, sizeof(val2), coln);
			if (strcmp(val1, val2) == 0){
				ok = 1;
				for (i = 0; i < ct; i++){
					if (i != 0)fprintf(fq, ",");
					eq[0] = 0;
					if (key_rcol[i] == -1){
						csvread_copynstring(buf2, eq, sizeof(eq), i);
					}
					else{
						strcpy(eq, val[key_rcol[i]]);
					}
#ifdef I_USE_EXCEL
					{
						unsigned short aaa[1024];
						kj_utf8_to_unicode_n(eq, aaa, sizeof(aaa));
						kj_unicode_to_cp932_n(aaa, eq, sizeof(eq));
					}
#endif
					fprintf(fq, "%s", eq);
				}
				fprintf(fq, "\n");
				continue;
			}
		}
		else if (mode_where == 2){
			goto err;
		}
		r = fputs(buf, fq);
		if (r < 0)goto err;
	}
	if (ok == 0)goto err;

	fclose(fp);
	fclose(fq);

	ret = path_copy(file2, file1);
	sql_zengo_remove(file2,SQL_PATH);
	
	return 0;

err:
	if (fp)fclose(fp);
	if (fq)fclose(fq);
	sql_zengo_remove(file2, SQL_PATH);
	return ret;
}




//
// execute
//

struct sql_cmdlist{
	char* cmd;
	int(*f)(const char*, void* out, int sz);
};

static struct sql_cmdlist list[] = {
	{"DUMMY",cmd_dummy},
	{ "CREATE", cmd_create },
	{ "DROP", cmd_drop },
	{ "INSERT", cmd_insert },
	{ "SELECT", cmd_select },
	{ "DELETE", cmd_delete },
	{ "UPDATE", cmd_update },
	{ NULL, NULL }
};

int sql_exec(const char* str, void* out, int sz)
{
	char tmp[256];
	int ret = -1, r = 0,ct=0;


	r = sql_get_tag(str, 0, tmp, sizeof(tmp));


	if (r < 0)return ret;
	while (1){
		if (list[ct].cmd == NULL)break;
		if (list[ct].f == NULL)break;
		if (strcmp(list[ct].cmd, tmp) == 0){
			ret = (*(list[ct].f))(str, out, sz);
			break;
		}
		ct++;
	}
	return ret;
}

//
// interface
//

static void clear_child(HCSVSQL hdb)
{
	int i;
	if (hdb == NULL)return;
	if (hdb->child == NULL)return;
	if (hdb->child && hdb->child->fp){
		fclose(hdb->child->fp);
		hdb->child->fp = NULL;
	}
	for (i = 0; i < MAX_COL; i++){
		if (hdb->child->ret_str[i]){
			free(hdb->child->ret_str[i]);
			hdb->child->ret_str[i] = NULL;
		}
	}
	if (hdb->child->str)free(hdb->child->str);
	if (hdb->child)free(hdb->child);
	hdb->child = NULL;

}
HCSVSQL csvsql_connect(const char* server, const char* user, const char* pass, const char* dbname)
{
	HCSVSQL ret = NULL;

	ret = malloc(sizeof(struct sqlcsv_handle));
	if (ret == NULL)return ret;
	memset(ret,0,sizeof(struct sqlcsv_handle));

	return ret;
}
void csvsql_close(HCSVSQL hdb)
{
	if (hdb == NULL)return;

	clear_child(hdb);
	free(hdb);
}

int csvsql_exec(HCSVSQL hdb, const char* str)
{
	if (hdb == NULL || str == NULL)return -1;
	clear_child(hdb);
	return sql_exec(str, NULL, 0);
}

HCSCOL csvsql_prepare(HCSVSQL hdb, const char* str)
{
	HCSCOL ret = NULL;
	int r;
	if (hdb == NULL || str==NULL)return ret;

	clear_child(hdb);

	ret = malloc(sizeof(struct sqlcsv_result));
	if (ret == NULL)return NULL;
	memset(ret, 0, sizeof(struct sqlcsv_result));

	ret->str = malloc(strlen(str)+10);
	strcpy(ret->str, str);

	ret->parent = hdb;
	ret->sz = sizeof(struct sqlcsv_handle);

	hdb->child = ret;
	r = sql_exec(str, hdb, sizeof(struct sqlcsv_handle));
	if (r < 0){
		clear_child(hdb);
		return NULL;
	}
	hdb->child = ret;
	return ret;
}
int csvsql_step(HCSCOL col)
{
	int ret;
	if (col == NULL)return 0;
	if (col->f == NULL) return 0;
	col->times++;
	ret = (col->f)(col->str, col->parent, col->sz);
	if (ret)return 0;
	return 1;
}
void csvsql_free_result(HCSCOL col)
{
	// nothing to do
}


int csvsql_column_int(HCSCOL col, int n)
{
	int ret = 0;
	int ct;
	if (col == NULL || n<0)return ret;

	col->tmp[0]=0;
	ct = sql_get_param_num(col->buf);
	if (n >= ct)return ret;

	csvread_copynstring(col->buf, col->tmp, sizeof(col->tmp),n);
	sscanf(col->tmp, "%d", &ret);
	return ret;
}
const char* csvsql_column_char(HCSCOL col, int n)
{
	const char* ret = NULL;
	int ct;
	char* p;

	if (col == NULL || n<0)return ret;

	col->tmp[0] = 0;
	ct = sql_get_param_num(col->buf);
	if (n >= ct)return ret;
	if (n >= MAX_COL)return ret;
	if (col->ret_str[n]){
		free(col->ret_str[n]);
		col->ret_str[n] = NULL;
	}
	csvread_copynstring(col->buf, col->tmp, sizeof(col->tmp), n);
	p = malloc(strlen(col->tmp) + 4);
	col->ret_str[n]=p;
	strcpy(p, col->tmp);
	return p;
}

//
// test
//

#if 0
int main()
{
	int ret;
	char buf[256];
	const char*p;


	p = endof_quot(" \" \"\"sssss\\\"\\\'g\"wwww");
	
	p = endof_quot(" \"  \"\"   \\     \'  \"sssss\\\"\\\'g\"wwww");



	p = endof_quot(" aaaa \" sssss\\\"\\\'g\"wwww");
	p = endof_quot(" \" sssss\\\"\\\'g\"wwww");
	p = endof_quot(" \' sssss\\\"\\\'g\'wwww");
	p = endof_bracket(" (\' sss)(ss\\\"\\\'g\'wwwwqqqq");
	p = endof_bracket(" (\' sss)(ss\\\"\\\'gwwwwqqqq");
	p = endof_bracket(" (\' sss)(ss\\\"\\\'g\'wwww)qqqq");



	ret = sql_get_tag("   create aaaa,bbbb,cccc    \"aaa\" bbb( ccc,\"qqq\") eee", 1, buf, 256);
	printf("ret=%d  buf=%s\n", ret, buf);
	ret = sql_get_tag("   create aaaa,bbbb,cccc    \"aaa\" bbb( ccc,\"qqq\") eee", 2, buf, 256);
	printf("ret=%d  buf=%s\n", ret, buf);
	ret = sql_get_tag("   create aaaa,bbbb,cccc    \"aaa\" bbb( ccc,\"qqq\") eee", 3, buf, 256);
	printf("ret=%d  buf=%s\n", ret, buf);
	ret = sql_get_tag("   create aaaa,bbbb,cccc    \"aaa\" bbb( ccc,\"qqq\") eee", 4, buf, 256);
	printf("ret=%d  buf=%s\n", ret, buf);



	ret = sql_get_tag("   create aaa    \"aaa\" bbb( ccc,) eee",0,buf,256);
	printf("ret=%d  buf=%s\n",ret,buf);

	ret = sql_get_tag("   create aaa    \"aaa\" bbb( ccc,\"qqq\") eee", 1, buf, 256);
	printf("ret=%d  buf=%s\n", ret, buf);

	ret = sql_get_tag("   create aaa    \"aaa\" bbb( ccc,\"qqq\") eee", 2, buf, 256);
	printf("ret=%d  buf=%s\n", ret, buf);

	ret = sql_get_tag("   create aaa    \"aaa\" bbb( ccc,\"qqq\") eee", 3, buf, 256);
	printf("ret=%d  buf=%s\n", ret, buf);

	ret = sql_get_tag("   create aaa    \"aaa\" bbb( ccc,\"qqq\") eee", 4, buf, 256);
	printf("ret=%d  buf=%s\n", ret, buf);


	ret = sql_get_tag_num("   create aaa    \"aaa\" bbb( ccc,\"qqq\") eee");
	printf("tag_num ret=%d  \n", ret);

	ret = sql_is_tag("   create aaa    \"aaa\" bbb( ccc,\"qqq\") eee", "aAa");
	printf("is tag ret=%d  \n", ret);

	ret = sql_is_tag("   create aaa    \"aaa\" bbb( ccc,\"qqq\") eee", "aA");
	printf("is tag ret=%d  \n", ret);

	ret = sql_exec("   create aaa    \"aaa\" bbb( ccc,\"qqq\") eee", buf, 256);
	printf("exec ret=%d  \n", ret);

	ret = sql_get_function("   create aaa    \"aaa\" bbb( ccc,\"qqq\") eee", buf, 256);
	printf("hikisu ret=%d  >><%s<<\n", ret,buf);

	ret = sql_get_function("   create aaa    bbb ccc,)", buf, 256);
	printf("hikisu ret=%d  >><%s<<\n", ret, buf);

	p = sql_get_tag_pos("   create aaa    bbb( ccc,) ddd", "bbb");
	printf("pos ret=>>%s<< \n", p);

	p = sql_get_tag_pos("   create aaa    bbb( ccc,) ddd", "bb");
	printf("pos ret=>>%s<< \n", p);


	ret = sql_get_param(" ss ss \",\"\',\'   ,,aaa,", 0, buf, 256);
	printf("ret=%d  buf=>>%s<<\n", ret, buf);

	ret = sql_get_param(" ss ss \",\"\',\'   ,,aaa,", 1, buf, 256);
	printf("ret=%d  buf=>>%s<<\n", ret, buf);

	ret = sql_get_param(" ss ss \",\"\',\'   ,,aaa,", 2, buf, 256);
	printf("ret=%d  buf=>>%s<<\n", ret, buf);

	ret = sql_get_param(" ss ss \",\"\',\'   ,,aaa,", 3, buf, 256);
	printf("ret=%d  buf=>>%s<<\n", ret, buf);

	ret = sql_get_param(" ss ss \",\"\',\'   ,,aaa,", 4, buf, 256);
	printf("ret=%d  buf=>>%s<<\n", ret, buf);

	ret = sql_get_param_num(" ss ss \",\"\',\'   ,,aaa,");
	printf("param_num ret=%d  \n", ret);

	return 0;
}

#endif

#if 1

int main()
{
	HCSVSQL conn = NULL;
	HCSCOL col = NULL;

	char *sql_serv = "localhost";
	char *user = "root";
	char *passwd = "wanted";
	char *db_name = "db_test";
	int ret;

#if defined(_WIN32) && !defined(__GNUC__)
	//	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_WNDW);
	//	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_WNDW);
	//	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_WNDW);
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif


	conn = csvsql_connect(sql_serv, user, passwd, db_name);
	if (conn == NULL) {
		printf("mydb_connect error\n");
		return 0;
	}

#if 0
	//
	// test drop
	//
	printf("%s\n", "DROP TABLE test");
	ret = csvsql_exec(conn, "DROP TABLE test");
	printf("ret=%d\n", ret);

	printf("%s\n", "DROP TABLE id exists test");
	ret = csvsql_exec(conn, "DROP TABLE IF EXISTS test");
	printf("ret=%d\n", ret);
#endif
#if 1

	//ret = csvsql_exec(conn, "CREATE TABLE IF NOT EXISTS test(id INTEGER,name CHAR)");
	//printf("ret=%d\n", ret);

	//ret = csvsql_exec(conn, "create table if not exists test(id integer,name char)");
	//printf("ret=%d\n", ret);

	printf("%s\n", "create table test");
	ret = csvsql_exec(conn, "CREATE TABLE test  (   id INTEGER   ,    name CHAR    )    ");
	printf("ret=%d\n", ret);


	if (ret == 0){

		ret = csvsql_exec(conn, "INSERT INTO test VALUES (    123   ,     \"hogehoge\"     )");
		printf("ret=%d\n", ret);

		ret = csvsql_exec(conn, "INSERT INTO test VALUES (456,\"hage  hage\")");
		printf("ret=%d\n", ret);

		ret = csvsql_exec(conn, "INSERT INTO test VALUES (789,\"aaaa\"\"bbbb\")");
		printf("ret=%d\n", ret);

		ret = csvsql_exec(conn, "INSERT INTO test VALUES (999,\"\"\"\")");
		printf("ret=%d\n", ret);
	}

	printf("%s\n", "SELECT max(id) from test");
	col = csvsql_prepare(conn, "SELECT max(   id   ) from test");
	if (col == NULL) {
		csvsql_close(conn);
		exit(-1);
	}
	while (csvsql_step(col)){
		printf("%d : %s\n", csvsql_column_int(col, 0), csvsql_column_char(col, 1));
	}
	csvsql_free_result(col);
	printf("\n");

	printf("%s\n", "SELECT * from test");
	col = csvsql_prepare(conn, "SELECT * from test");
	if (col == NULL) {
		csvsql_close(conn);
		exit(-1);
	}
	while (csvsql_step(col)){
		printf("%d : %s\n", csvsql_column_int(col, 0), csvsql_column_char(col, 1));
	}
	csvsql_free_result(col);
	printf("\n");

	printf("%s\n", "SELECT * from where id=123");
	col = csvsql_prepare(conn, "select * from test where     id    =   123    ");
	if (col == NULL) {
		csvsql_close(conn);
		exit(-1);
	}
	while (csvsql_step(col)){
		printf("%d : %s\n", csvsql_column_int(col, 0), csvsql_column_char(col, 1));
	}
	csvsql_free_result(col);
	printf("\n");

	printf("%s\n", "SELECT * from test where name=\"\"\"\"");
	col = csvsql_prepare(conn, "select * from test where     name    =   \"\"\"\"   ");
	if (col == NULL) {
		csvsql_close(conn);
		exit(-1);
	}
	while (csvsql_step(col)){
		printf("%d : %s\n", csvsql_column_int(col, 0), csvsql_column_char(col, 1));
	}
	csvsql_free_result(col);
	printf("\n");

	printf("%s\n", "SELECT * from test where name=\"aaaa\"");
	col = csvsql_prepare(conn, "select * from test where     name    =   \"aaaa\"   ");
	if (col == NULL) {
		csvsql_close(conn);
		exit(-1);
	}
	while (csvsql_step(col)){
		printf("%d : %s\n", csvsql_column_int(col, 0), csvsql_column_char(col, 1));
	}
	csvsql_free_result(col);
	printf("\n");


	printf("%s\n", "insert into  test name=\"\"\"\"");
	ret = csvsql_exec(conn, "INSERT INTO test VALUES (999,\"\"\"\")");
	printf("ret=%d\n", ret);
	printf("\n");

	//printf("%s\n", "delete from table where name=\"\"\"\"");
	//ret = csvsql_exec(conn, "delete  from  test    where     name    =   \"\"\"\"   ");
	//printf("ret=%d\n", ret);
	//printf("\n");


	printf("%s\n", "update test set id = 888   ,  name  =  \"aaaa\"     where name   =   \"\"\"\"");
	ret = csvsql_exec(conn, "update test  set   name   =   \"kawatta\"  ,  id  =   111   where name   =   \"\"\"\"");
	printf("ret=%d\n", ret);
	printf("\n");

	printf("%s\n", "update test set name=ok,id=222");
	ret = csvsql_exec(conn, "update test set   name=\"ok\",id=222 where id=111");
	printf("ret=%d\n", ret);
	printf("\n");
#endif

#if 0
	printf("%s\n", "create database sssss");
	ret = csvsql_exec(conn, "create database sssss");
	printf("ret=%d\n", ret);
	printf("\n");

	printf("%s\n", "drop database sssss");
	ret = csvsql_exec(conn, "drop database sssss");
	printf("ret=%d\n", ret);
	printf("\n");
#endif

#if 0
	//
	// test for excel csv 
	//
	printf("%s\n", "SELECT * from user_list where name=\"yomei\"");
	col = csvsql_prepare(conn, "select * from user_list where   mail     =   \"yomei.otani@gmail.com\"   ");
	if (col == NULL) {
		csvsql_close(conn);
		exit(-1);
	}
	while (csvsql_step(col)){
		printf("%s : %s\n", csvsql_column_char(col, 0), csvsql_column_char(col, 1));
	}
	printf("\n");
#endif

#if 0
	//
	// tedst for Kanji
	//
	printf("%s\n", "SELECT * from user_list where name=\"yomei\"");
	col = csvsql_prepare(conn, "select * from user_list where   \xe3\x83\xa1\xe3\x83\xbc\xe3\x83\xab     =   \"yomei.otani@gmail.com\"   ");
	if (col == NULL) {
		csvsql_close(conn);
		exit(-1);
	}
	while (csvsql_step(col)){
		printf("%s : %s\n", csvsql_column_char(col, 0), csvsql_column_char(col, 1));
	}
	printf("\n");

	printf("%s\n", "update user_list set id=\"aaa\" where name=\"yomei\"");
	ret = csvsql_exec(conn, "update user_list set id=\"aaa\" where   \xe3\x83\xa1\xe3\x83\xbc\xe3\x83\xab     =   \"yomei.otani@gmail.com\"   ");
	printf("ret=%d\n", ret);
	printf("\n");

	printf("%s\n", "delete from user_list where id =\"aaa\"");
	ret = csvsql_exec(conn, "delete from user_list where   id=\"aaa\"   ");
	printf("ret=%d\n", ret);
	printf("\n");
#endif


	printf("test end\n");
	csvsql_close(conn);

	return 0;
}



#endif

