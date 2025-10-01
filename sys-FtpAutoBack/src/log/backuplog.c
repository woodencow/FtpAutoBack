#include "backuplog.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <switch.h>
static FsFileSystem g_fs = {0};
static FsFile g_log_file = {0};
static int g_has_log_file = 0;
static s64 g_file_off = 0;

void backuplog_write(const char* msg) {
    if (g_has_log_file) {
        size_t len = strlen(msg);

        if (len) {
            char buf[128];
            if (msg[len - 1] != '\n') {
                snprintf(buf, sizeof(buf), "%s\n", msg);
                msg = buf;
                len = strlen(msg);
            }

            if (R_SUCCEEDED(fsFileWrite(&g_log_file, g_file_off, msg, len, FsWriteOption_Flush))) {
                g_file_off += len;
            }
        }
    }
}

void backuplog_fwrite(const char* fmt, ...) {
    if (g_has_log_file) {
        char buf[128];
        va_list va;
        va_start(va, fmt);
        vsnprintf(buf, sizeof(buf), fmt, va);
        va_end(va);
        backuplog_write(buf);
    }
}

void backuplog_init(void) {
    const char* path = "/AutoBack/backuplog.txt";
    char safe_buf[FS_MAX_PATH];
    sniprintf(safe_buf, sizeof(safe_buf), "%s", path);

    if (g_has_log_file) {
        return;
    }

    if (!R_SUCCEEDED(fsOpenSdCardFileSystem(&g_fs))) {
        return;
    }

    // 尝试打开现有文件，如果不存在则创建
    Result rc = fsFsOpenFile(&g_fs, safe_buf, FsOpenMode_Write | FsOpenMode_Append, &g_log_file);
    if (R_SUCCEEDED(rc)) {
        // 获取文件末尾位置
        s64 file_size = 0;
        if (R_SUCCEEDED(fsFileGetSize(&g_log_file, &file_size))) {
            g_file_off = file_size;
        } else {
            g_file_off = 0;
        }
        g_has_log_file = 1;
        return;
    }
    
    // 文件不存在，创建新文件
    if (R_SUCCEEDED(fsFsCreateFile(&g_fs, safe_buf, 0, 0))) {
        // 确保 /AutoBack 目录存在：不存在则创建
        Result dir_rc = fsFsCreateDirectory(&g_fs, "/AutoBack");
        if (!R_SUCCEEDED(dir_rc) && dir_rc != 0x402 /* FSERROR_PATH_ALREADY_EXISTS */) {
            fsFsClose(&g_fs);
            return;
        }
        if (R_SUCCEEDED(fsFsOpenFile(&g_fs, safe_buf, FsOpenMode_Write | FsOpenMode_Append, &g_log_file))) {
            g_file_off = 0;
            g_has_log_file = 1;
            return;
        }
    }
}

void backuplog_exit(void) {
    if (g_has_log_file) {
        fsFileFlush(&g_log_file);
        fsFileClose(&g_log_file);
        fsFsCommit(&g_fs);
        fsFsClose(&g_fs);
        g_has_log_file = 0;
        g_file_off = 0;
    }
}