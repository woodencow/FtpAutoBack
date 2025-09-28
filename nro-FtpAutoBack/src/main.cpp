/*
    AUTOBackup manager, a sys-clk frontend homebrew
    Copyright (C) 2019-2020  natinusala
    Copyright (C) 2019  p-sam
    Copyright (C) 2019-2020  m4xw

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string>

#include <borealis.hpp>
#include <switch.h>

#include "main_frame.h"

int main(int argc, char* argv[]) {
    // 初始化Switch系统库
    socketInitializeDefault();
    nxlinkStdio();

    // 初始化Borealis应用框架
    if (!brls::Application::init("APP_TITLE"))
    {
        brls::Logger::error("无法初始化Borealis应用程序");
        return EXIT_FAILURE;
    }

    // 设置日志级别
    brls::Logger::setLogLevel(brls::LogLevel::DEBUG);

    // 加载中文字体支持
    PlFontData font;
    Result rc = plGetSharedFontByType(&font, PlSharedFontType_ChineseSimplified);
    if (R_SUCCEEDED(rc)) 
    {
        brls::Logger::info("添加中文字体支持");
        int chineseFont = brls::Application::loadFontFromMemory("chinese", font.address, font.size, false);
        nvgAddFallbackFontId(brls::Application::getNVGContext(), brls::Application::getFontStash()->regular, chineseFont);
    } 
    else 
    {
        brls::Logger::error("无法加载中文字体");
    }

    // 创建并推送主界面
    brls::Application::pushView(new MainFrame());

    // 运行应用主循环
    while (brls::Application::mainLoop());

    // 退出应用
    socketExit();
    
    return EXIT_SUCCESS;
}