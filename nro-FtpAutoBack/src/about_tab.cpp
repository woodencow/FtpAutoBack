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
        "Switch自动备份插件，由塔菲的神必团队开发\nHahappify & TOM SON & 葡萄糖酸菜鱼\n感谢ITotalJustice的FTP-SRV和switch开源社区的技术基础",
        true
    );
    subTitle->setHorizontalAlign(NVG_ALIGN_CENTER);
    this->addView(subTitle);


    // Links
    this->addView(new brls::Header(""));
    brls::Label *links = new brls::Label(
        brls::LabelStyle::DESCRIPTION,
        "\n",
        true
    );
    this->addView(links);
}