/**
 * Copyright 2024 TotalJustice.
 * SPDX-License-Identifier: MIT
 * 
 * vfs_nx_save.c - Nintendo Switch存档文件系统虚拟文件系统实现
 * 
 * 本文件实现了Switch存档文件系统的虚拟文件系统接口，主要功能包括：
 * 1. ZIP格式的存档文件创建和读取
 * 2. 存档文件系统的挂载和管理
 * 3. 用户账户和存档类型的处理
 * 4. 文件路径解析和转换
 * 5. VFS操作接口的实现
 */

#include "ftpsrv_vfs.h"
#include "vfs_nx_save.h"
#include "../utils.h"
#include "log/log.h"
#include "device_mapping.h"  // 设备名称映射支持
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define min(x, y) ((x) < (y) ? (x) : (y))

// ZIP文件格式相关的魔数定义
#define LOCAL_HEADER_SIG 0x4034B50      // 本地文件头签名
#define FILE_HEADER_SIG 0x2014B50       // 中央目录文件头签名
#define DATA_DESCRIPTOR_SIG 0x8074B50   // 数据描述符签名
#define END_RECORD_SIG 0x6054B50        // 中央目录结束记录签名

// ZIP本地文件头结构体
#pragma pack(push,1)
typedef struct mmz_LocalHeader {
    uint32_t sig;               // 签名 (0x04034b50)
    uint16_t version;           // 解压所需版本
    uint16_t flags;             // 通用位标志
    uint16_t compression;       // 压缩方法
    uint16_t modtime;           // 最后修改时间
    uint16_t moddate;           // 最后修改日期
    uint32_t crc32;             // CRC-32校验值
    uint32_t compressed_size;   // 压缩后大小
    uint32_t uncompressed_size; // 压缩前大小
    uint16_t filename_len;      // 文件名长度
    uint16_t extrafield_len;    // 扩展字段长度
} mmz_LocalHeader;
#pragma pack(pop)

// ZIP数据描述符结构体
#pragma pack(push,1)
typedef struct mmz_DataDescriptor {
    uint32_t sig;               // 签名 (0x08074b50)
    uint32_t crc32;             // CRC-32校验值
    uint32_t compressed_size;   // 压缩后大小
    uint32_t uncompressed_size; // 压缩前大小
} mmz_DataDescriptor;
#pragma pack(pop)

// ZIP中央目录文件头结构体
#pragma pack(push,1)
typedef struct mmz_FileHeader {
    uint32_t sig;               // 签名 (0x02014b50)
    uint16_t version;           // 压缩使用的版本
    uint16_t version_needed;    // 解压所需版本
    uint16_t flags;             // 通用位标志
    uint16_t compression;       // 压缩方法
    uint16_t modtime;           // 最后修改时间
    uint16_t moddate;           // 最后修改日期
    uint32_t crc32;             // CRC-32校验值
    uint32_t compressed_size;   // 压缩后大小
    uint32_t uncompressed_size; // 压缩前大小
    uint16_t filename_len;      // 文件名长度
    uint16_t extrafield_len;    // 扩展字段长度
    uint16_t filecomment_len;   // 文件注释长度
    uint16_t disk_start;        // 文件开始磁盘号
    uint16_t internal_attr;     // 内部文件属性
    uint32_t external_attr;     // 外部文件属性
    uint32_t local_hdr_off;     // 本地文件头相对偏移
} mmz_FileHeader;
#pragma pack(pop)

// ZIP中央目录结束记录结构体
#pragma pack(push,1)
typedef struct mmz_EndRecord {
    uint32_t sig;                   // 签名 (0x06054b50)
    uint16_t disk_number;           // 当前磁盘号
    uint16_t disk_wcd;              // 中央目录开始磁盘号
    uint16_t disk_entries;          // 当前磁盘中央目录记录数
    uint16_t total_entries;         // 中央目录记录总数
    uint32_t central_directory_size; // 中央目录大小
    uint32_t file_hdr_off;          // 中央目录偏移
    uint16_t comment_len;           // 注释长度
} mmz_EndRecord;
#pragma pack(pop)

// 文件信息缓冲区结构体
struct mmz_FileInfoBuffer {
    struct mmz_FileInfoMeta meta;   // 文件元数据
    char path[FS_MAX_PATH];         // 文件路径
};

// ZIP数据缓冲区结构体
struct mmz_DataBuf {
    s64 fbuf_size;                  // 内部缓冲区大小
    FsDirectoryEntry entry;         // 目录条目
    char path[FS_MAX_PATH];         // 当前路径
    char path_temp[FS_MAX_PATH];    // 临时路径
};

/**
 * 构建ZIP本地文件头
 * @param mz ZIP数据结构指针
 * @param local 本地文件头结构体指针
 * @return 本地文件头的总大小（包括文件名）
 */
static u32 mmz_build_local_header(const struct mmz_Data* mz, struct mmz_LocalHeader* local) {
    memset(local, 0, sizeof(*local));
    local->sig = LOCAL_HEADER_SIG;
    local->flags = 1 << 3; // data descriptor
    local->filename_len = mz->meta.string_len;
    struct tm tm = {0};
    if (localtime_r(&mz->time, &tm)) {
        local->modtime = (tm.tm_sec) | ((tm.tm_min) << 5) | (tm.tm_hour << 11);
        local->moddate = (tm.tm_mday) | ((tm.tm_mon + 1) << 5) | ((tm.tm_year > 80 ? tm.tm_year - 80 : 0) << 9);
    }
    return sizeof(*local) + mz->meta.string_len;
}

/**
 * 构建ZIP中央目录文件头
 * @param mz ZIP数据结构指针
 * @param file 文件头结构体指针
 * @return 文件头的总大小（包括文件名）
 */
static u32 mmz_build_file_header(const struct mmz_Data* mz, struct mmz_FileHeader* file) {
    memset(file, 0, sizeof(*file));
    file->sig = FILE_HEADER_SIG;
    file->version = 3 << 8; // UNIX
    file->flags = 1 << 3; // data descriptor
    file->crc32 = mz->meta.crc32;
    file->compressed_size = mz->meta.size;
    file->uncompressed_size = mz->meta.size;
    file->filename_len = mz->meta.string_len;
    file->local_hdr_off = mz->local_hdr_off;
    struct tm tm = {0};
    if (localtime_r(&mz->time, &tm)) {
        file->modtime = (tm.tm_sec) | ((tm.tm_min) << 5) | (tm.tm_hour << 11);
        file->moddate = (tm.tm_mday) | ((tm.tm_mon + 1) << 5) | ((tm.tm_year > 80 ? tm.tm_year - 80 : 0) << 9);
    }
    return sizeof(*file) + mz->meta.string_len;
}

