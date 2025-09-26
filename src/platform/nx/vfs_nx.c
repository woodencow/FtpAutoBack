/**
 * Copyright 2024 TotalJustice.
 * SPDX-License-Identifier: MIT
 * 
 * Nintendo Switch 虚拟文件系统实现
 * 提供统一的文件系统接口，支持多种存储设备和文件系统类型
 */

#include "ftpsrv_vfs.h"
#include "log/log.h"
#include "utils.h"

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define NCM_SIZE 2          // NCM 存储数量（SD卡和内置存储）
#define DEVICE_NUM 32       // 最大设备数量

// 全局变量定义
static bool g_enabled_devices = false;                     // 是否启用设备挂载
static NcmContentStorage g_cs[NCM_SIZE];                   // NCM 内容存储服务
static NcmContentMetaDatabase g_db[NCM_SIZE];              // NCM 内容元数据数据库
static struct VfsDeviceEntry g_device[DEVICE_NUM];         // 设备条目数组
static enum VFS_TYPE g_device_type[DEVICE_NUM];            // 设备类型数组
static u32 g_device_count;                                 // 当前设备数量
static bool g_skip_ascii_convert = false;                  // 是否跳过ASCII字符转换

// VFS 类型到操作函数的映射表
static const FtpVfs* g_vfs[] = {
    [VFS_TYPE_NONE] = &g_vfs_none,          // 无效类型
    [VFS_TYPE_ROOT] = &g_vfs_root,          // 根目录
    [VFS_TYPE_FS] = &g_vfs_fs,              // 文件系统
#if USE_VFS_SAVE
    [VFS_TYPE_SAVE] = &g_vfs_save,          // 存档系统
#endif
#if USE_VFS_STORAGE
    [VFS_TYPE_STORAGE] = &g_vfs_storage,    // 存储系统
#endif
#if USE_VFS_GC
    [VFS_TYPE_GC] = &g_vfs_gc,              // 游戏卡
#endif
#if USE_VFS_USBHSFS
    [VFS_TYPE_STDIO] = &g_vfs_stdio,        // 标准IO（ROM文件系统）
    [VFS_TYPE_HDD] = &g_vfs_hdd,            // USB硬盘
#endif
    [VFS_TYPE_USER] = NULL,                 // 用户自定义类型
};

/**
 * 检查路径是否匹配指定的设备名称
 * @param path 要检查的路径
 * @param name 设备名称
 * @return 如果匹配返回true，否则返回false
 */
static bool is_path(const char* path, const char* name) {
    if (path[0] == '/') {
        return !strncmp(path + 1, name, strlen(name));
    } else {
        return !strncmp(path, name, strlen(name));
    }
}

/**
 * 根据路径获取VFS类型
 * @param path 文件路径
 * @return 对应的VFS类型
 */
static enum VFS_TYPE get_type(const char* path) {
    if (!g_enabled_devices) {
        return VFS_TYPE_FS;
    } else {
        if (!path || !strcmp(path, "/")) {
            return VFS_TYPE_ROOT;
        } else if (strchr(path, ':')) {
            for (u32 i = 0; i < g_device_count; i++) {
                if (is_path(path, g_device[i].name)) {
                    return g_device_type[i];
                }
            }
        } else {
            return VFS_TYPE_FS;
        }

        return VFS_TYPE_NONE;
    }
}

/**
 * 修正路径格式以适配不同的VFS类型
 * @param path 原始路径
 * @param type VFS类型
 * @return 修正后的路径
 */
static const char* fix_path(const char* path, enum VFS_TYPE type) {
    switch (type) {
        case VFS_TYPE_NONE: return NULL;
        case VFS_TYPE_ROOT: return "/";
        default:
            if (strchr(path, ':') && path[0] == '/') {
                return path + 1;
            }
            return path;
    }
}

/**
 * 打开文件
 * @param f 文件结构体指针
 * @param path 文件路径
 * @param mode 打开模式
 * @return 成功返回0，失败返回负数
 */
