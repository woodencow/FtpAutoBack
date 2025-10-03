#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <switch.h>

#define CONFIG_FILE "sdmc:/config/ftpsrv/config.ini"

// 添加minIni-nx库的包含
extern "C" {
#include <minIni.h>
}

namespace utils {
    
    /**
     * 备份记录结构体
     */
    typedef struct {
        std::string status;     // 状态
        std::string gameName;   // 游戏名
        std::string userName;   // 用户名
        std::string time;       // 时间
    } BackupLogEntry;

    /**
     * 读取INI配置文件中指定节和选项的值
     * @param sectionName 节名称
     * @param optionKey 选项键名
     * @param defaultValue 默认值（可选）
     * @return 选项的值，如果不存在则返回默认值
     */
    std::string readConfigOption(const std::string& sectionName, const std::string& optionKey, const std::string& defaultValue = "");

    /**
     * 写入INI配置文件中指定节和选项的值
     * @param sectionName 节名称
     * @param optionKey 选项键名
     * @param value 要写入的值
     * @return 是否写入成功
     */
    bool writeConfigOption(const std::string& sectionName, const std::string& optionKey, const std::string& value);
    
    /**
     * 重启自动备份功能
     */
    void restartAutoBackup();
    
    /**
     * 解析备份日志条目字符串
     * @param line 格式为 "状态|游戏名|用户名|时间" 的字符串
     * @return 解析后的BackupLogEntry结构体
     */
    BackupLogEntry parseBackupLogEntry(const std::string& line);

}

#endif // UTILS_H