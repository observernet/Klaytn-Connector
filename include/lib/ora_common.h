#ifndef _ORA_COMMON_H
#define _ORA_COMMON_H

#define SQLCA_NONE

#include <sqlca.h>
#include <oraca.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ORACLE DB 관련 */
#define _SQLNOTOK			sqlca.sqlcode !=    0
#define _SQLERROR			sqlca.sqlcode != 0 && sqlca.sqlcode != 1403 && sqlca.sqlcode != -1405
#define _SQLERROR2			sqlca.sqlcode != 0 && sqlca.sqlcode != -1405
#define _SQLOK				sqlca.sqlcode ==    0
#define _SQLNOTUNIQUE		sqlca.sqlcode ==   -1
#define _SQLNOTFOUND		sqlca.sqlcode == 1403
#define _SQLNULL			sqlca.sqlcode == -1405
#define _SQLCONNECTERROR	sqlca.sqlcode == -3113

double	ora_temp_var_1;
double	ora_temp_var_2;
double	ora_temp_var_3;
double	ora_temp_var_4;
double	ora_temp_var_5;
double	ora_temp_var_6;

void ora_connect(char* userid);
void ora_disconnect();

char is_work_date(int date, char* exchange_cd, sql_context ctx);
int  get_work_date(int tdate, int days, char* exchange_cd, sql_context ctx);
int  get_work_days(int sdate, int edate, char* exchange_cd, sql_context ctx);

#ifdef __cplusplus
}
#endif

#endif
