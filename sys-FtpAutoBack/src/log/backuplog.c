#include "backuplog.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <switch.h>

// 备份日志文件路径常量
static const char* BACKUP_LOG_PATH = "/AutoBack/backuplog.txt";


// 按需打开文件写入日志
// 按需打开文件写入日志
void backuplog_write(const char* msg) {

    FsFileSystem fs = {0};
    // 打开SD卡文件系统
    if (R_FAILED(fsOpenSdCardFileSystem(&fs))) {
        return;  // SD卡文件系统打开失败
    }
    
    FsFile log_file = {0};
    // 尝试直接打开文件进行追加写入
    Result rc = fsFsOpenFile(&fs, BACKUP_LOG_PATH, FsOpenMode_Write | FsOpenMode_Append, &log_file);
    if (R_FAILED(rc)) {
        // 文件不存在，调用ensure_file函数创建
        if (!backuplog_ensure_file(&fs)) {
            fsFsClose(&fs);
            return;
        }
        
        // 重新尝试打开文件
        rc = fsFsOpenFile(&fs, BACKUP_LOG_PATH, FsOpenMode_Write | FsOpenMode_Append, &log_file);
        if (R_FAILED(rc)) {
            fsFsClose(&fs);
            return;
        }
    }

    // 准备写入内容，确保以换行符结尾
    size_t len = strlen(msg);
    char buf[128];
    if (msg[len - 1] != '\n') {
        snprintf(buf, sizeof(buf), "%s\n", msg);
        msg = buf;
        len = strlen(msg);
    }

    // 获取文件大小，用于追加写入
    s64 file_size = 0;
    fsFileGetSize(&log_file, &file_size);
    
    // 写入内容并立即刷新（写入到文件末尾）
    fsFileWrite(&log_file, file_size, msg, len, FsWriteOption_Flush);
    // 关闭文件和文件系统
    fsFileClose(&log_file);
    fsFsClose(&fs);
}

void backuplog_fwrite(const char* fmt, ...) {
    char buf[128];
    va_list va;
    va_start(va, fmt);
    vsnprintf(buf, sizeof(buf), fmt, va);
    va_end(va);
    backuplog_write(buf);
}

void backuplog_init(void) {
    // 初始化检查文件是否存在
    FsFileSystem temp_fs = {0};
    if (R_FAILED(fsOpenSdCardFileSystem(&temp_fs))) {
        return;  // SD卡文件系统打开失败
    }
    
    backuplog_ensure_file(&temp_fs);
    
    // 关闭临时文件系统
    fsFsClose(&temp_fs);
}

bool backuplog_ensure_file(FsFileSystem* fs) {
    // 确保目录存在
    Result rc = fsFsCreateDirectory(fs, "/AutoBack");
    if (R_FAILED(rc) && rc != 0x402) {  // 0x402 表示目录已存在
        return false;  // 创建目录失败
    }
    
    // 创建日志文件（如果不存在）
    rc = fsFsCreateFile(fs, BACKUP_LOG_PATH, 0, 0);
    if (R_FAILED(rc) && rc != 0x402) {  // 0x402 表示文件已存在
        return false;  // 创建文件失败
    }
    
    return true;  // 创建成功
}
