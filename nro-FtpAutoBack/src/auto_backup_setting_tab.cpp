#include "auto_backup_setting_tab.h"
#include "utils.h"
#include "webdav.h"
#include <string>
#include <algorithm>

AutoBackupSettingTab::AutoBackupSettingTab() {

    // 标题
    this->addView(new brls::Header("自动备份设置"));
    this->addView(new brls::Label(brls::LabelStyle::DESCRIPTION, "修改自动备份相关设置", true));

    // 启用自动备份功能开关
    std::string autoBackupEnabled = utils::readConfigOption("Backup-Basic Settings", "auto_backup_enabled", "1");
    bool enabled = (autoBackupEnabled == "1");
    brls::ToggleListItem* autoBackupEnable = new brls::ToggleListItem("启用自动备份", enabled, "是否启用自动备份功能");
    autoBackupEnable->setReduceDescriptionSpacing(true);
    autoBackupEnable->getClickEvent()->subscribe([autoBackupEnable](View* view) {
        bool toggleState = autoBackupEnable->getToggleState();
        std::string value = toggleState ? "1" : "0";
        utils::writeConfigOption("Backup-Basic Settings", "auto_backup_enabled", value);
        utils::restartAutoBackup();
    });
    this->addView(autoBackupEnable);

    // 自动备份上传开关
    std::string webdavEnabled = utils::readConfigOption("Backup-WebDAV", "WebDAV_enabled", "0");
    bool webdavEnabledState = (webdavEnabled == "1");
    brls::ToggleListItem* backupEnable = new brls::ToggleListItem("启用自动备份上传", webdavEnabledState, "自动上传到配置的Webdav服务器");
    backupEnable->setReduceDescriptionSpacing(true);
    backupEnable->getClickEvent()->subscribe([backupEnable](View* view) {
        bool toggleState = backupEnable->getToggleState();
        std::string value = toggleState ? "1" : "0";
        utils::writeConfigOption("Backup-WebDAV", "WebDAV_enabled", value);
        utils::restartAutoBackup();
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
        
        utils::writeConfigOption("Backup-Basic Settings", "maxback", validatedValue);
        utils::restartAutoBackup();
    });
    this->addView(Maxbackup);

    // 备份时LED闪烁设置
    std::string backupLed = utils::readConfigOption("Backup-Basic Settings", "backup_led", "0");
    bool ledEnabled = (backupLed == "1");
    brls::ToggleListItem* backupLedToggle = new brls::ToggleListItem("备份时呼吸灯提示", ledEnabled, "备份时是否通过呼吸等提示（Lite不可用）");
    backupLedToggle->setReduceDescriptionSpacing(true);
    backupLedToggle->getClickEvent()->subscribe([backupLedToggle](View* view) {
        bool toggleState = backupLedToggle->getToggleState();
        std::string value = toggleState ? "1" : "0";
        utils::writeConfigOption("Backup-Basic Settings", "backup_led", value);
    });
    this->addView(backupLedToggle);

    // 备份时弹窗通知设置
    std::string backupNotify = utils::readConfigOption("Backup-Basic Settings", "backup_notify", "0");
    bool notifyEnabled = (backupNotify == "1");
    brls::ToggleListItem* backupNotifyToggle = new brls::ToggleListItem("备份时弹窗通知", notifyEnabled, "备份时是否弹窗通知（需要配合Ultrahand 2.1.0+）");
    backupNotifyToggle->setReduceDescriptionSpacing(true);
    backupNotifyToggle->getClickEvent()->subscribe([backupNotifyToggle](View* view) {
        bool toggleState = backupNotifyToggle->getToggleState();
        std::string value = toggleState ? "1" : "0";
        utils::writeConfigOption("Backup-Basic Settings", "backup_notify", value);
    });
    this->addView(backupNotifyToggle);

    // 备份服务器设置
    brls::ListItem* WebdavCFG = new brls::ListItem("云同步服务器", "配置Webdav服务器");
    WebdavCFG->setReduceDescriptionSpacing(true);
    WebdavCFG->getClickEvent()->subscribe([](View* view) {
        // 点击后跳转到WebDAV设置页面
        brls::Application::pushView(new WebDAVSettingTab());
    });
    this->addView(WebdavCFG);


}