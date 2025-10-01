#include "ftp_setting_tab.h"
#include "utils.h"
#include <string>
#include <algorithm>

FTPSettingTab::FTPSettingTab() {
    // 添加标题
    this->addView(new brls::Header("FTP设置"));

    // 添加说明文字
    this->addView(new brls::Label(brls::LabelStyle::DESCRIPTION, "修改FTP传输相关设置", true));

    // FTP端口设置
    std::string ftpPortValue = utils::readConfigOption("Ftp-Network", "port", "21");
    brls::InputListItem* ftpPort = new brls::InputListItem("FTP端口", ftpPortValue, "", "设置FTP服务端口 (理论可用范围:1~65535)");
    ftpPort->setReduceDescriptionSpacing(true);
    ftpPort->getClickEvent()->subscribe([ftpPort](View* view) {
        std::string value = ftpPort->getValue();
        
        // 验证端口范围
        int port = 21;
        try {
            port = std::stoi(value);
        } catch (const std::exception& e) {
            port = 21;
        }
        
        if (port < 1 || port > 65535) {
            port = 21;
        }
        
        // 更新显示值
        std::string validatedValue = std::to_string(port);
        ftpPort->setValue(validatedValue);
        
        // 保存到配置文件
        utils::writeConfigOption("Ftp-Network", "port", validatedValue);
        utils::restartAutoBackup();
    });
    this->addView(ftpPort);

    // FTP密码开关
    std::string anonValue = utils::readConfigOption("Ftp-Login", "anon", "1");
    bool anonEnabled = (anonValue == "1"); // 1表示关闭验证（匿名登录），0表示开启验证
    brls::ToggleListItem* ftpVerify = new brls::ToggleListItem("启用FTP验证", !anonEnabled, "是否需要登录验证");
    ftpVerify->setReduceDescriptionSpacing(true);
    ftpVerify->getClickEvent()->subscribe([ftpVerify](View* view) {
        bool toggleState = ftpVerify->getToggleState();
        // toggleState true 启用密码 0
        // toggleState false 禁用密码 1
        std::string value = toggleState ? "0" : "1";
        utils::writeConfigOption("Ftp-Login", "anon", value);
        utils::restartAutoBackup();
    });
    this->addView(ftpVerify);

    // FTP用户名设置
    std::string userValue = utils::readConfigOption("Ftp-Login", "user", "user");
    // 移除可能存在的引号用于显示
    if (!userValue.empty() && userValue.front() == '"' && userValue.back() == '"') {
        userValue = userValue.substr(1, userValue.length() - 2);
    }
    brls::InputListItem* ftpUser = new brls::InputListItem("用户名", userValue, "", "设置FTP验证用户名");
    ftpUser->setReduceDescriptionSpacing(true);
    ftpUser->getClickEvent()->subscribe([ftpUser](View* view) {
        std::string value = ftpUser->getValue();
        // 保存时添加引号
        std::string savedValue = "\"" + value + "\"";
        utils::writeConfigOption("Ftp-Login", "user", savedValue);
        utils::restartAutoBackup();
    });
    this->addView(ftpUser);

    // FTP密码设置
    std::string passValue = utils::readConfigOption("Ftp-Login", "pass", "1234567890");
    // 移除可能存在的引号用于显示
    if (!passValue.empty() && passValue.front() == '"' && passValue.back() == '"') {
        passValue = passValue.substr(1, passValue.length() - 2);
    }
    brls::InputListItem* ftpPass = new brls::InputListItem("密码", passValue, "", "设置FTP验证密码");
    ftpPass->setReduceDescriptionSpacing(true);
    ftpPass->getClickEvent()->subscribe([ftpPass](View* view) {
        std::string value = ftpPass->getValue();
        // 保存时添加引号
        std::string savedValue = "\"" + value + "\"";
        utils::writeConfigOption("Ftp-Login", "pass", savedValue);
        utils::restartAutoBackup();
    });
    this->addView(ftpPass);
}