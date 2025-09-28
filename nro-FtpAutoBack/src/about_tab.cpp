#include "about_tab.h"

#include <borealis.hpp>

AboutTab::AboutTab()
{
    // Title
    brls::Label *title = new brls::Label(
        brls::LabelStyle::REGULAR,
        "AUTO-Backup manager", 
        true
    );
    title->setHorizontalAlign(NVG_ALIGN_CENTER);
    this->addView(title);

    // Subtitle
    brls::Label *subTitle = new brls::Label(
        brls::LabelStyle::REGULAR,
        "Switch自动备份插件，由塔菲的神必团队开发", 
        true
    );
    subTitle->setHorizontalAlign(NVG_ALIGN_CENTER);
    this->addView(subTitle);


    // Links
    this->addView(new brls::Header("相关链接"));
    brls::Label *links = new brls::Label(
        brls::LabelStyle::DESCRIPTION,
        "作者 : B站 Hahappify\n",
        true
    );
    this->addView(links);
}