int ftp_vfs_open(struct FtpVfsFile* f, const char* path, enum FtpVfsOpenMode mode) {
    f->type = get_type(path);
    return g_vfs[f->type]->open(&f->root, fix_path(path, f->type), mode);
}

/**
 * 读取文件数据
 * @param f 文件结构体指针
 * @param buf 缓冲区
 * @param size 读取大小
 * @return 实际读取的字节数，失败返回负数
 */
int ftp_vfs_read(struct FtpVfsFile* f, void* buf, size_t size) {
    return g_vfs[f->type]->read(&f->root, buf, size);
}

/**
 * 写入文件数据
 * @param f 文件结构体指针
 * @param buf 数据缓冲区
 * @param size 写入大小
 * @return 实际写入的字节数，失败返回负数
 */
int ftp_vfs_write(struct FtpVfsFile* f, const void* buf, size_t size) {
    return g_vfs[f->type]->write(&f->root, buf, size);
}

/**
 * 文件定位
 * @param f 文件结构体指针
 * @param buf 缓冲区（某些实现可能需要）
 * @param size 大小
 * @param off 偏移量
 * @return 成功返回0，失败返回负数
 */
int ftp_vfs_seek(struct FtpVfsFile* f, const void* buf, size_t size, size_t off) {
    return g_vfs[f->type]->seek(&f->root, buf, size, off);
}

/**
 * 关闭文件
 * @param f 文件结构体指针
 * @return 成功返回0，失败返回负数
 */
int ftp_vfs_close(struct FtpVfsFile* f) {
    const enum VFS_TYPE type = f->type;
    f->type = VFS_TYPE_NONE;
    return g_vfs[type]->close(&f->root);
}

/**
 * 检查文件是否已打开
 * @param f 文件结构体指针
 * @return 已打开返回非0，否则返回0
 */
int ftp_vfs_isfile_open(struct FtpVfsFile* f) {
    return g_vfs[f->type]->isfile_open(&f->root);
}

/**
 * 打开目录
 * @param f 目录结构体指针
 * @param path 目录路径
 * @return 成功返回0，失败返回负数
 */
int ftp_vfs_opendir(struct FtpVfsDir* f, const char* path) {
    f->type = get_type(path);
    return g_vfs[f->type]->opendir(&f->root, fix_path(path, f->type));
}

/**
 * 读取目录条目
 * @param f 目录结构体指针
 * @param entry 目录条目结构体指针
 * @return 条目名称，结束时返回NULL
 */
const char* ftp_vfs_readdir(struct FtpVfsDir* f, struct FtpVfsDirEntry* entry) {
    return g_vfs[f->type]->readdir(&f->root, &entry->root);
}

/**
 * 获取目录条目的状态信息
 * @param f 目录结构体指针
 * @param entry 目录条目
 * @param path 路径
 * @param st 状态信息结构体
 * @return 成功返回0，失败返回负数
 */
int ftp_vfs_dirlstat(struct FtpVfsDir* f, const struct FtpVfsDirEntry* entry, const char* path, struct stat* st) {
    return g_vfs[f->type]->dirlstat(&f->root, &entry->root, fix_path(path, f->type), st);
}

/**
 * 关闭目录
 * @param f 目录结构体指针
 * @return 成功返回0，失败返回负数
 */
int ftp_vfs_closedir(struct FtpVfsDir* f) {
    const enum VFS_TYPE type = f->type;
    f->type = VFS_TYPE_NONE;
    return g_vfs[type]->closedir(&f->root);
}

/**
 * 检查目录是否已打开
 * @param f 目录结构体指针
 * @return 已打开返回非0，否则返回0
 */
int ftp_vfs_isdir_open(struct FtpVfsDir* f) {
    return g_vfs[f->type]->isdir_open(&f->root);
}

/**
 * 获取文件/目录状态信息
 * @param path 路径
 * @param st 状态信息结构体
 * @return 成功返回0，失败返回负数
 */
int ftp_vfs_stat(const char* path, struct stat* st) {
    enum VFS_TYPE type = get_type(path);
    const char* dilem = strchr(path, ':');

    if (type != VFS_TYPE_NONE && dilem && (!strcmp(dilem, ":") || !strcmp(dilem, ":/"))) {
        return g_vfs[VFS_TYPE_ROOT]->stat(fix_path(path, type), st);
    }

    return g_vfs[type]->stat(fix_path(path, type), st);
}