/**
 * 构建ZIP数据描述符
 * @param mz ZIP数据结构指针
 * @param desc 数据描述符结构体指针
 * @return 数据描述符的大小
 */
static u32 mmz_build_data_descriptor(const struct mmz_Data* mz, struct mmz_DataDescriptor* desc) {
    memset(desc, 0, sizeof(*desc));
    desc->sig = DATA_DESCRIPTOR_SIG;
    desc->crc32 = mz->meta.crc32;
    desc->compressed_size = mz->meta.size;
    desc->uncompressed_size = mz->meta.size;
    return sizeof(*desc);
}

/**
 * 构建ZIP结束记录
 * @param mz ZIP数据结构指针
 * @param rec 结束记录结构体指针
 * @return 结束记录的大小
 */
static u32 mmz_build_end_record(const struct mmz_Data* mz, struct mmz_EndRecord* rec) {
    memset(rec, 0, sizeof(*rec));
    rec->sig = END_RECORD_SIG;
    rec->disk_entries = mz->file_count;
    rec->total_entries = mz->file_count;
    rec->central_directory_size = mz->central_directory_size;
    rec->file_hdr_off = mz->local_hdr_off;
    return sizeof(*rec);
}

/**
 * 向ZIP文件添加单个文件
 * @param mz ZIP数据结构指针
 * @param db 数据缓冲区结构体指针
 * @param path 文件路径
 * @return 操作结果
 */
static Result mmz_add_file(struct mmz_Data* mz, struct mmz_DataBuf* db, const char* path) {
    // skip leading root path, zip paths are relative.
    if (path[0] == '/') {
        path++;
    }

    Result rc;
    struct mmz_FileInfoBuffer buf = {0};
    buf.meta.string_len = strlen(path);
    memcpy(buf.path, path, buf.meta.string_len);
    const size_t buf_size = sizeof(buf.meta) + buf.meta.string_len;

    if (db->fbuf_size - mz->fbuf_off < buf_size) {
        db->fbuf_size += 1024 * 64;
        if (R_FAILED(rc = fsFileSetSize(&mz->fbuf_out, db->fbuf_size))) {
            return rc;
        }
    }

    if (R_FAILED(rc = fsFileWrite(&mz->fbuf_out, mz->fbuf_off, &buf, buf_size, FsWriteOption_None))) {
        return rc;
    }

    mz->file_count++;
    mz->fbuf_off += buf_size;
    return rc;
}

/**
 * 向ZIP文件添加目录（递归处理）
 * @param mz ZIP数据结构指针
 * @param db 数据缓冲区结构体指针
 * @param path 目录路径
 * @return 操作结果
 */
static Result mmz_add_dir(struct mmz_Data* mz, struct mmz_DataBuf* db, const char* path) {
    Result rc;
    FsDir dir;
    snprintf(db->path, sizeof(db->path), path);

    if (R_FAILED(rc = fsFsOpenDirectory(mz->fs, db->path, FsDirOpenMode_ReadDirs|FsDirOpenMode_ReadFiles|FsDirOpenMode_NoFileSize, &dir))) {
        return rc;
    }

    if (!strcmp(db->path, "/")) {
        db->path[0] = '\0';
    }

    s64 total_entries;
    while ((R_SUCCEEDED(rc = fsDirRead(&dir, &total_entries, 1, &db->entry))) && total_entries > 0) {
        snprintf(db->path_temp, sizeof(db->path_temp), "%s/%s", db->path, db->entry.name);
        if (db->entry.type == FsDirEntryType_Dir) {
            if (R_FAILED(rc = mmz_add_dir(mz, db, db->path_temp))) {
                break;
            }
            strrchr(db->path, '/')[0] = '\0';
        } else {
            if (R_FAILED(rc = mmz_add_file(mz, db, db->path_temp))) {
                break;
            }
        }
    }

    fsDirClose(&dir);
    return rc;
}

 void mzz_build_temp_path(char* out, u64 app_id, AccountUid uid, u8 type) {
    snprintf(out, FS_MAX_PATH, "/~ftpsrv_mmzip_temp_%016lX_%016lX%016lX_%d", app_id, uid.uid[0], uid.uid[1], type);
}

 Result mmz_build_zip(struct mmz_Data* mz, FsFileSystem* save_fs, u64 app_id, AccountUid uid, u8 type) {
    memset(mz, 0, sizeof(*mz));
    struct mmz_DataBuf db = {0};
    mz->fs = save_fs;
    db.fbuf_size = 1024 * 64;

    FsFileSystem* sdmc_fs = fsdev_wrapGetDeviceFileSystem("sdmc");
    mzz_build_temp_path(db.path_temp, app_id, uid, type);
    fsFsDeleteFile(sdmc_fs, db.path_temp);

    Result rc;
    if (R_FAILED(rc = fsFsCreateFile(sdmc_fs, db.path_temp, db.fbuf_size, 0))) {
        return rc;
    }

    if (R_FAILED(rc = fsFsOpenFile(sdmc_fs, db.path_temp, FsOpenMode_Read|FsOpenMode_Write|FsOpenMode_Append, &mz->fbuf_out))) {
        goto end;
    }

    if (R_FAILED(rc = mmz_add_dir(mz, &db, "/"))) {
        goto end;
    }

    if (!mz->file_count) {
        rc = 0x339602;
        goto end;
    }

end:
    if (R_FAILED(rc)) {
        fsFileClose(&mz->fbuf_out);
        mzz_build_temp_path(db.path_temp, app_id, uid, type);
        fsFsDeleteFile(sdmc_fs, db.path_temp);
    } else {
        // 确保元数据写入磁盘
        fsFileFlush(&mz->fbuf_out);
        mz->fbuf_off = 0;
        mz->time = time(NULL);
    }

    return rc;
}

/**
 * @brief 从ZIP数据缓冲区读取文件信息
 * @param mz ZIP数据结构指针
 * @param buf 文件信息缓冲区指针
 * @return Result 操作结果
 */
