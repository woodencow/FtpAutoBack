#include "backup_log_tab.h"
#include "utils.h"

#include <borealis.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

BackupLogTab::BackupLogTab() : brls::List()
{
    // 设置列表间隔为更小的值
    this->setSpacing(0);
    
    // 终止插件
    const uint64_t titleID = 0x420000000000011B;
    pmshellInitialize();
    pmshellTerminateProgram(titleID);

    // 将backuplog.txt复制为backup.txt
    std::ifstream sourceFile("sdmc:/AutoBack/backuplog.txt", std::ios::binary);
    if (sourceFile.is_open()) {
        // 删除已存在的目标文件
        remove("sdmc:/AutoBack/backup.txt");
        
        // 复制文件
        std::ofstream dst("sdmc:/AutoBack/backup.txt", std::ios::binary);
        if (dst.is_open()) {
            dst << sourceFile.rdbuf();
            dst.close();
        }
        sourceFile.close();
    }

    // 重启插件
        const NcmProgramLocation programLocation{
            .program_id = titleID,
            .storageID = NcmStorageId_None,
        };
        u64 pid = 0;
        pmshellLaunchProgram(0, &programLocation, &pid);
    
    // 添加标题
    this->addView(new brls::Header("备份记录"));

    // 尝试读取备份记录文件
    std::ifstream logFile("sdmc:/AutoBack/backup.txt");
    std::vector<std::string> lines;
    std::string line;
    
    // 读取所有行
    if (logFile.is_open()) {
        while (std::getline(logFile, line)) {
            if (!line.empty()) {
                lines.push_back(line);
            }
        }
        logFile.close();
    }
    
    if (!lines.empty()) {
        std::reverse(lines.begin(), lines.end());
        
        size_t count = std::min(lines.size(), static_cast<size_t>(10));
        
        for (size_t i = 0; i < count; ++i) {
            utils::BackupLogEntry entry = utils::parseBackupLogEntry(lines[i]);
            
            std::string subText = entry.gameName + " (" + entry.userName + ")";
            std::string subText2 = entry.time + " (" + entry.status + ")";
            brls::ListItem* item = new brls::ListItem(subText, "", subText2);
            item->setReduceDescriptionSpacing(true);
            this->addView(item);
        }
    } else {
        brls::ListItem* item = new brls::ListItem("无备份记录");
        item->setReduceDescriptionSpacing(true);
        this->addView(item);
    }
}

BackupLogTab::~BackupLogTab()
{
}