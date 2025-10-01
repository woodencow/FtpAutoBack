#include "webdav.h"
#include "utils.h"
#include <string>

WebDAVSettingTab::WebDAVSettingTab() {
    // 添加标题
    this->addView(new brls::Header("WebDAV设置"));

    // 添加说明文字
    this->addView(new brls::Label(brls::LabelStyle::DESCRIPTION, "修改WebDAV同步相关设置", true));

    // WebDAV服务器设置
    std::string originValue = utils::readConfigOption("Backup-WebDAV", "origin", "");
    brls::InputListItem* webdavOrigin = new brls::InputListItem("WebDAV服务器", originValue, "", "设置WebDAV服务器地址");
    webdavOrigin->setReduceDescriptionSpacing(true);
    webdavOrigin->getClickEvent()->subscribe([webdavOrigin](View* view) {
        std::string value = webdavOrigin->getValue();
        utils::writeConfigOption("Backup-WebDAV", "origin", value);
        utils::restartAutoBackup();
    });
    this->addView(webdavOrigin);

    // WebDAV路径设置
    std::string basePathValue = utils::readConfigOption("Backup-WebDAV", "basepath", "");
    brls::InputListItem* webdavBasePath = new brls::InputListItem("WebDAV路径", basePathValue, "", "设置WebDAV基础路径  不了解请勿改动");
    webdavBasePath->setReduceDescriptionSpacing(true);
    webdavBasePath->getClickEvent()->subscribe([webdavBasePath](View* view) {
        std::string value = webdavBasePath->getValue();
        utils::writeConfigOption("Backup-WebDAV", "basepath", value);
        utils::restartAutoBackup();
    });
    this->addView(webdavBasePath);

    // WebDAV账号设置
    std::string usernameValue = utils::readConfigOption("Backup-WebDAV", "username", "");
    // 移除可能存在的引号用于显示
    if (!usernameValue.empty() && usernameValue.front() == '"' && usernameValue.back() == '"') {
        usernameValue = usernameValue.substr(1, usernameValue.length() - 2);
    }
    brls::InputListItem* webdavUsername = new brls::InputListItem("账号", usernameValue, "", "设置WebDAV验证用户名  不了解请勿改动");
    webdavUsername->setReduceDescriptionSpacing(true);
    webdavUsername->getClickEvent()->subscribe([webdavUsername](View* view) {
        std::string value = webdavUsername->getValue();
        // 保存时添加引号
        std::string savedValue = "\"" + value + "\"";
        utils::writeConfigOption("Backup-WebDAV", "username", savedValue);
        utils::restartAutoBackup();
    });
    this->addView(webdavUsername);

    // WebDAV密码设置
    std::string passwordValue = utils::readConfigOption("Backup-WebDAV", "password", "");
    // 移除可能存在的引号用于显示
    if (!passwordValue.empty() && passwordValue.front() == '"' && passwordValue.back() == '"') {
        passwordValue = passwordValue.substr(1, passwordValue.length() - 2);
    }
    brls::InputListItem* webdavPassword = new brls::InputListItem("密码", passwordValue, "", "设置WebDAV验证密码");
    webdavPassword->setReduceDescriptionSpacing(true);
    webdavPassword->getClickEvent()->subscribe([webdavPassword](View* view) {
        std::string value = webdavPassword->getValue();
        // 保存时添加引号
        std::string savedValue = "\"" + value + "\"";
        utils::writeConfigOption("Backup-WebDAV", "password", savedValue);
        utils::restartAutoBackup();
    });
    this->addView(webdavPassword);
    
    this->registerAction("返回", brls::Key::B, [this] {
        brls::Application::popView();
        return true;
    });
}