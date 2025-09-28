#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <switch.h>

#define CONFIG_FILE "sdmc:/config/ftpsrv/config.ini"

namespace utils {
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

}

#endif // UTILS_H