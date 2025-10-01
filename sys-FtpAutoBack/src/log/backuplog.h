/**
 * @file backuplog.h
 * @brief 存档备份日志记录模块
 * 
 * 提供专门用于记录存档备份操作的日志功能
 * 日志文件路径: /AutoBack/backuplog.txt
 */

#ifndef __BACKUPLOG_H__
#define __BACKUPLOG_H__

#include <switch.h>

#ifdef __cplusplus
extern "C" {
#endif

void backuplog_write(const char* msg);
void backuplog_fwrite(const char* fmt, ...);
void backuplog_init(void);  // 标准初始化函数，初始化全局文件系统
bool backuplog_ensure_file(FsFileSystem* fs);  // 创建日志目录和文件（内部函数）

#ifdef __cplusplus
}
#endif

#endif