/**
 * 获取链接状态信息（当前实现与stat相同）
 * @param path 路径
 * @param st 状态信息结构体
 * @return 成功返回0，失败返回负数
 */
int ftp_vfs_lstat(const char* path, struct stat* st) {
    return ftp_vfs_stat(path, st);
}

/**
 * 创建目录
 * @param path 目录路径
 * @return 成功返回0，失败返回负数
 */
int ftp_vfs_mkdir(const char* path) {
    const enum VFS_TYPE type = get_type(path);
    return g_vfs[type]->mkdir(fix_path(path, type));
}

/**
 * 删除文件
 * @param path 文件路径
 * @return 成功返回0，失败返回负数
 */
int ftp_vfs_unlink(const char* path) {
    const enum VFS_TYPE type = get_type(path);
    return g_vfs[type]->unlink(fix_path(path, type));
}

/**
 * 删除目录
 * @param path 目录路径
 * @return 成功返回0，失败返回负数
 */
int ftp_vfs_rmdir(const char* path) {
    const enum VFS_TYPE type = get_type(path);
    return g_vfs[type]->rmdir(fix_path(path, type));
}

/**
 * 重命名/移动文件或目录
 * @param src 源路径
 * @param dst 目标路径
 * @return 成功返回0，失败返回负数
 */
int ftp_vfs_rename(const char* src, const char* dst) {
    const enum VFS_TYPE src_type = get_type(src);
    const enum VFS_TYPE dst_type = get_type(dst);
    if (src_type != dst_type) {
        errno = EXDEV; // 跨设备操作错误
        return -1;
    }

    return g_vfs[src_type]->rename(fix_path(src, src_type), fix_path(dst, dst_type));
}

/**
 * 读取符号链接（当前不支持）
 * @param path 链接路径
 * @param buf 缓冲区
 * @param buflen 缓冲区大小
 * @return 始终返回-1（不支持）
 */
int ftp_vfs_readlink(const char* path, char* buf, size_t buflen) {
    return -1;
}

/**
 * 获取用户名（固定返回"unknown"）
 * @param st 状态信息
 * @return 用户名字符串
 */
const char* ftp_vfs_getpwuid(const struct stat* st) {
    return "unknown";
}

/**
 * 获取组名（固定返回"unknown"）
 * @param st 状态信息
 * @return 组名字符串
 */
const char* ftp_vfs_getgrgid(const struct stat* st) {
    return "unknown";
}

// 语言代码到NACP语言索引的映射表

static const u32 g_nacpLanguageTable[18] = {
    [0]  = 2,    // 日语 (SetLanguage_JA)
    [1]  = 0,    // 美式英语 (SetLanguage_ENUS)
    [2]  = 3,    // 法语 (SetLanguage_FR)
    [3]  = 4,    // 德语 (SetLanguage_DE)
    [4]  = 7,    // 意大利语 (SetLanguage_IT)
    [5]  = 6,    // 西班牙语 (SetLanguage_ES)
    [6]  = 14,   // 简体中文 (SetLanguage_ZHCN)
    [7]  = 12,   // 韩语 (SetLanguage_KO)
    [8]  = 8,    // 荷兰语 (SetLanguage_NL)
    [9]  = 15,   // 葡萄牙语 (SetLanguage_PT)
    [10] = 11,   // 俄语 (SetLanguage_RU)
    [11] = 13,   // 繁体中文 (SetLanguage_ZHTW)
    [12] = 1,    // 英式英语 (SetLanguage_ENGB)
    [13] = 9,    // 加拿大法语 (SetLanguage_FRCA)
    [14] = 5,    // 拉丁美洲西班牙语 (SetLanguage_ES419)
    [15] = 14,   // 简体中文 (新)
    [16] = 13,   // 繁体中文 (新)
    [17] = 15,   // 巴西葡萄牙语 (新)
};