static Result mmz_read_buffer_info(struct mmz_Data* mz, struct mmz_FileInfoBuffer* buf) {
    Result rc;
    u64 bytes_read;
    if (R_FAILED(rc = fsFileRead(&mz->fbuf_out, mz->fbuf_off, &buf->meta, sizeof(buf->meta), 0, &bytes_read))) {
        return rc;
    }

    if (R_FAILED(rc = fsFileRead(&mz->fbuf_out, mz->fbuf_off + sizeof(buf->meta), buf->path, buf->meta.string_len, 0, &bytes_read))) {
        return rc;
    }

    buf->path[buf->meta.string_len] = 0;
    return rc;
}

 int mmz_read(struct mmz_Data* mz, void* buf, size_t size) {
    Result rc;
    if (mz->pending) {
        mz->off = 0;
        mz->pending = false;

        switch (mz->state) {
            case mmz_State_Local: {
                struct mmz_FileInfoBuffer info_buf;
                if (R_FAILED(rc = mmz_read_buffer_info(mz, &info_buf))) {
                    return vfs_fs_set_errno(rc);
                }

                char file_path[FS_MAX_PATH];
                snprintf(file_path, sizeof(file_path), "/%s", info_buf.path);
                if (R_FAILED(rc = fsFsOpenFile(mz->fs, file_path, FsOpenMode_Read, &mz->fin))) {
                    return vfs_fs_set_errno(rc);
                }

                s64 size;
                if (R_FAILED(rc = fsFileGetSize(&mz->fin, &size))) {
                    return vfs_fs_set_errno(rc);
                }

                mz->meta.size = size;
                mz->new_crc32 = 0;
                mz->state = mmz_State_Data;
            }   break;

            case mmz_State_Data: {
                fsFileClose(&mz->fin);

                // store crc32 and size
                mz->meta.crc32 = mz->new_crc32;
                if (R_FAILED(rc = fsFileWrite(&mz->fbuf_out, mz->fbuf_off, &mz->meta, sizeof(mz->meta), 0))) {
                    return vfs_fs_set_errno(rc);
                }

                mz->state = mmz_State_Descriptor;
            }   break;

            case mmz_State_Descriptor: {
                mz->index++;
                if (mz->index == mz->file_count) {
                    mz->fbuf_off = 0;
                    mz->index = 0;
                    mz->state = mmz_State_File;
                } else {
                    mz->fbuf_off += sizeof(mz->meta) + mz->meta.string_len;
                    mz->state = mmz_State_Local;
                }
            }   break;

            case mmz_State_File:
                mz->index++;
                mz->central_directory_size += sizeof(mmz_FileHeader) + mz->meta.string_len;
                mz->local_hdr_off += sizeof(mmz_LocalHeader) + mz->meta.string_len + mz->meta.size + sizeof(mmz_DataDescriptor);
                if (mz->index == mz->file_count) {
                    mz->state = mmz_State_End;
                } else {
                    mz->fbuf_off += sizeof(mz->meta) + mz->meta.string_len;
                }
                break;

            case mmz_State_End:
                return 0;
        }
    }

    u32 total_size = 0;

    switch (mz->state) {
        case mmz_State_Local: {
            struct mmz_FileInfoBuffer info_buf;
            if (R_FAILED(rc = mmz_read_buffer_info(mz, &info_buf))) {
                return vfs_fs_set_errno(rc);
            }

            mz->meta = info_buf.meta;
            struct mmz_LocalHeader local_hdr;
            total_size = mmz_build_local_header(mz, &local_hdr);

            if (mz->off < sizeof(local_hdr)) {
                size = min(size, sizeof(local_hdr) - mz->off);
                memcpy(buf, (u8*)&local_hdr + mz->off, size);
            } else {
                size = min(size, local_hdr.filename_len - (mz->off - sizeof(local_hdr)));
                memcpy(buf, info_buf.path + mz->off - sizeof(local_hdr), size);
            }
        }   break;

        case mmz_State_Data: {
            mz->meta.crc32 = mz->new_crc32;
            u64 bytes_read;
            total_size = mz->meta.size;
            size = min(size, total_size - mz->off);
            if (R_FAILED(rc = fsFileRead(&mz->fin, mz->off, buf, size, 0, &bytes_read))) {
                return vfs_fs_set_errno(rc);
            }

            mz->new_crc32 = crc32CalculateWithSeed(mz->meta.crc32, buf, size);
        }   break;

        case mmz_State_Descriptor: {
            mmz_DataDescriptor data_desc = {0};
            total_size = mmz_build_data_descriptor(mz, &data_desc);
            size = min(size, total_size - mz->off);
            memcpy(buf, (const u8*)&data_desc + mz->off, size);
        }   break;

        case mmz_State_File: {
            struct mmz_FileInfoBuffer info_buf;
            if (R_FAILED(rc = mmz_read_buffer_info(mz, &info_buf))) {
                return vfs_fs_set_errno(rc);
            }

            mz->meta = info_buf.meta;
            struct mmz_FileHeader file_hdr;
            total_size = mmz_build_file_header(mz, &file_hdr);

            if (mz->off < sizeof(file_hdr)) {
                size = min(size, sizeof(file_hdr) - mz->off);
                memcpy(buf, (const u8*)&file_hdr + mz->off, size);
            } else {
                size = min(size, file_hdr.filename_len - (mz->off - sizeof(file_hdr)));
                memcpy(buf, info_buf.path + mz->off - sizeof(file_hdr), size);
            }
        }   break;

        case mmz_State_End: {
            struct mmz_EndRecord end_rec;
            total_size = mmz_build_end_record(mz, &end_rec);
            size = min(size, total_size - mz->off);
            memcpy(buf, (const u8*)&end_rec + mz->off, size);
        }   break;
    }

    mz->off += size;
    mz->zip_off += size;

    if (mz->off == total_size) {
        mz->pending = true;
    }

    return size;
}

/**
 * @brief 存档账户信息结构体
 */
struct SaveAcc {
    AccountUid uid;     // 用户账户ID
    char name[0x20];    // 用户名称
};

// HOS系统只允许存档文件系统挂载一次...
// 为了解决这个问题，我们保持一个16个存档的缓存（足够了）
/**
 * @brief 存档缓存条目结构体
 */
struct SaveCacheEntry {
    FsFileSystem fs;        // 文件系统对象
    u64 app_id;            // 应用程序ID
    AccountUid uid;        // 用户账户ID
    FsSaveDataType type;   // 存档数据类型
    u32 ref_count;         // 引用计数
};

static struct SaveCacheEntry g_save_cache[16];  // 存档缓存数组
static struct SaveAcc g_acc_profile[12];        // 账户配置文件数组
static s32 g_acc_count;                         // 账户数量
static bool g_writable;                         // 是否可写标志

