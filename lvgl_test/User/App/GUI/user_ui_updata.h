#ifndef USER_UI_UPDATA_H
#define USER_UI_UPDATA_H
#include "sys_data.h"
void user_ui_updata(SYS_DataEventType_t event, const SystemGlobalData_t *p_data);
void user_ui_refresh_all(const SystemGlobalData_t *p_data);
#endif