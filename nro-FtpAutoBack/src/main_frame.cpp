#include "main_frame.h"
#include "ftp_setting_tab.h"
#include "auto_backup_setting_tab.h"
#include "about_tab.h"

MainFrame::MainFrame() : TabFrame()
{
    this->setTitle("存档自动云备份 设置管理器");

    this->addTab("自动备份设置", new AutoBackupSettingTab());
    this->addTab("FTP传输设置", new FTPSettingTab());
    this->addTab("关于", new AboutTab());
}

MainFrame::~MainFrame()
{
    
}