#ifndef DEVICE_MAPPING_H
#define DEVICE_MAPPING_H

#include <string.h>

// 设备名称映射结构体
typedef struct {
    const char* user_friendly_name;  // 用户友好名称（显示给用户看的）
    const char* system_name;         // 系统内部名称（实际挂载的名称）
} DeviceNameMap;

// 设备名称映射表 - 内联定义
static const DeviceNameMap g_device_name_maps[] = {
    {"01. SD卡", "sdmc"},
    {"05. 相册(虚拟)", "album_sd"},
    {"06. 相册(正版)", "album_nand"},
};
static const int g_device_name_map_count = sizeof(g_device_name_maps) / sizeof(DeviceNameMap);

// 根据系统名称查找用户友好名称
static inline const char* get_user_friendly_name(const char* system_name) {
    for (int i = 0; i < g_device_name_map_count; i++) {
        if (strcmp(system_name, g_device_name_maps[i].system_name) == 0) {
            return g_device_name_maps[i].user_friendly_name;
        }
    }
    return system_name;  // 如果没有找到映射，返回原始名称
}

// 根据用户友好名称查找系统名称
static inline const char* get_system_name(const char* user_name) {
    for (int i = 0; i < g_device_name_map_count; i++) {
        if (strcmp(user_name, g_device_name_maps[i].user_friendly_name) == 0) {
            return g_device_name_maps[i].system_name;
        }
    }
    return user_name;  // 如果没有找到映射，返回原始名称
}

#endif // DEVICE_MAPPING_H