// 所有对FAT文件系统无效的字符列表，
// 这些字符会被转换为"_"
static const char INVALID_CHAR_TABLE[] = {
    '<',
    '>',
    ':',
    '"',
    '/',
    '\\',
    '|',
    '?',
    '*',
    '.',
    ',',
    ';',
    '+',
    '=',
    '&',
    '%', // probably invalid
};

/**
 * @brief 使ZIP文件中的字符串有效化
 * 
 * 此函数用于清理字符串，使其符合ZIP文件格式和FAT文件系统的要求。
 * 根据skip_ascii_convert配置决定是否处理非ASCII字符。
 * 主要处理以下几类字符：
 * 1. 控制字符（< 0x20）
 * 2. 非ASCII字符（>= 0x80，根据配置处理）
 * 3. FAT文件系统中的无效字符（如 < > : " / \ | ? * 等）
 * 
 * @param str 需要处理的字符串（会被直接修改）
 * 
 * 处理规则：
 * - 始终替换控制字符（0x00-0x1F）为下划线
 * - 根据skip_ascii_convert配置处理非ASCII字符（>= 0x80）：
 *   - 如果skip_ascii_convert为false：
 *     - 特殊处理：'é' (0xC3 0xA9) 和 右单引号 (0xE2 0x80 0x99)
 *     - 其他非ASCII字符替换为下划线
 *   - 如果skip_ascii_convert为true：保留非ASCII字符（如中文）
 * - 始终处理ASCII无效字符（来自INVALID_CHAR_TABLE）：
 *   - 如果是字符串末尾，截断字符串
 *   - 如果下一个字符不是空格，替换为下划线
 *   - 如果下一个字符是空格，删除该字符
 */
static void make_zip_string_valid(char* str) {
    // 获取是否跳过ASCII转换的配置
    bool skip_ascii_convert = vfs_get_skip_ascii_convert();
    
    for (int i = 0; str[i]; i++) {
        const unsigned char c = str[i];        // 当前字符
        const unsigned char c2 = str[i + 1];   // 下一个字符
        
        // 处理控制字符和非ASCII字符（< 0x20 或 >= 0x80）
        if (c < 0x20 || (!skip_ascii_convert && c >= 0x80)) {
            // 如果不跳过ASCII转换，处理非ASCII字符
            if (!skip_ascii_convert && c >= 0x80) {
                // 特殊处理UTF-8编码的 'é' 字符 (0xC3 0xA9)
                if (c == 195 && c2 == 169) {
                    str[i + 1] = 'e';  // 将第二个字节改为 'e'
                    memcpy(str + i, str + i + 1, strlen(str) - i);  // 删除第一个字节
                } 
                // 特殊处理UTF-8编码的右单引号 (0xE2 0x80 0x99)
                else if (c == 226 && c2 == 128 && (unsigned char)str[i + 2] == 153) {
                    str[i + 2] = '\'';  // 将第三个字节改为单引号
                    memcpy(str + i, str + i + 2, strlen(str) - i);  // 删除前两个字节
                } 
                // 其他非ASCII字符直接替换为下划线
                else {
                    str[i] = '_';
                }
            }
            // 处理控制字符（始终处理，不受skip_ascii_convert影响）
            else if (c < 0x20) {
                str[i] = '_';
            }
        } 
        // 处理ASCII字符中的无效字符
        else {
            // 检查是否为FAT文件系统无效字符
            for (int j = 0; j < ARRAY_SIZE(INVALID_CHAR_TABLE); j++) {
                if (c == INVALID_CHAR_TABLE[j]) {
                    // 根据下一个字符决定处理方式
                    if (str[i + 1] == '\0') {
                        // 如果是字符串末尾，直接截断
                        str[i] = '\0';
                    } else if (str[i + 1] != ' ') {
                        // 如果下一个字符不是空格，替换为下划线
                        str[i] = '_';
                    } else {
                        // 如果下一个字符是空格，删除当前字符
                        memcpy(str + i, str + i + 1, strlen(str) - i);
                    }
                    break;
                }
            }
        }
    }
}

/**
 * @brief 挂载存档文件系统
 * @param d 存档路径数据指针
 * @return FsFileSystem* 成功时返回文件系统指针，失败时返回NULL
 */
static FsFileSystem* mount_save_fs(const struct SavePathData* d) {
    for (int i = 0; i < ARRAY_SIZE(g_save_cache); i++) {
        struct SaveCacheEntry* entry = &g_save_cache[i];
        if (entry->ref_count && entry->app_id == d->app_id && entry->type == d->data_type && !memcmp(&entry->uid, &d->uid, sizeof(d->uid))) {
            entry->ref_count++;
            return &entry->fs;
        }
    }

    // save is not currently mounted, find the next free slot
    for (int i = 0; i < ARRAY_SIZE(g_save_cache); i++) {
        struct SaveCacheEntry* entry = &g_save_cache[i];
        if (!entry->ref_count) {
            FsSaveDataAttribute attr = {0};
            attr.save_data_type = d->data_type;
            Result rc;

            if (d->data_type == FsSaveDataType_System) {
                attr.system_save_data_id = d->app_id;
                rc = fsOpenSaveDataFileSystemBySystemSaveDataId(&entry->fs, d->space_id, &attr);
            } else {
                attr.application_id = d->app_id;
                attr.uid = d->uid;
                if (g_writable) {
                    rc = fsOpenSaveDataFileSystem(&entry->fs, d->space_id, &attr);
                } else {
                    rc = fsOpenReadOnlySaveDataFileSystem(&entry->fs, d->space_id, &attr);
                }
            }

            if (R_FAILED(rc)) {
                vfs_fs_set_errno(rc);
                log_file_fwrite("failed: fsOpenReadOnlySaveDataFileSystem(%016lX) 0x%X\n", d->app_id, rc);
                return NULL;
            }

            entry->uid = d->uid;
            entry->app_id = d->app_id;
            entry->type = d->data_type;
            entry->ref_count++;
            return &entry->fs;
        }
    }

    return NULL;
}

/**
 * @brief 卸载存档文件系统
 * @param d 存档路径数据指针
 */
static void unmount_save_fs(const struct SavePathData* d) {
    for (int i = 0; i < ARRAY_SIZE(g_save_cache); i++) {
        struct SaveCacheEntry* entry = &g_save_cache[i];
        if (entry->ref_count && entry->app_id == d->app_id && entry->type == d->data_type && !memcmp(&entry->uid, &d->uid, sizeof(d->uid))) {
            entry->ref_count--;
            if (!entry->ref_count) {
                if (g_writable) {
                    fsFsCommit(&entry->fs);
                }
                fsFsClose(&entry->fs);
            }
        }
    }
}

