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

#ifndef CSVSQL_H_
#define CSVSQL_H_

#include <stdio.h>

#define MAX_COL 30


#ifdef __cplusplus
extern "C"
{
#endif
	struct sqlcsv_result;

struct sqlcsv_handle{
	struct sqlcsv_result* child;
	char path[256];
};
	
struct sqlcsv_result{
	FILE* fp;
	int(*f)(const char*, void* out, int sz);
	char* str;
	struct sqlcsv_handle* parent;
	int sz;
	int times;
	char buf[4096];
	char tmp[4096];
	char key[256];
	char val[256];
	int key_pos;
	char* ret_str[MAX_COL];

	int mode_where;
	void* rpn_alanizer;
	char* rpn_str_id[MAX_COL];
	const char* rpn_gets;
};

typedef struct sqlcsv_handle* HCSVSQL;

typedef struct sqlcsv_result* HCSCOL;

HCSVSQL csvsql_connect(const char* server,const char* user,const char* pass,const char* dbname);
void csvsql_close(HCSVSQL hdb);

int csvsql_exec(HCSVSQL hdb,const char* str);

HCSCOL csvsql_prepare(HCSVSQL hdb, const char* str);
int csvsql_step(HCSCOL col);
void csvsql_free_result(HCSCOL col);

int csvsql_column_int(HCSCOL col,int n);
const char* csvsql_column_char(HCSCOL col,int n);


#ifdef __cplusplus
}
#endif

#endif /* CSVSQL_H_ */

