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


#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "rpn_sql.h"

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


#define RPNSQL_TYPE_KAKKO_START 1
#define RPNSQL_TYPE_KAKKO_END 2
#define RPNSQL_TYPE_VALUE_STRING 3
#define RPNSQL_TYPE_VALUE_INT 4
#define RPNSQL_TYPE_OPERATOR 5
#define RPNSQL_TYPE_HENSU 6


//
// stinrg
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


//
// tokenizer
//
static const char* endof_quot(const char* str)
{
	const char *ret = NULL, *p = str, *st = NULL, *ed = NULL;

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


static void replace_quote(char* buf)
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

//
// global
//
HRPNSQL rpnsql_open()
{
	HRPNSQL ret = NULL;
	ret = malloc(sizeof(struct rpn_sql));
	if (ret == NULL)return ret;
	memset(ret, 0, sizeof(struct rpn_sql));
	return ret;
}

void rpnsql_close(HRPNSQL h)
{
	int i;
	if (h == NULL)return;
	for (i = 0; i < STACK_MAX; i++){
		if (h->token[i])free(h->token[i]);
		if (h->result[i])free(h->result[i]);
	}
	free(h);
}

static int add_token(HRPNSQL h,const char* str, int sz, int type)
{
	int ret = -1;
	if (h == NULL || str == NULL)return ret;
	if (h->token_pointer >= STACK_MAX)return ret;
	h->token[h->token_pointer] = malloc(sz + 2);
	if (h->token[h->token_pointer] == NULL)return ret;
	memcpy(h->token[h->token_pointer], str, sz);
	h->token[h->token_pointer][sz] = 0;
	if (type == RPNSQL_TYPE_HENSU){
		mystrupr(h->token[h->token_pointer]);
		if (strcmp(h->token[h->token_pointer], "AND") == 0)type = RPNSQL_TYPE_OPERATOR;
		if (strcmp(h->token[h->token_pointer], "OR") == 0)type = RPNSQL_TYPE_OPERATOR;
		if (strcmp(h->token[h->token_pointer], "NOT") == 0)type = RPNSQL_TYPE_OPERATOR;
	}
	h->token_type[h->token_pointer] = type;
	if (type == RPNSQL_TYPE_VALUE_STRING){
		replace_quote(h->token[h->token_pointer]);
	}
	h->token_pointer++;
	h->token[h->token_pointer] = NULL;
	ret = 0;
	return ret;
}

static void print_token(HRPNSQL h)
{
	int i=0;
	if (h == NULL)return;

	while (1){
		if (h->token[i] == NULL)break;
		printf("token[%d]=  (%d)  \"%s\" \n",i,h->token_type[i],h->token[i]);
		i++;
	}
	printf("\n");
}

int rpnsql_analize(HRPNSQL h,const char* str)
{
	int ret = -1,i;
	const char* p;

	if (h == NULL || str == NULL)return -1;
	for (i = 0; i<STACK_MAX; i++){
		if (h->token[i]){
			free(h->token[i]);
			h->token[i] = NULL;
		}
	}
	h->token_pointer = 0;

	while (*str){
		if (h->token_pointer >= STACK_MAX)return -1;

		if (*str && *str > 0 && *str <= ' ')str++;
		if (*str == '('){
			add_token(h,str, 1, RPNSQL_TYPE_KAKKO_START);
			str++;
			continue;
		}
		if (*str == ')'){
			add_token(h, str, 1, RPNSQL_TYPE_KAKKO_END);
			str++;
			continue;
		}
		if (*str == '\"' || *str == '\''){
			p=endof_quot(str);
			if (p == NULL)return -1;
			add_token(h, str+1,p-str-2, RPNSQL_TYPE_VALUE_STRING);
			str += p - str;
			continue;
		}
		if (*str >= '0' && *str <= '9'){
			p = endof_number(str);
			if (p == NULL)return -1;
			add_token(h, str, p - str, RPNSQL_TYPE_VALUE_INT);
			str += p - str;
			continue;

		}
		if (isoperator(*str)){
			p = endof_operator(str);
			if (p == NULL)return -1;
			add_token(h, str, p - str, RPNSQL_TYPE_OPERATOR);
			str += p - str;
			continue;
		}
		if (ishensu(*str)){
			p = endof_hensu(str);
			if (p == NULL)return -1;
			add_token(h, str, p - str, RPNSQL_TYPE_HENSU);
			str += p - str;
			continue;
		}
		str++;
	}
	print_token(h);
	ret = 0;

	return ret;
}

//
// RPN
//


static int push(HRPNSQL h,char *pToken,int type) {
	if (h == NULL)return -1;
	if (h->stack_pointer >= STACK_MAX)return -1;
	h->stack_type[h->stack_pointer] = type;
	h->stack[h->stack_pointer++] = pToken;
	return 0;
}

static char *pop(HRPNSQL h) {
	if (h == NULL)return NULL;
	return h->stack_pointer > 0 ? h->stack[--h->stack_pointer] : NULL;
}

static char *peek(HRPNSQL h) {
	if (h == NULL)return NULL;
	return h->stack_pointer > 0 ? h->stack[h->stack_pointer - 1] : NULL;
}

int	peek_type(HRPNSQL h) {
	if (h == NULL)return 0;
	return h->stack_pointer > 0 ? h->stack_type[h->stack_pointer - 1] : 0;
}

static int rank(char *op) {
	if (strcmp(op, "*") == 0)return 4;
	if (strcmp(op, "/") == 0)return 4;
	if (strcmp(op, "%") == 0)return 4;
	if (strcmp(op, "+") == 0)return 5;
	if (strcmp(op, "-") == 0)return 5;
	if (strcmp(op, "<") == 0)return 7;
	if (strcmp(op, "<=") == 0)return 7;
	if (strcmp(op, "=<") == 0)return 7;
	if (strcmp(op, ">") == 0)return 7;
	if (strcmp(op, ">=") == 0)return 7;
	if (strcmp(op, "=>") == 0)return 7;
	if (strcmp(op, "=") == 0)return 8;
	if (strcmp(op, "==") == 0)return 8;
	if (strcmp(op, "!=") == 0)return 8;
	if (strcmp(op, "AND") == 0)return 12;
	if (strcmp(op, "OR") == 0)return 13;
	return 99;
}


int rpnsql_convert(HRPNSQL h) 
{
	int n;
	char *pToken;
	int length;
	int type;
	int r;
	if (h == NULL)return -1;

	length = h->token_pointer;
	h->stack_pointer = 0;

	int nBuf = 0;

	for (n = 0; n < length; n++) {
		if ((h->token_type[n] == RPNSQL_TYPE_HENSU) ||
			(h->token_type[n] == RPNSQL_TYPE_VALUE_STRING) || 
			(h->token_type[n] == RPNSQL_TYPE_VALUE_INT)) {
			h->buffer_type[nBuf] = h->token_type[n];
			h->buffer[nBuf++] = h->token[n];
		}
		else if (h->token_type[n]==RPNSQL_TYPE_KAKKO_END) {
			while (1)
			{
				type = peek_type(h);
				pToken = pop(h);
				if (pToken == NULL)break;
				if (type == RPNSQL_TYPE_KAKKO_START)break;
				h->buffer_type[nBuf] = type;
				h->buffer[nBuf++] = pToken;
			};
			if (pToken == NULL){
				return -1;
			}
		}
		else if (h->token_type[n] == RPNSQL_TYPE_KAKKO_START) {
			r=push(h,h->token[n],h->token_type[n]);
			if (r < 0)return -1;
		}
		else if (peek(h) == NULL) {
			r=push(h,h->token[n],h->token_type[n]);
			if (r < 0)return -1;
		}
		else {
			while (peek(h) != NULL) {
				if (rank(h->token[n]) > rank(peek(h))) {
					h->buffer_type[nBuf] = peek_type(h);
					h->buffer[nBuf++] = pop(h);
				}
				else {
					r=push(h,h->token[n],h->token_type[n]);
					if (r < 0)return -1;
					break;
				}
			}
		}
	}
	while (1)
	{
		type = peek_type(h);
		pToken = pop(h);
		if (pToken == NULL)break;
		h->buffer_type[nBuf] = type;
		h->buffer[nBuf++] = pToken;
	}
	h->buffer_type[nBuf] = 0;
	h->buffer[nBuf++] = NULL;
	return 0;
}

static void print_buffer(HRPNSQL h)
{
	int i = 0;
	if (h == NULL)return;

	while (1){
		if (h->buffer[i] == NULL)break;
		printf("buffer[%d]=  (%d)  \"%s\" \n", i, h->buffer_type[i], h->buffer[i]);
		i++;
	};

	printf("\n");
}


static int opfunc(HRPNSQL h)
{
	const char *op, *v1, *v2;
	int type, t1, t2;
	const char *p1=NULL, *p2=NULL;

	type = peek_type(h);
	op = pop(h);

	t2 = peek_type(h);
	v2 = pop(h);

	t1 = peek_type(h);
	v1 = pop(h);



	if (t1 == RPNSQL_TYPE_HENSU){
		h->result[h->res_count] = malloc(1024);
		if (h->result[h->res_count] == NULL)return -1;
		h->result[h->res_count][0] = 0;
		t1 = h->getval(h->vp, v1,h->result[h->res_count], sizeof(h->result[h->res_count]));
		if (t1 < 0)return -1;
		p1 = h->result[h->res_count];
		h->res_count++;
	}
	else{
		p1 = v1;
	}
	if (t2 == RPNSQL_TYPE_HENSU){
		h->result[h->res_count] = malloc(1024);
		if (h->result[h->res_count] == NULL)return -1;
		h->result[h->res_count][0] = 0;
		t2 = h->getval(h->vp, v2, h->result[h->res_count], sizeof(h->result[h->res_count]));
		if (t2 < 0)return -1;
		p2 = h->result[h->res_count];
		h->res_count++;
	}
	else{
		p2 = v2;
	}
#ifdef DEBUG_PRINT
	printf("%s  %s  %s \n", p1, op, p2);
#endif

	h->result[h->res_count] = malloc(1024);
	h->result[h->res_count][0] = 0;


	if (strcmp(op, ";") == 0){
		int a = 0, b = 0,c=0;
		sscanf(p1, "%d", &a);
		sscanf(p2, "%d", &b);
		c = a+b;
		sprintf(h->result[h->res_count], "%d", c);
		h->result_type[h->res_count] = RPNSQL_TYPE_VALUE_INT;
	}
	else if (strcmp(op, "-") == 0){
		int a = 0, b = 0, c = 0;
		sscanf(p1, "%d", &a);
		sscanf(p2, "%d", &b);
		c = a-b;
		sprintf(h->result[h->res_count], "%d", c);
		h->result_type[h->res_count] = RPNSQL_TYPE_VALUE_INT;
	}
	else if (strcmp(op, "%") == 0){
		int a = 0, b = 0, c = 0;
		sscanf(p1, "%d", &a);
		sscanf(p2, "%d", &b);
		c = a % b;
		sprintf(h->result[h->res_count], "%d", c);
		h->result_type[h->res_count] = RPNSQL_TYPE_VALUE_INT;
	}
	else if (strcmp(op, "*") == 0){
		int a = 0, b = 0, c = 0;
		sscanf(p1, "%d", &a);
		sscanf(p2, "%d", &b);
		c = a * b;
		sprintf(h->result[h->res_count], "%d", c);
		h->result_type[h->res_count] = RPNSQL_TYPE_VALUE_INT;
	}
	else if (strcmp(op, "/") == 0){
		int a = 0, b = 0, c = 0;
		sscanf(p1, "%d", &a);
		sscanf(p2, "%d", &b);
		c = a / b;
		sprintf(h->result[h->res_count], "%d", c);
		h->result_type[h->res_count] = RPNSQL_TYPE_VALUE_INT;
	}
	else if ((strcmp(op, "=") == 0) || (strcmp(op, "==") == 0)){
		int c = 0;
		c = strcmp(p1, p2)==0?1:0;
		sprintf(h->result[h->res_count], "%d", c);
		h->result_type[h->res_count] = RPNSQL_TYPE_VALUE_INT;
	}
	else if (strcmp(op, "!=") == 0){
		int c = 0;
		c = strcmp(p1, p2) != 0 ? 1 : 0;
		sprintf(h->result[h->res_count], "%d", c);
		h->result_type[h->res_count] = RPNSQL_TYPE_VALUE_INT;
	}
	else if (strcmp(op, "AND") == 0){
		int a = 0, b = 0, c = 0;
		sscanf(p1, "%d", &a);
		sscanf(p2, "%d", &b);
		c = a && b;
		sprintf(h->result[h->res_count], "%d", c);
		h->result_type[h->res_count] = RPNSQL_TYPE_VALUE_INT;
	}
	else if (strcmp(op, "OR") == 0){
		int a = 0, b = 0, c = 0;
		sscanf(p1, "%d", &a);
		sscanf(p2, "%d", &b);
		c = a || b;
		sprintf(h->result[h->res_count], "%d", c);
		h->result_type[h->res_count] = RPNSQL_TYPE_VALUE_INT;
	}
	else if ((strcmp(op, "<=") == 0) || (strcmp(op, "=<") == 0)){
		int a = 0, b = 0, c = 0;
		sscanf(p1, "%d", &a);
		sscanf(p2, "%d", &b);
		c = (a <= b);
		sprintf(h->result[h->res_count], "%d", c);
		h->result_type[h->res_count] = RPNSQL_TYPE_VALUE_INT;
	}
	else if (strcmp(op, "<") == 0){
		int a = 0, b = 0, c = 0;
		sscanf(p1, "%d", &a);
		sscanf(p2, "%d", &b);
		c = (a < b);
		sprintf(h->result[h->res_count], "%d", c);
		h->result_type[h->res_count] = RPNSQL_TYPE_VALUE_INT;
	}
	else if ((strcmp(op, ">=") == 0) || (strcmp(op, "=>") == 0)){
		int a = 0, b = 0, c = 0;
		sscanf(p1, "%d", &a);
		sscanf(p2, "%d", &b);
		c = (a >= b);
		sprintf(h->result[h->res_count], "%d", c);
		h->result_type[h->res_count] = RPNSQL_TYPE_VALUE_INT;
	}
	else if (strcmp(op, ">") == 0){
		int a = 0, b = 0, c = 0;
		sscanf(p1, "%d", &a);
		sscanf(p2, "%d", &b);
		c = (a > b);
		sprintf(h->result[h->res_count], "%d", c);
		h->result_type[h->res_count] = RPNSQL_TYPE_VALUE_INT;
	}
	else {
		return -1;
	}

	//sprintf(h->result[h->res_count], "%s%d", "result", h->res_count);
	//h->result_type[h->res_count] = RPNSQL_TYPE_VALUE_INT;



	push(h, h->result[h->res_count], h->result_type[h->res_count]);

#ifdef DEBUG_PRINT
	printf("%s  %s  %s  =  %s\n", p1, op, p2, h->result[h->res_count]);
#endif

	h->res_count++;

	return 0;
}


int rpnsql_operation(HRPNSQL h, int(*getval)(void*,const char* name,char* buf,int sz), void* vp)
{
	const char* op;
	int type;
	int pt = 0;
	int r,i;

	if (h == NULL || getval==NULL)return -1;
	h->stack_pointer = 0;
	h->getval = getval;
	h->vp = vp;

	for (i = 0; i<STACK_MAX; i++){
		if (h->result[i]){
			free(h->result[i]);
			h->result[i] = NULL;
		}
	}
	h->res_count = 0;


	while (1){
		op = h->buffer[pt];
		type = h->buffer_type[pt];
		if (op == NULL)return -1;
		push(h, (char*)op, type);
		if (type == RPNSQL_TYPE_OPERATOR){
			r=opfunc(h);
			if (r < 0)return -1;
		}
		pt++;
	};
	return 0;
}

const char* rpnsql_get_result(HRPNSQL h)
{
	if (h == NULL)return NULL;
	return h->result[h->res_count-1];
}

int rpnsql_get_result_type(HRPNSQL h)
{
	if (h == NULL)return -1;
	return h->result_type[h->res_count-1];
}

//
// main
//

static int getval(void*vp, const char* name, char* buf, int sz)
{
	int ret = -1;
	if (strcmp(name, "A") == 0){
		sprintf(buf, "%d", 123);
		ret = RPNSQL_TYPE_VALUE_INT;
		return ret;
	}
	if (strcmp(name, "B") == 0){
		sprintf(buf, "%d", 456);
		ret = RPNSQL_TYPE_VALUE_INT;
		return ret;
	}
	if (strcmp(name, "C") == 0){
		sprintf(buf, "%d", 789);
		ret = RPNSQL_TYPE_VALUE_INT;
		return ret;
	}
	return ret;
}


#if 1
int main()
{
	HRPNSQL h;
	int ret;
	const char* str;

#if defined(_WIN32) && !defined(__GNUC__)
	//	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_WNDW);
	//	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_WNDW);
	//	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_WNDW);
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	h = rpnsql_open();
	
	//str = "( a+b*2+3 )";
	str = "( a<=123 ) and ( b>=45)";

	printf("str=%s\n",str);
	printf("\n");

	ret = rpnsql_analize(h, str);
	printf("ret=%d\n",ret);

	ret = rpnsql_convert(h);
	printf("ret=%d\n", ret);
	print_buffer(h);

	rpnsql_operation(h, getval,NULL);
	printf("\n");
	printf("result  =  %s  \n", rpnsql_get_result(h));
	printf("\n");

	rpnsql_operation(h, getval, NULL);
	printf("\n");
	printf("result  =  %s  \n", rpnsql_get_result(h));
	printf("\n");


	rpnsql_close(h);


	return 0;
}
#endif