/**
 * 解析存档路径并返回对应的存档数据信息
 * @param path 存档路径字符串
 * @return 解析后的存档路径数据结构
 */
static struct SavePathData get_type(const char* path) {
    struct SavePathData data = {0};  // 初始化存档路径数据结构
    
    // 进行反向映射：将用户友好的设备名称转换为系统内部名称
    const char* mapped_path = path;
    char mapped_buffer[FS_MAX_PATH];
    
    // 查找路径中的冒号分隔符，直接转换为 save: 系统路径
    const char* colon_pos = strchr(path, ':');
    if (colon_pos != NULL) {
        // 获取剩余路径（跳过冒号）
        const char* remaining_path = colon_pos + 1;
        
        // 直接构建 save: 系统路径
        snprintf(mapped_buffer, sizeof(mapped_buffer), "save:%s", remaining_path);
        mapped_path = mapped_buffer;
    }
    
    // 检查是否为存档根目录
    if (!strcmp(mapped_path, "save:")) {
        data.type = SaveDirType_Root;  // 设置为根目录类型
    } else {
        // 查找路径中的分隔符 '[' 用于解析用户ID和应用ID
        const char* dilem = strchr(mapped_path, '[');
        data.space_id = FsSaveDataSpaceId_User;  // 默认设置为用户存储空间
        
        // 解析不同类型的存档路径
        if (!strncmp(mapped_path, "save:/bcat", strlen("save:/bcat"))) {
            // BCAT存档类型 - 用于游戏数据分发
            data.data_type = FsSaveDataType_Bcat;
            data.space_id = FsSaveDataSpaceId_User;
            data.type = SaveDirType_User1;
        } else if (!strncmp(mapped_path, "save:/cache", strlen("save:/cache"))) {
            // 缓存存档类型 - 存储在SD卡上的缓存数据
            data.data_type = FsSaveDataType_Cache;
            data.space_id = FsSaveDataSpaceId_SdUser;  // SD卡用户空间
            data.type = SaveDirType_User1;
        } else if (!strncmp(mapped_path, "save:/device", strlen("save:/device"))) {
            // 设备存档类型 - 设备特定的存档数据
            data.data_type = FsSaveDataType_Device;
            data.space_id = FsSaveDataSpaceId_User;
            data.type = SaveDirType_User1;
        } else if (!strncmp(mapped_path, "save:/system", strlen("save:/system"))) {
            // 系统存档类型 - 系统级别的存档数据
            data.data_type = FsSaveDataType_System;
            data.space_id = FsSaveDataSpaceId_System;  // 系统存储空间
            data.type = SaveDirType_User1;
        } else if (dilem && strlen(dilem) >= 33) {
            // 解析用户账户存档路径，格式: save:/[用户ID32位十六进制][应用ID16位十六进制]
            dilem++;  // 跳过 '[' 字符
            char uid_buf[2][17];  // 用于存储用户ID的两个部分（每部分16字符）
            
            // 分割32位用户ID为两个16位部分
            snprintf(uid_buf[0], sizeof(uid_buf[0]), "%s", dilem);
            snprintf(uid_buf[1], sizeof(uid_buf[1]), "%s", dilem + 0x10);

            // 将十六进制字符串转换为64位整数
            data.uid.uid[0] = strtoull(uid_buf[0], NULL, 0x10);
            data.uid.uid[1] = strtoull(uid_buf[1], NULL, 0x10);

            // 设置为账户存档类型
            data.data_type = FsSaveDataType_Account;
            data.space_id = FsSaveDataSpaceId_User;
            data.type = SaveDirType_User1;
            
            // 查找下一个 '[' 字符，用于解析应用ID
            dilem = strchr(dilem, '[');
        }

        // 进一步解析存档子目录类型
        if (data.type == SaveDirType_User1) {
            if (strstr(mapped_path, "/zips")) {
                // ZIP压缩存档目录
                data.type = SaveDirType_Zip;
            } else if (strstr(mapped_path, "/files")) {
                // 文件存档目录
                data.type = SaveDirType_File;
            }

            // 如果是文件或ZIP类型，进一步解析应用ID
            if (data.type == SaveDirType_File || data.type == SaveDirType_Zip) {
                // 需要正确处理这部分逻辑，目前的实现已经足够使用
                if (dilem && strlen(dilem) >= 17) {
                    dilem++;  // 跳过 '[' 字符
                    // 解析16位十六进制应用ID
                    data.app_id = strtoull(dilem, NULL, 0x10);
                    // 根据原类型设置对应的应用存档类型
                    data.type = data.type == SaveDirType_File ? SaveDirType_FileApp : SaveDirType_ZipApp;
                    dilem += 17;  // 跳过应用ID和结束的 ']'
                    // 计算原始路径中的偏移量
                    // 由于我们总是进行映射，需要找到原始路径中对应的位置
                    const char* colon_in_original = strchr(path, ':');
                    if (colon_in_original) {
                        // 计算映射路径中 ']' 后的偏移量
                        size_t mapped_offset = dilem - mapped_path;
                        // 减去 "save:" 的长度，加上原始路径冒号后的位置
                        data.path_off = (colon_in_original + 1 - path) + (mapped_offset - 5);  // 5 = strlen("save:")
                    } else {
                        data.path_off = dilem - mapped_path;
                    }
                }
            }
        }
    }

    return data;  // 返回解析后的存档路径数据
}

static void build_native_path(char out[FS_MAX_PATH], const char* path, const struct SavePathData* data) {
    const char* relative_path = path + data->path_off;
    // 如果相对路径为空或者只是一个结束符，返回根目录
    if (!relative_path || !*relative_path || strlen(relative_path) == 0) {
        strcpy(out, "/");
    } else {
        snprintf(out, FS_MAX_PATH, "%s", relative_path);
    }
}