static u8 g_lang_index;  // 当前语言索引

/**
 * 从指定的NCM数据库和存储中获取应用程序名称（内部函数）
 * @param app_id 应用程序ID
 * @param db NCM内容元数据数据库指针
 * @param cs NCM内容存储指针
 * @param id 内容ID指针（输出参数）
 * @param name 应用程序名称结构体指针（输出参数）
 * @return 成功返回0，失败返回错误代码
 */
Result get_app_name2(u64 app_id, NcmContentMetaDatabase* db, NcmContentStorage* cs, NcmContentId* id, struct AppName* name) {
    Result rc;
    NcmContentMetaKey key;
    s32 entries_total;
    s32 entries_written;
    
    // 从数据库中查找应用程序的元数据
    if (R_FAILED(rc = ncmContentMetaDatabaseList(db, &entries_total, &entries_written, &key, 1, NcmContentMetaType_Application, app_id, 0, UINT64_MAX, NcmContentInstallType_Full))) {
        return rc;
    }

    // 获取控制数据的内容ID
    if (R_FAILED(rc = ncmContentMetaDatabaseGetContentIdByType(db, id, &key, NcmContentType_Control))) {
        return rc;
    }

    // 获取内容存储路径
    char nxpath[FS_MAX_PATH];
    if (R_FAILED(rc = ncmContentStorageGetPath(cs, nxpath, sizeof(nxpath), id))) {
        return rc;
    }

    // 打开控制数据文件系统
    FsFileSystem fs;
    if (R_FAILED(rc = fsOpenFileSystemWithId(&fs, key.id, FsFileSystemType_ContentControl, nxpath, FsContentAttributes_All))) {
        return rc;
    }

    // 打开control.nacp文件
    strcpy(nxpath, "/control.nacp");
    FsFile file;
    if (R_FAILED(rc = fsFsOpenFile(&fs, nxpath, FsOpenMode_Read, &file))) {
        fsFsClose(&fs);
        return rc;
    }

    // 初始化名称字符串
    name->str[0] = '\0';
    
    // 根据当前语言索引读取应用程序名称
    s64 off = g_lang_index * sizeof(NacpLanguageEntry);
    u64 bytes_read;
    rc = fsFileRead(&file, off, name->str, sizeof(name->str), 0, &bytes_read);
    
    // 如果当前语言没有名称，尝试其他语言
    if (name->str[0] == '\0') {
        for (int i = 0; i < 16; i++) {
            off = i * sizeof(NacpLanguageEntry);
            rc = fsFileRead(&file, off, name->str, sizeof(name->str), 0, &bytes_read);
            if (name->str[0] != '\0') {
                break;
            }
        }
    }

    // 关闭文件和文件系统
    fsFileClose(&file);
    fsFsClose(&fs);
    return rc;
}

/**
 * 获取应用程序名称（主函数）
 * 尝试从SD卡和内置存储中查找应用程序名称
 * @param app_id 应用程序ID
 * @param id 内容ID指针（输出参数）
 * @param name 应用程序名称结构体指针（输出参数）
 * @return 成功返回0，失败返回错误代码
 */
Result get_app_name(u64 app_id, NcmContentId* id, struct AppName* name) {
    Result rc;

    // 按常用程度排序的存储ID列表
    static const NcmStorageId ids[NCM_SIZE] = {
        NcmStorageId_SdCard,        // SD卡存储
        NcmStorageId_BuiltInUser,   // 内置用户存储
    };

    // 遍历所有存储位置
    for (int i = 0; i < NCM_SIZE; i++) {
        // 如果NCM内容存储服务未激活，则打开它
        // 在这里而不是启动时打开是因为NCM服务在启动时可能还未准备好
        if (!serviceIsActive(&g_cs[i].s)) {
            if (R_FAILED(rc = ncmOpenContentStorage(&g_cs[i], ids[i]))) {
                log_file_fwrite("failed: ncmOpenContentStorage() 0x%X\n", rc);
                continue;
            }
        }

        // 如果NCM内容元数据数据库未激活，则打开它
        if (!serviceIsActive(&g_db[i].s)) {
            if (R_FAILED(rc = ncmOpenContentMetaDatabase(&g_db[i], ids[i]))) {
                log_file_fwrite("failed: ncmOpenContentMetaDatabase() 0x%X\n", rc);
                continue;
            }
        }

        // 尝试从当前存储获取应用程序名称
        if (R_SUCCEEDED(rc = get_app_name2(app_id, &g_db[i], &g_cs[i], id, name))) {
            return rc;
        }
    }

    return rc;
}

