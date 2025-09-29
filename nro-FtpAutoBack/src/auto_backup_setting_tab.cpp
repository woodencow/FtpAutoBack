#include "auto_backup_setting_tab.h"
#include "utils.h"
#include "webdav.h"
#include <string>
#include <algorithm>

AutoBackupSettingTab::AutoBackupSettingTab() {

    // 标题
    this->addView(new brls::Header("自动备份设置"));
    this->addView(new brls::Label(brls::LabelStyle::DESCRIPTION, "修改自动备份相关设置", true));

    // 自动备份上传开关
    std::string webdavEnabled = utils::readConfigOption("Backup-WebDAV", "WebDAV_enabled", "0");
    bool enabled = (webdavEnabled == "1");
    brls::ToggleListItem* backupEnable = new brls::ToggleListItem("启用自动备份上传", enabled, "自动上传到配置的Webdav服务器");
    backupEnable->setReduceDescriptionSpacing(true);
    backupEnable->getClickEvent()->subscribe([backupEnable](View* view) {
        bool toggleState = backupEnable->getToggleState();
        std::string value = toggleState ? "1" : "0";
        if (utils::writeConfigOption("Backup-WebDAV", "WebDAV_enabled", value)) {
            brls::Application::notify("设置已保存");
            utils::restartAutoBackup();
        } else {
            brls::Application::notify("设置保存失败");
        }
    });
    this->addView(backupEnable);

    // 最大备份数量设置
    std::string maxBack = utils::readConfigOption("Backup-Basic Settings", "maxback", "5");
    brls::InputListItem* Maxbackup = new brls::InputListItem("最大备份数量", maxBack, "", "设置每个游戏存档文件的最大备份次数 (1~10 | 0表示禁用限制,可能导致报错)");
    Maxbackup->setReduceDescriptionSpacing(true);
    Maxbackup->getClickEvent()->subscribe([Maxbackup](View* view) {
        std::string value = Maxbackup->getValue();
        
        // 验证输入值
        int maxBackValue = 5; // 默认值
        try {
            maxBackValue = std::stoi(value);
        } catch (const std::exception& e) {
            // 如果转换失败，使用默认值5
            maxBackValue = 5;
        }
        
        // 限制范围：0-10，超出范围强制设为边界值
        if (maxBackValue > 10) {
            maxBackValue = 10;
        } else if (maxBackValue < 0) {
            maxBackValue = 5;
        }
        
        // 转换回字符串
        std::string validatedValue = std::to_string(maxBackValue);
        
        // 更新输入框显示的值
        Maxbackup->setValue(validatedValue);
        
        if (utils::writeConfigOption("Backup-Basic Settings", "maxback", validatedValue)) {
            brls::Application::notify("设置已保存");
            utils::restartAutoBackup();
        } else {
            brls::Application::notify("设置保存失败");
        }
    });
    this->addView(Maxbackup);

    // 备份服务器设置
    brls::ListItem* WebdavCFG = new brls::ListItem("云同步服务器", "配置Webdav服务器");
    WebdavCFG->setReduceDescriptionSpacing(true);
    WebdavCFG->getClickEvent()->subscribe([](View* view) {
        // 点击后跳转到WebDAV设置页面
        brls::Application::pushView(new WebDAVSettingTab());
    });
    this->addView(WebdavCFG);


}