static void rescan_users(void) {
    memset(g_acc_profile, 0, sizeof(g_acc_profile));
    g_acc_count = 0;

    AccountUid uids[8];
    s32 count;
    Result rc;
    if (R_FAILED(rc = accountListAllUsers(uids, 8, &count))) {
        log_file_fwrite("failed: accountListAllUsers() 0x%X\n", rc);
    } else {
        for (int i = 0; i < count; i++) {
            AccountProfile profile;
            if (R_FAILED(rc = accountGetProfile(&profile, uids[i]))) {
                log_file_fwrite("failed: accountGetProfile() 0x%X\n", rc);
            } else {
                AccountProfileBase base;
                if (R_FAILED(rc = accountProfileGet(&profile, NULL, &base))) {
                    log_file_fwrite("failed: accountProfileGet() 0x%X\n", rc);
                } else {
                    strcpy(g_acc_profile[g_acc_count].name, base.nickname);
                    g_acc_profile[g_acc_count].uid = uids[i];
                    g_acc_count++;
                }
                accountProfileClose(&profile);
            }
        }
    }

    strcpy(g_acc_profile[g_acc_count++].name, "bcat");
    strcpy(g_acc_profile[g_acc_count++].name, "cache");
    strcpy(g_acc_profile[g_acc_count++].name, "device");
    strcpy(g_acc_profile[g_acc_count++].name, "system");
}

static int vfs_save_open(void* user, const char* path, enum FtpVfsOpenMode mode) {
    struct VfsSaveFile* f = user;
    f->data = get_type(path);
    if (mode != FtpVfsOpenMode_READ && (!g_writable || f->data.type != SaveDirType_ZipApp)) {
        errno = EROFS;
        return -1;
    }

    if (f->data.type != SaveDirType_FileApp && f->data.type != SaveDirType_ZipApp) {
        return -1;
    }

    FsFileSystem* fs = mount_save_fs(&f->data);
    if (!fs) {
        return -1;
    }

    char nxpath[FS_MAX_PATH];
    build_native_path(nxpath, path, &f->data);

    if (f->data.type == SaveDirType_FileApp) {
        if (vfs_fs_internal_open(fs, &f->fs_file, nxpath, mode)) {
            unmount_save_fs(&f->data);
            return -1;
        }

        f->fs = *fs;
        f->is_valid = 1;
        return 0;
    } else {
        Result rc;
        if (R_FAILED(rc = mmz_build_zip(&f->mz, fs, f->data.app_id, f->data.uid, f->data.space_id))) {
            unmount_save_fs(&f->data);
            return vfs_fs_set_errno(rc);
        }

        f->fs = *fs;
        f->is_valid = 1;
        return 0;
    }
}

static int vfs_save_read(void* user, void* buf, size_t size) {
    struct VfsSaveFile* f = user;
    if (f->data.type == SaveDirType_ZipApp) {
        return mmz_read(&f->mz, buf, size);
    } else {
        return vfs_fs_internal_read(&f->fs_file, buf, size);
    }
}

static int vfs_save_write(void* user, const void* buf, size_t size) {
    struct VfsSaveFile* f = user;
    if (!g_writable || f->data.type == SaveDirType_ZipApp) {
        errno = EROFS;
        return -1;
    }

    return vfs_fs_internal_write(&f->fs_file, buf, size);
}

static int vfs_save_seek(void* user, const void* buf, size_t size, size_t off) {
    struct VfsSaveFile* f = user;

    if (f->data.type == SaveDirType_ZipApp) {
        if (off > f->mz.zip_off) {
            errno = ESPIPE;
            return -1;
        }

        f->mz.new_crc32 = crc32CalculateWithSeed(f->mz.meta.crc32, buf, size);
        f->mz.pending = false;
        f->mz.off -= f->mz.zip_off - off;
        f->mz.zip_off = off;
        return 0;
    } else {
        return vfs_fs_internal_seek(&f->fs_file, off);
    }
}

static int vfs_save_isfile_open(void* user) {
    struct VfsSaveFile* f = user;
    return f->is_valid;
}

static int vfs_save_close(void* user) {
    struct VfsSaveFile* f = user;
    if (!vfs_save_isfile_open(f)) {
        return -1;
    }

    if (f->data.type == SaveDirType_FileApp) {
        vfs_fs_internal_close(&f->fs_file);
    } else {
        fsFileClose(&f->mz.fbuf_out);
        fsFileClose(&f->mz.fin);
        char nxpath[FS_MAX_PATH];
        mzz_build_temp_path(nxpath, f->data.app_id, f->data.uid, f->data.space_id);
        fsFsDeleteFile(fsdev_wrapGetDeviceFileSystem("sdmc"), nxpath);
    }

    unmount_save_fs(&f->data);
    f->is_valid = 0;
    return 0;
}

/**
 * @brief 打开存档虚拟文件系统目录
 * 
 * 此函数用于打开存档VFS中的目录，支持多种不同类型的存档目录结构。
 * 根据路径类型执行不同的初始化操作，为后续的目录读取操作做准备。
 * 
 * @param user VfsSaveDir结构体指针，用于存储目录状态信息
 * @param path 要打开的目录路径（如 "save:", "save:/files", "save:/zips" 等）
 * @return int 成功返回0，失败返回-1
 * 
 * 支持的目录类型：
 * - SaveDirType_Root: 根目录，显示所有用户账户
 * - SaveDirType_User1: 用户级目录，显示files和zips子目录
 * - SaveDirType_File: 文件模式，显示存档文件列表
 * - SaveDirType_Zip: ZIP模式，显示存档ZIP文件列表
 * - SaveDirType_FileApp: 应用存档文件系统，直接访问存档内容
 */
static int vfs_save_opendir(void* user, const char* path) {
    struct VfsSaveDir* f = user;
    f->data = get_type(path);  // 解析路径类型和相关数据

    switch (f->data.type) {
        default: return -1;  // 不支持的目录类型

        case SaveDirType_Root:
            // 根目录：重新扫描用户账户列表
            rescan_users();
            break;

        case SaveDirType_User1:
            // 用户级目录：无需特殊初始化，直接显示固定的子目录
            break;

        case SaveDirType_File:
        case SaveDirType_Zip: {
            // 文件/ZIP模式：设置存档数据过滤器，用于读取存档信息
            FsSaveDataFilter filter = {0};
            filter.filter_by_save_data_type = true;
            filter.attr.save_data_type = f->data.data_type;

            // 如果是账户类型的存档，添加用户ID过滤
            if (f->data.data_type == FsSaveDataType_Account) {
                filter.filter_by_user_id = true;
                filter.attr.uid = f->data.uid;
            }

            Result rc;
            // 打开存档信息读取器，用于枚举符合条件的存档
            if (R_FAILED(rc = fsOpenSaveDataInfoReaderWithFilter(&f->r, f->data.space_id, &filter))) {
                log_file_fwrite("failed: fsOpenSaveDataInfoReaderWithFilter() 0x%X\n", rc);
                return -1;
            }
        }   break;

        case SaveDirType_FileApp: {
            // 应用存档文件系统：挂载特定的存档文件系统
            FsFileSystem* fs = mount_save_fs(&f->data);
            if (!fs) {
                return -1;
            }
            f->fs = *fs;

            // 构建原生路径并打开存档内的目录
            char nxpath[FS_MAX_PATH] = {"/"};
            build_native_path(nxpath, path, &f->data);
            if (vfs_fs_internal_opendir(&f->fs, &f->fs_dir, nxpath)) {
                unmount_save_fs(&f->data);
                return -1;
            }
        }   break;
    }

    f->index = 0;      // 重置读取索引
    f->is_valid = 1;   // 标记目录为有效状态
    return 0;
}

