#include "utils.h"
#include <SimpleIniParser.hpp>
#include <borealis/logger.hpp>
#include <switch.h>

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
}