/**
 * 替换字符串中的非法文件系统字符
 * 该函数将文件名中的非法字符替换为下划线，以确保文件名在文件系统中有效
 * 支持UTF-8编码，可选择仅处理ASCII字符或处理所有字符
 * 
 * @param str 要处理的字符串（会被就地修改）
 * @param ascii_only 是否仅处理ASCII字符
 *                   - true: 仅保留ASCII字符（0x00-0x7E），其他字符替换为下划线
 *                   - false: 保留所有有效UTF-8字符，仅替换控制字符和文件系统非法字符
 * 
 * 注意：如果全局变量g_skip_ascii_convert为true，则跳过所有处理
 * 
 * 非法字符包括：
 * - 控制字符（0x00-0x1F）
 * - DEL字符（0x7F，仅在ascii_only为false时）
 * - 文件系统非法字符：\ / : * ? " < > |
 * - 非ASCII字符（仅在ascii_only为true时）
 */
// taken from nxdumptool.
void utilsReplaceIllegalCharacters(char *str, bool ascii_only)
{
    // 文件系统中的非法字符列表
    static const char g_illegalFileSystemChars[] = "\\/:*?\"<>|";

    size_t str_size = 0, cur_pos = 0;

    // 检查输入参数有效性
    if (!str || !(str_size = strlen(str))) return;

    u8 *ptr1 = (u8*)str, *ptr2 = ptr1;  // ptr1: 读取指针, ptr2: 写入指针
    ssize_t units = 0;                   // UTF-8字符的字节数
    u32 code = 0;                        // Unicode码点
    bool repl = false;                   // 是否刚刚替换了字符（避免连续下划线）

    // 遍历字符串中的每个UTF-8字符
    while(cur_pos < str_size)
    {
        // 解码UTF-8字符，获取Unicode码点和字节数
        units = decode_utf8(&code, ptr1);
        if (units < 0) break;  // 解码失败，停止处理

        // 判断字符是否需要替换
        bool should_replace = false;
        
        // 始终替换文件系统非法字符，不受g_skip_ascii_convert影响
        if (units == 1 && memchr(g_illegalFileSystemChars, (int)code, sizeof(g_illegalFileSystemChars))) {
            should_replace = true;
        }
        // 如果没有设置跳过ASCII转换标志，则进行其他字符处理
        else if (!g_skip_ascii_convert) {
            if (code < 0x20 ||                                      // 控制字符（0x00-0x1F）
                (!ascii_only && code == 0x7F) ||                   // DEL字符（仅在非ASCII模式下）
                (ascii_only && code >= 0x7F)) {                    // 非ASCII字符（仅在ASCII模式下）
                should_replace = true;
            }
        }
        
        if (should_replace) {
            // 需要替换的字符：用下划线替换，但避免连续的下划线
            if (!repl)
            {
                *ptr2++ = '_';
                repl = true;
            }
        } else {
            // 有效字符：保留原字符
            if (ptr2 != ptr1) memmove(ptr2, ptr1, (size_t)units);  // 如果位置发生变化，移动字符
            ptr2 += units;
            repl = false;  // 重置替换标志
        }

        ptr1 += units;          // 移动读取指针
        cur_pos += (size_t)units;  // 更新当前位置
    }

    *ptr2 = '\0';  // 添加字符串结束符
}

/**
 * @brief 获取是否跳过ASCII字符转换的配置
 * 
 * @return bool 如果为true，则跳过ASCII字符转换；否则进行转换
 */
bool vfs_get_skip_ascii_convert(void)
{
    return g_skip_ascii_convert;
}