/**
 * @brief 读取存档虚拟文件系统目录条目
 * 
 * 此函数用于从已打开的存档VFS目录中读取下一个目录条目。
 * 根据不同的目录类型，返回相应的条目信息（用户账户、存档文件、应用程序等）。
 * 
 * @param user VfsSaveDir结构体指针，包含目录状态信息
 * @param user_entry VfsSaveDirEntry结构体指针，用于存储读取到的目录条目信息
 * @return const char* 成功时返回条目名称，到达目录末尾或失败时返回NULL
 * 
 * 不同目录类型的处理：
 * - SaveDirType_Root: 返回用户账户信息，格式为"昵称 [UID]"
 * - SaveDirType_User1: 返回固定的子目录名称（"files", "zips"）
 * - SaveDirType_File/Zip: 返回存档应用程序信息，显示应用名称或ID
 * - SaveDirType_FileApp: 返回存档文件系统内的实际文件/目录条目
 */
static const char* vfs_save_readdir(void* user, void* user_entry) {
    struct VfsSaveDir* f = user;
    struct VfsSaveDirEntry* entry = user_entry;

    Result rc;
    switch (f->data.type) {
        default: return NULL;  // 不支持的目录类型

        case SaveDirType_Root: {
            // 根目录：返回用户账户信息
            if (f->index >= g_acc_count) {
                return NULL;  // 已读取完所有账户
            }
            const struct SaveAcc* p = &g_acc_profile[f->index];
            // 根据UID有效性决定显示格式
            if (!accountUidIsValid(&p->uid)) {
                // 系统账户（如bcat、cache等），只显示名称
                snprintf(entry->name, sizeof(entry->name), "%s", p->name);
            } else {
                // 用户账户，显示"昵称 [UID]"
                snprintf(entry->name, sizeof(entry->name), "%s [%016lX%016lX]", p->name, p->uid.uid[0], p->uid.uid[1]);
            }
            f->index++;
            return entry->name;
        }

        case SaveDirType_User1: {
            // 用户级目录：返回固定的子目录名称
            static const char* e[] = { "files","zips" };
            if (f->index >= sizeof(e)/sizeof(e[0])) {
                return NULL;  // 只有两个固定子目录
            }
            return e[f->index++];
        }

        case SaveDirType_File:
        case SaveDirType_Zip: {
            // 文件/ZIP模式：读取存档信息并显示应用程序名称
            s64 total;
            // 从存档信息读取器中读取下一个存档条目
            if (R_FAILED(rc = fsSaveDataInfoReaderRead(&f->r, &entry->info, 1, &total))) {
                log_file_fwrite("failed: fsSaveDataInfoReaderRead() 0x%X\n", rc);
                return NULL;
            }

            if (total <= 0) {
                log_file_fwrite("fsSaveDataInfoReaderRead() no more entries %zd\n", total);
                return NULL;  // 没有更多存档条目
            }

            // 尝试获取应用程序名称（如果游戏已卸载可能会失败）
            NcmContentId id;
            struct AppName name;
            const char* ext = f->data.type == SaveDirType_File ? "" : ".zip";
            
            // 处理系统存档
            if (entry->info.save_data_type == FsSaveDataType_System || entry->info.save_data_type == FsSaveDataType_SystemBcat) {
                snprintf(entry->name, sizeof(entry->name), "[%016lX]%s", entry->info.system_save_data_id, ext);
            } 
            // 尝试获取应用程序名称
            else if (R_FAILED(rc = get_app_name(entry->info.application_id, &id, &name))) {
                // 无法获取应用名称，使用应用程序ID
                snprintf(entry->name, sizeof(entry->name), "[%016lX]%s", entry->info.application_id, ext);
            } else {
                // 成功获取应用名称，清理文件名中的无效字符
                utilsReplaceIllegalCharacters(name.str, true);
                if (f->data.type == SaveDirType_Zip) {
                    // ZIP模式需要额外的字符串有效化处理
                    make_zip_string_valid(name.str);
                }
                snprintf(entry->name, sizeof(entry->name), "%s [%016lX]%s", name.str, entry->info.application_id, ext);
            }

            log_file_fwrite("read entry %s data: %s space: %s %u index: %u rank %u\n", name.str, entry->info.save_data_index, entry->info.save_data_rank);
            return entry->name;
        }

        case SaveDirType_FileApp: {
            // 应用存档文件系统：直接读取存档内的文件系统条目
            return vfs_fs_internal_readdir(&f->fs_dir, &entry->fs_buf);
        }
    }
}

static int vfs_save_dirlstat(void* user, const void* user_entry, const char* path, struct stat* st) {
    struct VfsSaveDir* f = user;
    const struct VfsSaveDirEntry* entry = user_entry;
    memset(st, 0, sizeof(*st));

    switch (f->data.type) {
        default: return -1;

        case SaveDirType_Root:
        case SaveDirType_User1:
        case SaveDirType_File:
            st->st_nlink = 1;
            st->st_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH;
            return 0;

        case SaveDirType_Zip:
            // random size for the client, hopefully they don't take it seriously ;)
            st->st_nlink = 1;
            st->st_size = entry->info.size;
            st->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
            return 0;

        case SaveDirType_FileApp: {
            char nxpath[FS_MAX_PATH];
            build_native_path(nxpath, path, &f->data);
            return vfs_fs_internal_dirlstat(&f->fs, &f->fs_dir, &entry->fs_buf, nxpath, st);
        }
    }
}

static int vfs_save_isdir_open(void* user) {
    struct VfsSaveDir* f = user;
    return f->is_valid;
}

