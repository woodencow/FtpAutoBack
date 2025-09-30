#pragma once

#include <borealis.hpp>

#include "ftp_setting_tab.h"
#include "auto_backup_setting_tab.h"
#include "about_tab.h"
#include "backup_log_tab.h"

class MainFrame : public brls::TabFrame
{
    public:
        MainFrame();
        ~MainFrame();
};