/**  过于高级注释掉了
 * BIS分区挂载条目结构体
 * 用于定义BIS分区的名称和对应的分区ID
 
struct MountEntry {
    const char* name;           // 分区名称
    FsBisPartitionId id;        // 分区ID
};
*/

/**   过于高级注释掉了
 * BIS分区挂载表
 * 定义了可挂载的BIS分区列表
 
static const struct MountEntry BIS_NAMES[] = {
    { "bis_calibration_file", FsBisPartitionId_CalibrationFile },  // 校准文件分区
    { "bis_safe_mode", FsBisPartitionId_SafeMode },                // 安全模式分区
    { "bis_user", FsBisPartitionId_User },                         // 用户分区
    { "bis_system", FsBisPartitionId_System },                     // 系统分区
};
*/

/**
 * 初始化Nintendo Switch虚拟文件系统
 * 该函数负责初始化VFS系统，挂载各种存储设备和文件系统
 * 
 * @param custom 自定义VFS路径配置（可为NULL）
 * @param enable_devices 是否启用设备挂载
 *                       - true: 启用多设备模式，挂载各种存储设备
 *                       - false: 仅使用基本文件系统模式
 * @param save_writable 存档系统是否可写
 *                      - true: 允许写入存档数据
 *                      - false: 存档数据只读
 * @param mount_bis 是否挂载BIS分区
 *                  - true: 挂载系统BIS分区（需要特殊权限）
 *                  - false: 不挂载BIS分区
 * @param skip_ascii_convert 是否跳过ASCII字符转换
 *                           - true: 保留原始字符，不进行转换
 *                           - false: 对非法字符进行转换处理
 * 
 * 挂载的设备包括：
 * - SD卡 (sdmc)
 * - 相册分区 (album_nand, album_sd)
 * - BIS存储 (bis) - 如果启用存储VFS
 * - BIS文件系统分区 - 如果启用BIS挂载
 * - 内容存储 (content_system, content_user, content_sdcard, content_system0)
 * - 自定义存储 (custom_system, custom_sd)
 * - 快捷方式 (switch, atmosphere_contents)
 * - 游戏卡 (gc) - 如果启用游戏卡VFS
 * - 存档系统 (save) - 如果启用存档VFS
 * - ROM文件系统 (romfs, romfs_qlaunch) - 如果启用USBHSFS
 * - USB硬盘 (hdd) - 如果启用USBHSFS
 * - 用户自定义设备 - 如果提供了custom参数
 */
