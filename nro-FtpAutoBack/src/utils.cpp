#include "utils.h"
#include <borealis/logger.hpp>
#include <borealis/application.hpp>
#include <switch.h>
#include <sstream>
#include <vector>

namespace utils {

    std::string readConfigOption(const std::string& sectionName, const std::string& optionKey, const std::string& defaultValue) {
        try {
            // 使用minIni-nx库读取配置项
            std::string value;
            char buffer[256];
            
            // 读取指定section中的配置项
            ini_gets(sectionName.c_str(), optionKey.c_str(), defaultValue.c_str(), buffer, sizeof(buffer), CONFIG_FILE);
            
            value = buffer;
            return value;
        }
        catch (const std::exception& e) {
            brls::Logger::error("读取配置项失败: %s", e.what());
            return defaultValue;
        }
    }

    bool writeConfigOption(const std::string& sectionName, const std::string& optionKey, const std::string& value) {
        try {
            // 使用minIni-nx库写入配置项
            bool result;
            
            // 写入指定section中的配置项
            result = ini_puts(sectionName.c_str(), optionKey.c_str(), value.c_str(), CONFIG_FILE);

            // 显示保存结果通知
            if (result) {
                brls::Application::notify("设置已保存");
            }
            else {
                brls::Application::notify("设置保存失败");
            }

            return result;
        }
        catch (const std::exception& e) {
            brls::Logger::error("写入配置项失败: %s", e.what());
            brls::Application::notify("设置保存失败");
            return false;
        }
    }

    void restartAutoBackup() {
        // 420000000000011B
        const uint64_t titleID = 0x420000000000011B;
        u64 pid = 0;

        // 初始化
        pmshellInitialize();
        
        // 检查插件是否正在运行
        if (R_FAILED(pmdmntGetProcessId(&pid, titleID)) || pid == 0) {
            // 插件未运行，直接返回
            return;
        }

        // 终止插件
        pmshellTerminateProgram(titleID);

        // 等待
        svcSleepThread(100000000); // 6个0 ms

        // 重启插件
        const NcmProgramLocation programLocation{
            .program_id = titleID,
            .storageID = NcmStorageId_None,
        };
        pid = 0;
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

            // 将时间格式从 YYYY.MM.DD@HH.MM.SS 转换为 YYYY年MM月DD日 HH时MM分SS秒
            // 查找@符号的位置
            size_t atPos = entry.time.find('@');
            if (atPos != std::string::npos) {
                std::string datePart = entry.time.substr(0, atPos);
                std::string timePart = entry.time.substr(atPos + 1);

                // 替换日期部分的点号为中文
                size_t firstDot = datePart.find('.');
                size_t secondDot = datePart.find('.', firstDot + 1);
                datePart.replace(secondDot, 1, "月");
                datePart.replace(firstDot, 1, "年");
                datePart += "日";

                // 替换时间部分的点号为中文
                size_t firstTimeDot = timePart.find('.');
                size_t secondTimeDot = timePart.find('.', firstTimeDot + 1);
                timePart.replace(secondTimeDot, 1, "分");
                timePart.replace(firstTimeDot, 1, "时");
                timePart += "秒";

                entry.time = datePart + " " + timePart;
            }
        }
    
        return entry;
    }
}