static int vfs_save_closedir(void* user) {
    struct VfsSaveDir* f = user;
    if (!vfs_save_isdir_open(f)) {
        return -1;
    }

    switch (f->data.type) {
        default: break;

        case SaveDirType_File:
        case SaveDirType_Zip:
            fsSaveDataInfoReaderClose(&f->r);
            break;

        case SaveDirType_FileApp:
            vfs_fs_internal_closedir(&f->fs_dir);
            unmount_save_fs(&f->data);
            break;
    }

    memset(f, 0, sizeof(*f));
    return 0;
}

static int vfs_save_stat(const char* path, struct stat* st) {
    const struct SavePathData data = get_type(path);
    memset(st, 0, sizeof(*st));
    st->st_nlink = 1;


    switch (data.type) {
        default: return -1;

        case SaveDirType_Root:
        case SaveDirType_User1:
        case SaveDirType_File:
        case SaveDirType_Zip:
            st->st_nlink = 1;
            st->st_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH;
            return 0;

        case SaveDirType_FileApp: {
            FsFileSystem* fs = mount_save_fs(&data);
            if (!fs) {
                return -1;
            }

            char nxpath[FS_MAX_PATH];
            build_native_path(nxpath, path, &data);
            int rc = vfs_fs_internal_stat(fs, nxpath, st);
            unmount_save_fs(&data);
            return rc;
        }

        case SaveDirType_ZipApp: {
            // random size for the client, hopefully they don't take it seriously ;)
            st->st_size = 1024*1024*64;
            st->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
            return 0;
        }
    }

    return 0;
}

static int vfs_save_mkdir(const char* path) {
    const struct SavePathData data = get_type(path);
    if (!g_writable || data.type != SaveDirType_FileApp) {
        errno = EROFS;
        return -1;
    }

    FsFileSystem* fs = mount_save_fs(&data);
    if (!fs) {
        return -1;
    }

    char nxpath[FS_MAX_PATH];
    build_native_path(nxpath, path, &data);
    int rc = vfs_fs_internal_mkdir(fs, nxpath);
    unmount_save_fs(&data);
    return rc;
}

static int vfs_save_unlink(const char* path) {
    const struct SavePathData data = get_type(path);
    if (!g_writable || data.type != SaveDirType_FileApp) {
        errno = EROFS;
        return -1;
    }

    FsFileSystem* fs = mount_save_fs(&data);
    if (!fs) {
        return -1;
    }

    char nxpath[FS_MAX_PATH];
    build_native_path(nxpath, path, &data);
    int rc = vfs_fs_internal_unlink(fs, nxpath);
    unmount_save_fs(&data);
    return rc;
}

static int vfs_save_rmdir(const char* path) {
    const struct SavePathData data = get_type(path);
    if (!g_writable || data.type != SaveDirType_FileApp) {
        errno = EROFS;
        return -1;
    }

    FsFileSystem* fs = mount_save_fs(&data);
    if (!fs) {
        return -1;
    }

    char nxpath[FS_MAX_PATH];
    build_native_path(nxpath, path, &data);
    int rc = vfs_fs_internal_rmdir(fs, nxpath);
    unmount_save_fs(&data);
    return rc;
}

static int vfs_save_rename(const char* src, const char* dst) {
    const struct SavePathData data_src = get_type(src);
    const struct SavePathData data_dst = get_type(dst);
    if (!g_writable || data_src.type != SaveDirType_FileApp || data_dst.type != SaveDirType_FileApp) {
        errno = EROFS;
        return -1;
    }


    if (data_src.app_id != data_dst.app_id || memcmp(&data_src.uid, &data_dst.uid, sizeof(data_src.uid))) {
        return -1;
    }

    FsFileSystem* fs = mount_save_fs(&data_src);
    if (!fs) {
        return -1;
    }

    char nxpath_src[FS_MAX_PATH];
    char nxpath_dst[FS_MAX_PATH];
    build_native_path(nxpath_src, src, &data_src);
    build_native_path(nxpath_dst, dst, &data_dst);
    int rc = vfs_fs_internal_rename(fs, nxpath_src, nxpath_dst);
    unmount_save_fs(&data_src);
    return rc;
}

void vfs_save_init(bool save_writable) {
    g_writable = save_writable;
    rescan_users();
    AccountUid uids[8];
    s32 count;
    Result rc;
    if (R_FAILED(rc = accountListAllUsers(uids, 8, &count))) {
        log_file_fwrite("failed: accountListAllUsers() 0x%X\n", rc);
    } else {
        for (int i = 0; i < count; i++) {
            AccountProfile profile;
            if (R_FAILED(rc = accountGetProfile(&profile, uids[i]))) {
                log_file_fwrite("failed: accountGetProfile() 0x%X\n", rc);
            } else {
                AccountProfileBase base;
                if (R_FAILED(rc = accountProfileGet(&profile, NULL, &base))) {
                    log_file_fwrite("failed: accountProfileGet() 0x%X\n", rc);
                } else {
                    strcpy(g_acc_profile[g_acc_count].name, base.nickname);
                    g_acc_profile[g_acc_count].uid = base.uid;
                    g_acc_count++;
                }
                accountProfileClose(&profile);
            }
        }
    }

    strcpy(g_acc_profile[g_acc_count++].name, "bcat");
    strcpy(g_acc_profile[g_acc_count++].name, "cache");
    strcpy(g_acc_profile[g_acc_count++].name, "device");
    strcpy(g_acc_profile[g_acc_count++].name, "system");
}

void vfs_save_exit(void) {
    for (int i = 0; i < ARRAY_SIZE(g_save_cache); i++) {
        struct SaveCacheEntry* entry = &g_save_cache[i];
        if (entry->ref_count) {
            if (g_writable) {
                fsFsCommit(&entry->fs);
            }
            fsFsClose(&entry->fs);
        }
    }

    memset(g_acc_profile, 0, sizeof(g_acc_profile));
    g_acc_count = 0;
}

const FtpVfs g_vfs_save = {
    .open = vfs_save_open,
    .read = vfs_save_read,
    .write = vfs_save_write,
    .seek = vfs_save_seek,
    .close = vfs_save_close,
    .isfile_open = vfs_save_isfile_open,
    .opendir = vfs_save_opendir,
    .readdir = vfs_save_readdir,
    .dirlstat = vfs_save_dirlstat,
    .closedir = vfs_save_closedir,
    .isdir_open = vfs_save_isdir_open,
    .stat = vfs_save_stat,
    .lstat = vfs_save_stat,
    .mkdir = vfs_save_mkdir,
    .unlink = vfs_save_unlink,
    .rmdir = vfs_save_rmdir,
    .rename = vfs_save_rename,
};