void vfs_nx_init(const struct VfsNxCustomPath* custom, bool enable_devices, bool save_writable, bool mount_bis, bool skip_ascii_convert) {
    // 设置全局配置变量
    g_enabled_devices = enable_devices;
    g_skip_ascii_convert = skip_ascii_convert;

    // 如果启用设备挂载模式
    if (g_enabled_devices) {
        // 挂载SD卡
        vfs_nx_add_device("1. SD卡", VFS_TYPE_FS);

        // 挂载相册分区
        if (!fsdev_wrapMountImage("album_nand", FsImageDirectoryId_Nand)) {
            vfs_nx_add_device("6. 相册（正版）", VFS_TYPE_FS);
        }
        if (!fsdev_wrapMountImage("album_sd", FsImageDirectoryId_Sd)) {
            vfs_nx_add_device("5. 相册（虚拟）", VFS_TYPE_FS);
        }


/**      过于高级，直接注释掉不管
        // 挂载BIS文件系统分区（如果启用BIS挂载）
        if (mount_bis) {
            // 挂载BIS存储（如果启用存储VFS）
#if USE_VFS_STORAGE
            vfs_storage_init();
            vfs_nx_add_device("bis", VFS_TYPE_STORAGE);
#endif
            for (int i = 0; i < ARRAY_SIZE(BIS_NAMES); i++) {
                if (!fsdev_wrapMountBis(BIS_NAMES[i].name, BIS_NAMES[i].id)) {
                    vfs_nx_add_device(BIS_NAMES[i].name, VFS_TYPE_FS);
                }
            }
        }

        // 挂载内容存储文件系统
        FsFileSystem fs;
        // 系统内容存储
        if (R_SUCCEEDED(fsOpenContentStorageFileSystem(&fs, FsContentStorageId_System))) {
            fsdev_wrapMountDevice("content_system", NULL, fs, true);
            vfs_nx_add_device("content_system", VFS_TYPE_FS);
        }
        // 用户内容存储
        if (R_SUCCEEDED(fsOpenContentStorageFileSystem(&fs, FsContentStorageId_User))) {
            fsdev_wrapMountDevice("content_user", NULL, fs, true);
            vfs_nx_add_device("content_user", VFS_TYPE_FS);
        }
        // SD卡内容存储
        if (R_SUCCEEDED(fsOpenContentStorageFileSystem(&fs, FsContentStorageId_SdCard))) {
            fsdev_wrapMountDevice("content_sdcard", NULL, fs, true);
            vfs_nx_add_device("content_sdcard", VFS_TYPE_FS);
        }
        // 系统0内容存储
        if (R_SUCCEEDED(fsOpenContentStorageFileSystem(&fs, FsContentStorageId_System0))) {
            fsdev_wrapMountDevice("content_system0", NULL, fs, true);
            vfs_nx_add_device("content_system0", VFS_TYPE_FS);
        }

        // 挂载自定义存储文件系统
        // 系统自定义存储
        if (R_SUCCEEDED(fsOpenCustomStorageFileSystem(&fs, FsCustomStorageId_System))) {
            fsdev_wrapMountDevice("custom_system", NULL, fs, true);
            vfs_nx_add_device("custom_system", VFS_TYPE_FS);
        }
        // SD卡自定义存储
        if (R_SUCCEEDED(fsOpenCustomStorageFileSystem(&fs, FsCustomStorageId_SdCard))) {
            fsdev_wrapMountDevice("custom_sd", NULL, fs, true);
            vfs_nx_add_device("custom_sd", VFS_TYPE_FS);
        }
*/

        // 添加一些快捷方式目录
        FsFileSystem* sdmc = fsdev_wrapGetDeviceFileSystem("sdmc");
        if (sdmc) {
            // Switch目录快捷方式
            if (!fsdev_wrapMountDevice("3. 自制插件", "/switch", *sdmc, false)) {
                vfs_nx_add_device("3. 自制插件", VFS_TYPE_FS);
            }
            // Atmosphere内容目录快捷方式
            if (!fsdev_wrapMountDevice("2. 金手指&MOD", "/atmosphere/contents", *sdmc, false)) {
                vfs_nx_add_device("2. 金手指&MOD", VFS_TYPE_FS);
            }
        }

/**     过于高级，直接注释掉不管
        // 初始化游戏卡VFS（如果启用）
#if USE_VFS_GC
        if (R_SUCCEEDED(vfs_gc_init())) {
            vfs_nx_add_device("gc", VFS_TYPE_GC);
        }
#endif
*/
        // 初始化存档VFS（如果启用）
#if USE_VFS_SAVE
        vfs_save_init(save_writable);
        vfs_nx_add_device("4. 游戏存档", VFS_TYPE_SAVE);
#endif

/**     过于高级了这个，看不懂直接注释掉
        // 初始化USBHSFS相关功能（如果启用）（没有启用）
#if USE_VFS_USBHSFS
        // 挂载当前进程的ROM文件系统
        if (R_SUCCEEDED(romfsMountFromCurrentProcess("romfs"))) {
            vfs_nx_add_device("romfs", VFS_TYPE_STDIO);
        }

        // 挂载qlaunch的ROM文件系统
        if (R_SUCCEEDED(romfsMountDataStorageFromProgram(0x0100000000001000, "romfs_qlaunch"))) {
            vfs_nx_add_device("romfs_qlaunch", VFS_TYPE_STDIO);
        }

        // 初始化USB硬盘支持
        if (R_SUCCEEDED(vfs_hdd_init())) {
            vfs_nx_add_device("hdd", VFS_TYPE_HDD);
        }
#endif
*/
        // 添加用户自定义设备（如果提供）
        if (custom) {
            vfs_nx_add_device(custom->name, VFS_TYPE_USER);
            g_vfs[VFS_TYPE_USER] = custom->func;
        }

        // 初始化根目录VFS
        vfs_root_init(g_device, &g_device_count);

        // 获取系统语言设置并设置语言索引
        u64 LanguageCode;
        SetLanguage Language = SetLanguage_ENUS;  // 默认为美式英语
        if (R_SUCCEEDED(setGetSystemLanguage(&LanguageCode))) {
            if (R_SUCCEEDED(setMakeLanguage(LanguageCode, &Language))) {
                // 验证语言代码有效性
                if (Language < 0 || Language >= 18) {
                    Language = SetLanguage_ENUS;  // 无效时回退到英语
                }
            }
        }

        // 设置当前语言索引，用于应用程序名称本地化
        g_lang_index = g_nacpLanguageTable[Language];
    }
}

