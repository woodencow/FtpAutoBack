/**
 * @file backuplog.h
 * @brief 存档备份日志记录模块
 * 
 * 提供专门用于记录存档备份操作的日志功能
 * 日志文件路径: /AutoBack/backuplog.txt
 */

#ifndef __BACKUPLOG_H__
#define __BACKUPLOG_H__

#ifdef __cplusplus
extern "C" {
#endif

void backuplog_write(const char* msg);
void backuplog_fwrite(const char* fmt, ...);
void backuplog_init(void);
void backuplog_exit(void);

#ifdef __cplusplus
}
#endif

#endif