#include "utils.h"
#include <SimpleIniParser.hpp>
#include <borealis/logger.hpp>
#include <switch.h>
#include <sstream>
#include <vector>

using namespace simpleIniParser;

namespace utils {
    
    std::string readConfigOption(const std::string& sectionName, const std::string& optionKey, const std::string& defaultValue) {
        try {
            // 解析配置文件
            Ini* configIni = Ini::parseFile(CONFIG_FILE);
            
            // 查找指定的节
            IniSection* section = configIni->findSection(sectionName);
            if (section == nullptr) {
                delete configIni;
                return defaultValue;
            }
            
            // 在节中查找指定的选项
            IniOption* option = section->findFirstOption(optionKey);
            if (option == nullptr) {
                delete configIni;
                return defaultValue;
            }
            
            std::string value = option->value;
            delete configIni;
            return value;
        }
        catch (const std::exception& e) {
            brls::Logger::error("读取配置项失败: %s", e.what());
            return defaultValue;
        }
    }
    
    bool writeConfigOption(const std::string& sectionName, const std::string& optionKey, const std::string& value) {
        try {
            // 解析配置文件
            Ini* configIni = Ini::parseFile(CONFIG_FILE);
            
            // 查找或创建节
            IniSection* section = configIni->findOrCreateSection(sectionName);
            
            // 查找或创建选项并设置值
            IniOption* option = section->findOrCreateFirstOption(optionKey, value);
            option->value = value;
            
            // 保存文件
            bool result = configIni->writeToFile(CONFIG_FILE);
            
            // 清理内存
            delete configIni;
            
            return result;
        }
        catch (const std::exception& e) {
            brls::Logger::error("写入配置项失败: %s", e.what());
            return false;
        }
    }
    
    void restartAutoBackup() {
        // 420000000000011B
        const uint64_t titleID = 0x420000000000011B;
        
        // 初始化
        pmshellInitialize();
        
        // 终止插件
        pmshellTerminateProgram(titleID);
        
        // 等待
        svcSleepThread(100000000); // 6个0 ms
        
        // 重启插件
        const NcmProgramLocation programLocation{
            .program_id = titleID,
            .storageID = NcmStorageId_None,
        };
        u64 pid = 0;
        pmshellLaunchProgram(0, &programLocation, &pid);
    }
    
    BackupLogEntry parseBackupLogEntry(const std::string& line) {
        BackupLogEntry entry;
        
        // 使用stringstream分割字符串
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> tokens;
        
        // 按'|'分割字符串
        while (std::getline(ss, token, '|')) {
            tokens.push_back(token);
        }
        
        // 确保有足够的字段
        if (tokens.size() >= 4) {
            entry.status = tokens[0];
            entry.gameName = tokens[1];
            entry.userName = tokens[2];
            entry.time = tokens[3];
            
            // 将时间格式从 YYYY.MM.DD@HH.MM.SS 转换为 YYYY年MM月DD日HH时MM分
            // 查找@符号的位置
            size_t atPos = entry.time.find('@');
            if (atPos != std::string::npos) {
                std::string datePart = entry.time.substr(0, atPos);
                std::string timePart = entry.time.substr(atPos + 1);
                
                // 替换日期部分的点号为中文
                for (size_t i = 0; i < datePart.length(); ++i) {
                    if (datePart[i] == '.') {
                        if (datePart.substr(0, i).find('.') == std::string::npos) {
                            datePart.replace(i, 1, "年");
                        } else if (datePart.substr(0, i).find("年") != std::string::npos && 
                                  datePart.substr(0, i).substr(datePart.substr(0, i).find("年")+2).find('.') == std::string::npos) {
                            datePart.replace(i, 1, "月");
                        } else {
                            datePart.replace(i, 1, "日");
                        }
                    }
                }
                
                // 替换时间部分的点号为中文
                for (size_t i = 0; i < timePart.length(); ++i) {
                    if (timePart[i] == '.') {
                        if (timePart.substr(0, i).find('.') == std::string::npos) {
                            timePart.replace(i, 1, "时");
                        } else {
                            timePart.replace(i, 1, "分");
                        }
                    }
                }
                
                entry.time = datePart + timePart;
            }
        }
        
        return entry;
    }
}