/**
 * 清理Nintendo Switch虚拟文件系统
 * 该函数负责清理VFS系统，卸载所有已挂载的设备和文件系统
 * 释放相关资源，恢复系统到未初始化状态
 * 
 * 清理操作包括：
 * - 退出游戏卡VFS
 * - 退出存储VFS
 * - 退出存档VFS
 * - 退出根目录VFS
 * - 卸载ROM文件系统
 * - 退出USB硬盘支持
 * - 关闭NCM内容存储和数据库服务
 * - 重置全局状态变量
 */
void vfs_nx_exit(void) {
    // 只有在启用设备模式时才需要清理
    if (g_enabled_devices) {

/**     过于高级了这个，看不懂直接注释掉
        // 退出游戏卡VFS（如果启用）
#if USE_VFS_GC
        vfs_gc_exit();
#endif
        // 退出存储VFS（如果启用）
#if USE_VFS_STORAGE
        vfs_storage_exit();
#endif
*/
        // 退出存档VFS（如果启用）
#if USE_VFS_SAVE
        vfs_save_exit();
#endif
        // 退出根目录VFS
        vfs_root_exit();

/**     过于高级了这个，看不懂直接注释掉
        // 清理USBHSFS相关资源（如果启用）
#if USE_VFS_USBHSFS
        romfsUnmount("romfs_qlaunch");  // 卸载qlaunch ROM文件系统
        romfsUnmount("romfs");          // 卸载当前进程ROM文件系统
        vfs_hdd_exit();                 // 退出USB硬盘支持
#endif
*/

        // 关闭NCM服务
        for (int i = 0; i < NCM_SIZE; i++) {
            ncmContentStorageClose(&g_cs[i]);      // 关闭内容存储服务
            ncmContentMetaDatabaseClose(&g_db[i]); // 关闭内容元数据数据库
        }

        // 重置全局状态
        g_enabled_devices = false;
    }
}

/**
 * 向VFS系统添加设备
 * 该函数将新设备注册到VFS系统中，使其可以通过FTP访问
 * 
 * @param name 设备名称（不包含冒号，函数会自动添加）
 * @param type VFS类型，指定设备的处理方式
 * 
 * 限制条件：
 * - 设备数量不能超过DEVICE_NUM（32个）
 * - 设备名称长度不能超过限制
 * 
 * 设备名称格式：
 * - 输入: "sdmc" -> 存储为: "sdmc:"
 * - 这样可以通过 "sdmc:/path" 的形式访问设备
 */
void vfs_nx_add_device(const char* name, enum VFS_TYPE type) {
    // 检查设备数量限制
    if (g_device_count >= DEVICE_NUM) {
        return;  // 已达到最大设备数量
    }

    // 检查设备名称长度（需要为冒号预留空间）
    if (strlen(name) >= sizeof(g_device[0].name) + 2) {
        return;  // 名称过长
    }

    // 添加设备到设备列表
    // 格式化设备名称，添加冒号后缀
    snprintf(g_device[g_device_count].name, sizeof(g_device[g_device_count].name), "%s:", name);
    g_device_type[g_device_count] = type;  // 设置设备类型
    g_device_count++;                      // 增加设备计数
}
