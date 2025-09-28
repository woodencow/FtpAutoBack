#define TESLA_INIT_IMPL // 如果有多个文件使用 tesla 头文件，只在主文件中定义这个
#include <tesla.hpp>    // Tesla 头文件
#include <vector>       // 用于std::vector支持
#include <minIni.h>     // minIni库用于读取INI配置文件

// ===========================================
// 全局常量定义
// ===========================================

// 全局版本常量 - 已包含v前缀
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
const char* const GLOBAL_APP_VERSION = "v" TOSTRING(APP_VERSION);

// 全局UI元素引用，用于在子界面返回时更新主界面状态
static tsl::elm::ListItem* g_statusItem = nullptr;
static tsl::elm::ListItem* g_restartItem = nullptr;
static tsl::elm::ListItem* g_backupCountItem = nullptr;
static tsl::elm::ListItem* g_timeoutItem = nullptr;



// 配置文件路径常量
const char* CONFIG_FILE_PATH = "/config/ftpsrv/config.ini";

// ===========================================
// 多彩文本支持结构体和颜色常量
// ===========================================

// 文本段落结构体，用于支持字符级别的颜色和大小控制
struct TextSegment {
    const char* text;       // 文本内容
    tsl::Color color;       // 文本颜色
    u16 fontSize;           // 字体大小
};

// 预定义颜色常量命名空间
namespace TextColors {
    const tsl::Color WHITE = {0xF, 0xF, 0xF, 0xF};      // 白色
    const tsl::Color BLACK = {0x9, 0x9, 0x9, 0xF};      // 黑色（更亮）
    const tsl::Color RED = {0xF, 0x6, 0x6, 0xF};        // 红色（更亮）
    const tsl::Color GREEN = {0x6, 0xF, 0x6, 0xF};      // 绿色（更亮）
    const tsl::Color BLUE = {0x6, 0xA, 0xF, 0xF};       // 蓝色（更亮）
    const tsl::Color YELLOW = {0xF, 0xF, 0x4, 0xF};     // 黄色（稍微调亮）
    const tsl::Color CYAN = {0x4, 0xF, 0xF, 0xF};       // 青色（稍微调亮）
    const tsl::Color MAGENTA = {0xF, 0x4, 0xF, 0xF};    // 洋红色（稍微调亮）
    const tsl::Color ORANGE = {0xF, 0xC, 0x4, 0xF};     // 橙色（更亮）
    const tsl::Color PURPLE = {0xD, 0x6, 0xF, 0xF};     // 紫色（更亮）
    const tsl::Color PINK = {0xF, 0xA, 0xD, 0xF};       // 粉色（更亮）
    const tsl::Color LIME = {0xA, 0xF, 0x6, 0xF};       // 酸橙色（更亮）
    const tsl::Color GRAY = {0xD, 0xD, 0xD, 0xF};       // 灰色（更亮）
    const tsl::Color LIGHT_GRAY = {0xE, 0xE, 0xE, 0xF}; // 浅灰色（更亮）
    const tsl::Color DARK_GRAY = {0xA, 0xA, 0xA, 0xF};  // 深灰色（更亮）
}


// ===========================================
// GUI类区域 - 每个功能页面一个独立的GUI类
// ===========================================

// 通用文本显示GUI类 - 可以接受任意标题和内容
class TextDisplayGui : public tsl::Gui {
private:
    char m_title[32];              // 页面标题缓冲区
    char m_subtitle[32];           // 页面副标题缓冲区
    TextSegment* m_segments;        // 多彩文本段落数组指针
    size_t m_segmentCount;          // 文本段落数量
    std::vector<TextSegment> m_segmentVector;  // vector存储

public:
    // 构造函数 - 接受标题、副标题和vector
    TextDisplayGui(const char* title, const char* subtitle, const std::vector<TextSegment>& segments) 
        : m_segmentVector(segments) {
        // 安全复制标题
        strncpy(m_title, title, sizeof(m_title) - 1);
        m_title[sizeof(m_title) - 1] = '\0';
        
        // 安全复制副标题
        strncpy(m_subtitle, subtitle, sizeof(m_subtitle) - 1);
        m_subtitle[sizeof(m_subtitle) - 1] = '\0';
        
        // 设置指针和数量（指向vector的数据）
        m_segments = const_cast<TextSegment*>(m_segmentVector.data());
        m_segmentCount = m_segmentVector.size();
    }

    virtual tsl::elm::Element* createUI() override {
        // 创建主框架，使用传入的标题和副标题（如果有副标题则使用，否则使用版本号）
        const char* subtitle = (strlen(m_subtitle) > 0) ? m_subtitle : GLOBAL_APP_VERSION;
        auto frame = new tsl::elm::OverlayFrame(m_title, subtitle);
        
        // 多彩文本模式 - 使用TextSegment数组，支持对齐，使用Tesla框架提供的正确绘制区域
        auto colorTextDrawer = new tsl::elm::CustomDrawer([this](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            // 直接使用Tesla框架提供的绘制区域
            // Tesla框架已经为我们计算好了正确的内容区域：
            // x=35, y=125, w=363, h=522（相对于FramebufferWidth=448, FramebufferHeight=720）
            s32 drawX = x;
            s32 drawY = y;
            s32 drawW = w;
            s32 drawH = h;
            
            s32 lineHeight = 25;       // 行高
            
            // 文本绘制逻辑
            s32 currentY = drawY + 20; // 添加上边距，避免文本显示在区域外
            s32 currentX = drawX;      // 当前X位置，持续跟踪
            
            // 遍历所有文本段落
            for (size_t i = 0; i < m_segmentCount; i++) {
                const TextSegment& segment = m_segments[i];
                const char* text = segment.text;
                
                // 检查是否超出可用绘制区域
                if (currentY + lineHeight > drawY + drawH) {
                    break; // 超出可用绘制区域，停止绘制
                }
                
                // 处理换行符
                if (strstr(text, "\n") != nullptr) {
                    // 包含换行符，需要分段处理
                    std::string textStr(text);
                    size_t pos = 0;
                    size_t newlinePos;
                    
                    while ((newlinePos = textStr.find('\n', pos)) != std::string::npos) {
                        if (newlinePos > pos) {
                            // 绘制换行符前的文本
                            std::string lineText = textStr.substr(pos, newlinePos - pos);
                            if (!lineText.empty()) {
                                // 使用透明颜色获取文本尺寸
                                auto textSize = renderer->drawString(lineText.c_str(), false, 0, 0, 
                                                                   segment.fontSize, {0, 0, 0, 0});
                                
                                // 检查是否超出右边界
                                if (currentX + static_cast<s32>(textSize.first) > drawX + drawW) {
                                    // 换行
                                    currentX = drawX;
                                    currentY += lineHeight;
                                    if (currentY + lineHeight > drawY + drawH) break;
                                }
                                
                                // 绘制文本
                                renderer->drawString(lineText.c_str(), false, currentX, currentY, 
                                                   segment.fontSize, segment.color);
                                currentX += textSize.first;
                            }
                        }
                        
                        // 处理换行符 - 移动到下一行
                        currentX = drawX;
                        currentY += lineHeight;
                        if (currentY + lineHeight > drawY + drawH) break;
                        
                        pos = newlinePos + 1;
                    }
                    
                    // 处理最后一段文本（换行符后的部分）
                    if (pos < textStr.length() && currentY + lineHeight <= drawY + drawH) {
                        std::string lineText = textStr.substr(pos);
                        if (!lineText.empty()) {
                            // 使用透明颜色获取文本尺寸
                            auto textSize = renderer->drawString(lineText.c_str(), false, 0, 0, 
                                                               segment.fontSize, {0, 0, 0, 0});
                            
                            // 检查是否超出右边界
                            if (currentX + static_cast<s32>(textSize.first) > drawX + drawW) {
                                // 换行
                                currentX = drawX;
                                currentY += lineHeight;
                                if (currentY + lineHeight > drawY + drawH) break;
                            }
                            
                            // 绘制文本
                            renderer->drawString(lineText.c_str(), false, currentX, currentY, 
                                               segment.fontSize, segment.color);
                            currentX += textSize.first;
                        }
                    }
                } else {
                    // 不包含换行符的普通文本
                    // 使用透明颜色获取文本尺寸
                    auto textSize = renderer->drawString(text, false, 0, 0, 
                                                       segment.fontSize, {0, 0, 0, 0});
                    
                    // 检查是否需要换行
                    if (currentX + static_cast<s32>(textSize.first) > drawX + drawW) {
                        currentX = drawX;
                        currentY += lineHeight;
                        if (currentY + lineHeight > drawY + drawH) break;
                    }
                    
                    // 绘制文本
                    renderer->drawString(text, false, currentX, currentY, 
                                       segment.fontSize, segment.color);
                    
                    // 更新X位置，继续在同一行
                    currentX += textSize.first;
                }
            }
        });
        frame->setContent(colorTextDrawer);
        
        return frame;
    }
    
    virtual void update() override { }
    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override {
        return false;
    }
};

// ===========================================
// 模块管理类 - 单例模式，用于管理FTP插件
// ===========================================

class ModuleManager {
private:
    // 模块ID - FTP插件的程序ID
    static constexpr u64 MODULE_ID = 0x420000000000011B;
    
    // 私有构造函数
    ModuleManager() {}
    
    // 禁止复制
    ModuleManager(const ModuleManager&) = delete;
    ModuleManager& operator=(const ModuleManager&) = delete;
    
public:
    // 获取单例实例
    static ModuleManager& getInstance() {
        static ModuleManager instance;
        return instance;
    }
    
    // 检查模块是否运行
    bool isRunning() {
        u64 pid = 0;
        // 直接使用已初始化的pmdmnt服务（libtesla已初始化）
        if (R_FAILED(pmdmntGetProcessId(&pid, MODULE_ID)))
            return false;
            
        return pid > 0;
    }
    
    // 启动模块
    Result startModule() {
        Result rc = 0;
        
        // 如果已经运行，则不需要启动
        if (isRunning()) {
            return 0;
        }
        
        // 直接使用pmshell启动程序（pmshell已在程序启动时初始化）
        NcmProgramLocation programLocation = {
            .program_id = MODULE_ID,
            .storageID = NcmStorageId_None
        };
        
        u64 pid = 0;
        rc = pmshellLaunchProgram(0, &programLocation, &pid);
        
        return rc;
    }
    
    // 停止模块
    Result stopModule() {
        Result rc = 0;
        
        // 如果没有运行，则不需要停止
        if (!isRunning()) {
            return 0;
        }
        
        // 直接使用pmshell终止程序（pmshell已在程序启动时初始化）
        rc = pmshellTerminateProgram(MODULE_ID);
        
        return rc;
    }
    
    // 重启模块
    Result restartModule() {
        Result rc = stopModule();
        if (R_SUCCEEDED(rc)) {
            // 等待一小段时间确保模块完全停止
            svcSleepThread(50000000ULL); // 等待50毫秒
            rc = startModule();
        }
        return rc;
    }
};



// 超时时间选择界面
class TimeoutGui : public tsl::Gui {   
private:
    int currentTimeout; // 当前选中的超时时间
    tsl::elm::List* list; // 保存list指针用于延迟焦点设置
    bool needsRefocus; // 标记是否需要重新设置焦点
    int frameCounter; // 帧计数器，用于延迟几帧后设置焦点
    
public:
    TimeoutGui(int timeout) : currentTimeout(timeout), list(nullptr), needsRefocus(true), frameCounter(0) {}
    
    // 绘制界面
    virtual tsl::elm::Element* createUI() override {
        auto frame = new tsl::elm::OverlayFrame("超时时间", "选择超时时间");
        list = new tsl::elm::List(); // 保存list指针到成员变量
        list->addItem(new tsl::elm::CategoryHeader("单位：秒  0表示关闭超时"));
        // 添加超时选项：0, 10, 20, 30, 40, 50
        int timeoutOptions[] = {0, 10, 20, 30, 40, 50};
        int optionCount = sizeof(timeoutOptions) / sizeof(timeoutOptions[0]);
        
        // 检查当前超时时间是否在有效选项中，如果不是则设为0
        for (int i = 0; i < optionCount; i++) {
            if (currentTimeout == timeoutOptions[i]) {
                break;
            }
            
            if (i == optionCount - 1) {
                currentTimeout = 0;
            }
        }
        
        for (int idx = 0; idx < optionCount; idx++) {
            int timeoutValue = timeoutOptions[idx];
            char num[4];
            sprintf(num, "%d", timeoutValue);
            
            auto item = new tsl::elm::ListItem(num);
            // 设置当前超时时间等于该选项的项为选中状态
            if (timeoutValue == currentTimeout) {
                item->setValue("\uE14B");
                currentTimeout = idx;
            }
            item->setClickListener([this, timeoutValue, num](u64 keys) {
                if (keys & HidNpadButton_A) {

                    // 保存到配置文件
                    ini_puts("Network", "timeout", num, CONFIG_FILE_PATH);
                    g_timeoutItem->setValue(num);
                    g_restartItem->setValue("需要重启");
                    tsl::goBack();
                    return true;
                }
                return false;
            });
            list->addItem(item);
        }
        
        frame->setContent(list);
        
        return frame;
    }
    
    virtual void update() override {
        // 延迟焦点重设逻辑：等待几帧后再设置焦点
        if (needsRefocus && list != nullptr) {
            frameCounter++;
            // 等待3帧后设置焦点，确保Tesla框架的初始化完成
            if (frameCounter >= 2) {
                // 设置焦点到正确的项目
                list->setFocusedIndex(currentTimeout + 1);
                // 获取目标元素并直接请求焦点
                auto targetItem = list->getItemAtIndex(currentTimeout + 1);
                if (targetItem != nullptr) {
                    this->requestFocus(targetItem, tsl::FocusDirection::None, false);
                }
                needsRefocus = false; // 标记已完成焦点设置
            }
        }
    }
    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override {
        return false;
    }
};



// 设置页面
class FTPSettingsGui : public tsl::Gui {
public:
    virtual tsl::elm::Element* createUI() override {
        auto frame = new tsl::elm::OverlayFrame("FTP设置", "配置FTP功能");
        auto list = new tsl::elm::List(); // 替换为标准List组件
        
        list->addItem(new tsl::elm::CategoryHeader("新手必读"));
        list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
            // 绘制警告文本，使用橙色作为警告颜色
            renderer->drawString("  设置完成后重启插件生效", false, x + 10, y + 20, 18, renderer->a({0xF, 0x8, 0x0, 0xF}));
        }), 30);

        auto funcIntroText = new tsl::elm::ListItem("功能介绍");
        funcIntroText->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                // 创建应用关于信息的多彩文本
                std::vector<TextSegment> coloredAbout = {
                    // 插件功能
                    {"匿名登录:", TextColors::CYAN, 20},
                    {"\n• ", TextColors::GRAY, 18},
                    {"开启后会与联机插件冲突", TextColors::RED, 18},
                    {"\n• ", TextColors::GRAY, 18},
                    {"开启后不需要账号密码即可连接", TextColors::WHITE, 18},

                    {"\n\n超时时间:", TextColors::CYAN, 20},
                    {"\n• ", TextColors::GRAY, 18},
                    {"超过这个时间自动断开客户端，单位秒", TextColors::WHITE, 18},

                    {"\n\n本地时区:", TextColors::CYAN, 20},
                    {"\n• ", TextColors::GRAY, 18},
                    {"XXXXXXXXXXXXXX", TextColors::WHITE, 18},

                    {"\n\n虚拟挂载:", TextColors::CYAN, 20},
                    {"\n• ", TextColors::GRAY, 18},
                    {"开启后会显示各种快捷入口", TextColors::WHITE, 18},

                    {"\n\nLED闪烁:", TextColors::CYAN, 20},
                    {"\n• ", TextColors::GRAY, 18},
                    {"开启后会在传输时闪烁LED", TextColors::WHITE, 18},

                    {"\n\n日志记录:", TextColors::CYAN, 20},
                    {"\n• ", TextColors::GRAY, 18},
                    {"开启后会记录所有操作", TextColors::WHITE, 18},
                };
                
                // 跳转到关于页面
                tsl::changeTo<TextDisplayGui>("功能介绍", "FTP相关功能", coloredAbout);
                return true;
            }
            return false;
        });
        list->addItem(funcIntroText);
        g_restartItem = new tsl::elm::ListItem("重启插件","");
        g_restartItem->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                // 重启系统模块
                Result rc = ModuleManager::getInstance().restartModule();
                if (R_SUCCEEDED(rc)) {
                    g_restartItem->setValue("重启完成");
                }
                else g_restartItem->setValue("重启失败");
                // 更新主界面状态
                g_statusItem->setValue(ModuleManager::getInstance().isRunning() ? "开" : "关");
                return true;
            }
            return false;
        });

        

        list->addItem(g_restartItem);

        list->addItem(new tsl::elm::CategoryHeader("FTP设置"));
    
        // 直接从配置文件读
        bool anonEnabled = ini_getbool("Login", "anon", 0, CONFIG_FILE_PATH);
        auto anonEnabledItem = new tsl::elm::ListItem("匿名登录", anonEnabled ? "开" : "关");
        anonEnabledItem->setClickListener([anonEnabledItem](u64 keys) {
            if (keys & HidNpadButton_A) {
                // 切换匿名登录状态
                bool new_anonEnabled = !ini_getbool("Login", "anon", 0, CONFIG_FILE_PATH);
                anonEnabledItem->setValue(new_anonEnabled ? "开" : "关");
                ini_putl("Login", "anon", new_anonEnabled ? 1 : 0, CONFIG_FILE_PATH);
                g_restartItem->setValue("需要重启");
                return true;
            }
            return false;
        });
        list->addItem(anonEnabledItem);

        char timeout[10];
        ini_gets("Network", "timeout", "0", timeout, sizeof(timeout), CONFIG_FILE_PATH);
        g_timeoutItem = new tsl::elm::ListItem("超时时间", timeout);
        g_timeoutItem->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                // 将当前超时时间转换为整数
                int currentTimeout = ini_getl("Network", "timeout", 0, CONFIG_FILE_PATH);
                // 确保数值在有效范围内
                tsl::changeTo<TimeoutGui>(currentTimeout);
                return true;
            }
            return false;
        });
        list->addItem(g_timeoutItem);
        
        bool use_localtimeEnabled = ini_getbool("Misc", "use_localtime", 0, CONFIG_FILE_PATH);
        auto use_localtimeEnabledItem = new tsl::elm::ListItem("本地时区", use_localtimeEnabled ? "开" : "关");
        use_localtimeEnabledItem->setClickListener([use_localtimeEnabledItem](u64 keys) {
            if (keys & HidNpadButton_A) {
                // 切换本地时区状态
                bool new_use_localtimeEnabled = !ini_getbool("Misc", "use_localtime", 0, CONFIG_FILE_PATH);
                use_localtimeEnabledItem->setValue(new_use_localtimeEnabled ? "开" : "关");
                ini_putl("Misc", "use_localtime", new_use_localtimeEnabled ? 1 : 0, CONFIG_FILE_PATH);
                g_restartItem->setValue("需要重启");
                return true;
            }
            return false;
        });
        list->addItem(use_localtimeEnabledItem);

        bool mount_devicesEnabled = ini_getbool("Nx", "mount_devices", 0, CONFIG_FILE_PATH);
        auto mount_devicesEnabledItem = new tsl::elm::ListItem("虚拟挂载", mount_devicesEnabled ? "开" : "关");
        mount_devicesEnabledItem->setClickListener([mount_devicesEnabledItem](u64 keys) {
            if (keys & HidNpadButton_A) {
                // 切换虚拟挂载状态
                bool new_mount_devicesEnabled = !ini_getbool("Nx", "mount_devices", 0, CONFIG_FILE_PATH);
                mount_devicesEnabledItem->setValue(new_mount_devicesEnabled ? "开" : "关");
                ini_putl("Nx", "mount_devices", new_mount_devicesEnabled ? 1 : 0, CONFIG_FILE_PATH);
                g_restartItem->setValue("需要重启");
                return true;
            }
            return false;
        });
        list->addItem(mount_devicesEnabledItem);

        
        bool ledEnabled = ini_getbool("Nx", "led", 0, CONFIG_FILE_PATH);
        auto ledEnabledItem = new tsl::elm::ListItem("LED闪烁", ledEnabled ? "开" : "关");
        ledEnabledItem->setClickListener([ledEnabledItem](u64 keys) {
            if (keys & HidNpadButton_A) {
                // 切换LED闪烁状态
                bool new_ledEnabled = !ini_getbool("Nx", "led", 0, CONFIG_FILE_PATH);
                ledEnabledItem->setValue(new_ledEnabled ? "开" : "关");
                ini_putl("Nx", "led", new_ledEnabled ? 1 : 0, CONFIG_FILE_PATH);
                g_restartItem->setValue("需要重启");
                return true;
            }
            return false;
        });
        list->addItem(ledEnabledItem);

        bool logEnabled = ini_getbool("Log", "log", 0, CONFIG_FILE_PATH);
        auto logEnabledItem = new tsl::elm::ListItem("日志记录", logEnabled ? "开" : "关");
        logEnabledItem->setClickListener([logEnabledItem](u64 keys) {
            if (keys & HidNpadButton_A) {
                // 切换日志记录状态
                bool new_logEnabled = !ini_getbool("Log", "log", 0, CONFIG_FILE_PATH);
                logEnabledItem->setValue(new_logEnabled ? "开" : "关");
                ini_putl("Log", "log", new_logEnabled ? 1 : 0, CONFIG_FILE_PATH);
                g_restartItem->setValue("需要重启");
                return true;
            }
            return false;
        });
        list->addItem(logEnabledItem);

        
        
        frame->setContent(list);
        
        return frame;
    }
    
    virtual void update() override { }
    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override {
        return false;
    }
};


// 备份数量选择界面
class BackupCountGui : public tsl::Gui {   
private:
    int currentCount; // 当前选中的数量
    tsl::elm::List* list; // 保存list指针用于延迟焦点设置
    bool needsRefocus; // 标记是否需要重新设置焦点
    int frameCounter; // 帧计数器，用于延迟几帧后设置焦点
    
public:
    BackupCountGui(int count) : currentCount(count), list(nullptr), needsRefocus(true), frameCounter(0) {}
    
    // 绘制界面
    virtual tsl::elm::Element* createUI() override {
        auto frame = new tsl::elm::OverlayFrame("备份数量", "选择备份存档数量");
        list = new tsl::elm::List(); // 保存list指针到成员变量
        list->addItem(new tsl::elm::CategoryHeader("0表示不限制数量（不建议）"));
        // 添加0-10的选项
        for (int i = 0; i <= 10; i++) {
            char num[3];
            sprintf(num, "%d", i);
            
            auto item = new tsl::elm::ListItem(num);
            // 设置当前数量等于i的项为选中状态
            if (i == currentCount) {
                item->setValue("\uE14B");
            }
            item->setClickListener([this, i, num](u64 keys) {
                if (keys & HidNpadButton_A) {

                    // 保存到配置文件
                    ini_puts("Backup", "maxback", num, CONFIG_FILE_PATH);
                    g_backupCountItem->setValue(num);
                    g_restartItem->setValue("需要重启");
                    tsl::goBack();
                    return true;
                }
                return false;
            });
            list->addItem(item);
        }
        
        frame->setContent(list);
        
        return frame;
    }
    
    virtual void update() override {
        // 延迟焦点重设逻辑：等待几帧后再设置焦点
        if (needsRefocus && list != nullptr) {
            frameCounter++;
            // 等待3帧后设置焦点，确保Tesla框架的初始化完成
            if (frameCounter >= 2) {
                // 设置焦点到正确的项目
                list->setFocusedIndex(currentCount + 1);
                // 获取目标元素并直接请求焦点
                auto targetItem = list->getItemAtIndex(currentCount + 1);
                if (targetItem != nullptr) {
                    this->requestFocus(targetItem, tsl::FocusDirection::None, false);
                }
                needsRefocus = false; // 标记已完成焦点设置
            }
        }
    }
    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override {
        return false;
    }
};



// 设置页面
class BackupSettingsGui : public tsl::Gui {
public:
    virtual tsl::elm::Element* createUI() override {
        auto frame = new tsl::elm::OverlayFrame("备份设置", "配置自动备份功能");
        auto list = new tsl::elm::List(); // 替换为标准List组件
        
        list->addItem(new tsl::elm::CategoryHeader("新手必读"));
        list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
            // 绘制警告文本，使用橙色作为警告颜色
            renderer->drawString("  设置完成后重启插件生效", false, x + 10, y + 20, 18, renderer->a({0xF, 0x8, 0x0, 0xF}));
        }), 30);
        auto funcIntroText = new tsl::elm::ListItem("功能介绍");
        funcIntroText->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                // 创建应用关于信息的多彩文本
                std::vector<TextSegment> coloredAbout = {
                    // 插件功能
                    {"备份存档:", TextColors::CYAN, 20},
                    {"\n• ", TextColors::GRAY, 18},
                    {"打开或者关闭插件自动备份存档的开关", TextColors::WHITE, 18},

                    {"\n\n存档名称:", TextColors::CYAN, 20},
                    {"\n• ", TextColors::GRAY, 18},
                    {"关闭后中文名称的存档会变成下划线", TextColors::WHITE, 18},

                    {"\n\n备份数量:", TextColors::CYAN, 20},
                    {"\n• ", TextColors::GRAY, 18},
                    {"设置网盘中存档备份的最大数量", TextColors::WHITE, 18},

                    {"\n\n自动上传:", TextColors::CYAN, 20},
                    {"\n• ", TextColors::GRAY, 18},
                    {"需保持备份功能为开启才生效", TextColors::RED, 18},
                    {"\n• ", TextColors::GRAY, 18},
                    {"用来打开关闭自动上传到网盘的开关", TextColors::WHITE, 18},

                    {"\n\n重启插件:", TextColors::CYAN, 20},
                    {"\n• ", TextColors::GRAY, 18},
                    {"修改设置后，需要重启插件生效", TextColors::WHITE, 18},
                };
                
                // 跳转到关于页面
                tsl::changeTo<TextDisplayGui>("功能介绍", "存档备份相关功能", coloredAbout);
                return true;
            }
            return false;
        });
        list->addItem(funcIntroText);
        g_restartItem = new tsl::elm::ListItem("重启插件","");
        g_restartItem->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                // 重启系统模块
                Result rc = ModuleManager::getInstance().restartModule();
                if (R_SUCCEEDED(rc)) {
                    g_restartItem->setValue("重启完成");
                }
                else g_restartItem->setValue("重启失败");
                // 更新主界面状态
                g_statusItem->setValue(ModuleManager::getInstance().isRunning() ? "开" : "关");
                return true;
            }
            return false;
        });
        list->addItem(g_restartItem);

        // 直接从配置文件读
        bool skip_ascii_convert = ini_getbool("Nx", "skip_ascii_convert", 0, CONFIG_FILE_PATH);
        bool WebDAV_enabled = ini_getbool("WebDAV", "enabled", 0, CONFIG_FILE_PATH);

        list->addItem(new tsl::elm::CategoryHeader("自动备份设置"));
        list->addItem(new tsl::elm::ListItem("备份存档", "--"));

        auto savenameItem = new tsl::elm::ListItem("存档名称", skip_ascii_convert ? "开" : "关");
        savenameItem->setClickListener([savenameItem](u64 keys) {
            if (keys & HidNpadButton_A) {
                // 切换存档名称显示方式
                bool new_skip_ascii_convert = !ini_getbool("Nx", "skip_ascii_convert", 0, CONFIG_FILE_PATH);
                savenameItem->setValue(new_skip_ascii_convert ? "开" : "关");
                ini_putl("Nx", "skip_ascii_convert", new_skip_ascii_convert ? 1 : 0, CONFIG_FILE_PATH);
                g_restartItem->setValue("需要重启");
                return true;
            }
            return false;
        });
        list->addItem(savenameItem);
        
        char maxback[10];
        ini_gets("Backup", "maxback", "0", maxback, sizeof(maxback), CONFIG_FILE_PATH);
        g_backupCountItem = new tsl::elm::ListItem("备份数量", maxback);
        g_backupCountItem->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                // 将当前备份数量转换为整数
                int currentCount = ini_getl("Backup", "maxback", 0, CONFIG_FILE_PATH);
                // 确保数值在有效范围内
                if (currentCount < 0 || currentCount > 10) currentCount = 0;
                tsl::changeTo<BackupCountGui>(currentCount);
                return true;
            }
            return false;
        });
        list->addItem(g_backupCountItem);

        auto WebDAV_enabledItem = new tsl::elm::ListItem("自动上传", WebDAV_enabled ? "开" : "关");
        WebDAV_enabledItem->setClickListener([WebDAV_enabledItem](u64 keys) {
            if (keys & HidNpadButton_A) {
                // 切换自动上传开关
                bool new_WebDAV_enabled = !ini_getbool("WebDAV", "enabled", 0, CONFIG_FILE_PATH);
                WebDAV_enabledItem->setValue(new_WebDAV_enabled ? "开" : "关");
                ini_putl("WebDAV", "enabled", new_WebDAV_enabled ? 1 : 0, CONFIG_FILE_PATH);
                g_restartItem->setValue("需要重启");
                return true;
            }
            return false;
        });
        list->addItem(WebDAV_enabledItem);
       
        frame->setContent(list);
        return frame;
    }
    
    virtual void update() override { }
    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override {
        return false;
    }
};


// 自定义密码显示组件
class PasswordItem : public tsl::elm::ListItem {
private:
    char m_password[32]; // 使用固定大小的字符数组存储密码
    bool m_showPassword;
    
public:
    PasswordItem(const char* title, const char* password) : 
        tsl::elm::ListItem(title, "******"), 
        m_showPassword(false) {
        // 安全地复制密码到内部缓冲区
        if (password) {
            // 使用更安全的方式复制字符串
            size_t len = strlen(password);
            if (len >= sizeof(m_password)) {
                len = sizeof(m_password) - 1; // 限制长度
            }
            memcpy(m_password, password, len);
            m_password[len] = '\0'; // 确保字符串结束
        } else {
            m_password[0] = '\0'; // 空字符串
        }
    }
    
    virtual bool onClick(u64 keys) override {
        return false;
    }
    
    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override {
        // 检测是否按住A键
        if (keysHeld & HidNpadButton_A) {
            if (!m_showPassword) {
                this->setValue(m_password); // 显示密码
                m_showPassword = true;
            }
        } else {
            if (m_showPassword) {
                this->setValue("******"); // 隐藏密码
                m_showPassword = false;
            }
        }
        
        return ListItem::handleInput(keysDown, keysHeld, touchPos, joyStickPosLeft, joyStickPosRight);
    }
};


// 网络设置页面
class NetworkSettingsGui : public tsl::Gui {
private:
    // IP地址获取函数
    const char* getCurrentIpAddress() {
        static char ipBuffer[16]; // 静态缓冲区，用于存储IP地址字符串
        u32 ipAddress = 0;
        
        // 使用nifm服务获取当前IP地址
        Result rc = nifmGetCurrentIpAddress(&ipAddress);
        
        if (R_SUCCEEDED(rc) && ipAddress != 0) {
            // 将u32格式的IP地址转换为字符串格式
            // IP地址以网络字节序存储，需要转换为主机字节序
            u8 a = (ipAddress >> 0) & 0xFF;
            u8 b = (ipAddress >> 8) & 0xFF;
            u8 c = (ipAddress >> 16) & 0xFF;
            u8 d = (ipAddress >> 24) & 0xFF;
            
            snprintf(ipBuffer, sizeof(ipBuffer), "%u.%u.%u.%u", a, b, c, d);
            return ipBuffer;
        }
        
        // 如果获取失败，返回默认值
        return "获取失败";
    }

public:
    virtual tsl::elm::Element* createUI() override {
        auto frame = new tsl::elm::OverlayFrame("FTP网络", "查看网络参数");
        auto list = new tsl::elm::List(); // 替换为标准List组件
        
        list->addItem(new tsl::elm::CategoryHeader("相关参数"));
        
        // 直接从配置文件读取网络设置参数
        char portBuffer[10], timeoutBuffer[10], userBuffer[20], passBuffer[20];
        ini_gets("Network", "port", "未设置", portBuffer, sizeof(portBuffer), CONFIG_FILE_PATH);
        ini_gets("Network", "timeout", "未设置", timeoutBuffer, sizeof(timeoutBuffer), CONFIG_FILE_PATH);
        ini_gets("Login", "user", "未设置", userBuffer, sizeof(userBuffer), CONFIG_FILE_PATH);
        ini_gets("Login", "pass", "未设置", passBuffer, sizeof(passBuffer), CONFIG_FILE_PATH);
        
        // 添加网络设置选项，显示从配置文件读取的实际值
        list->addItem(new tsl::elm::ListItem("IP地址", getCurrentIpAddress()));
        list->addItem(new tsl::elm::ListItem("服务端口", portBuffer));
        list->addItem(new tsl::elm::ListItem("超时时间", timeoutBuffer));
        list->addItem(new PasswordItem("用户账号", userBuffer));
        list->addItem(new PasswordItem("用户密码", passBuffer));
        list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
            // 绘制警告文本，使用橙色作为警告颜色
            renderer->drawString("  特斯拉插件无法使用键盘", false, x + 10, y + 30, 18, renderer->a({0xF, 0x8, 0x0, 0xF}));
        }), 30);
        list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
            // 绘制警告文本，使用橙色作为警告颜色
            renderer->drawString("  所以只能手动修改配置文件", false, x + 10, y + 30, 18, renderer->a({0xF, 0x8, 0x0, 0xF}));
        }), 30);
        list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
            // 绘制警告文本，使用橙色作为警告颜色
            renderer->drawString("  /config/FtpAutoBack/Confing.ini", false, x + 10, y + 30, 18, renderer->a({0xF, 0x8, 0x0, 0xF}));
        }), 30);
        
        frame->setContent(list);
        return frame;
    }
    
    virtual void update() override { }
    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override {
        return false;
    }
};


// 网盘地址与品牌名称的对照表
struct WebDavBrand {
    const char* url;
    const char* brand;
};

// 已知网盘服务列表
static const WebDavBrand WEBDAV_BRANDS[] = {
    {"https://dav.jianguoyun.com", "坚果云"},
    {nullptr, nullptr} // 结束标记
};


class WebDAVGui : public tsl::Gui {
private:
    // 根据URL识别网盘品牌
    const char* getBrandNameFromUrl(const char* url) {
        // 遍历对照表查找匹配项
        for (int i = 0; WEBDAV_BRANDS[i].url != nullptr; i++) {
            if (strstr(url, WEBDAV_BRANDS[i].url) != nullptr) {
                return WEBDAV_BRANDS[i].brand;
            }
        }
        
        // 没有匹配项
        return "未知";
    }
    
public:
    
    virtual tsl::elm::Element* createUI() override {
        auto frame = new tsl::elm::OverlayFrame("网盘网络", "查看相关参数");
        auto list = new tsl::elm::List(); // 替换为标准List组件
        
        list->addItem(new tsl::elm::CategoryHeader("相关参数"));
        
        // 直接从配置文件读取网络设置参数
        char origin[32], basepath[32], username[32], password[32];
        ini_gets("WebDAV", "origin", "未设置", origin, sizeof(origin), CONFIG_FILE_PATH);
        ini_gets("WebDAV", "basepath", "未设置", basepath, sizeof(basepath), CONFIG_FILE_PATH);
        ini_gets("WebDAV", "username", "未设置", username, sizeof(username), CONFIG_FILE_PATH);
        ini_gets("WebDAV", "password", "未设置", password, sizeof(password), CONFIG_FILE_PATH);
        
        // 添加网络设置选项，显示从配置文件读取的实际值
        list->addItem(new tsl::elm::ListItem("网盘品牌", getBrandNameFromUrl(origin)));
        list->addItem(new tsl::elm::ListItem("网盘路径", basepath));
        list->addItem(new PasswordItem("用户账号", username));
        list->addItem(new PasswordItem("用户密码", password)); // 使用自定义密码组件
        list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
            // 绘制警告文本，使用橙色作为警告颜色
            renderer->drawString("  特斯拉插件无法使用键盘", false, x + 10, y + 30, 18, renderer->a({0xF, 0x8, 0x0, 0xF}));
        }), 30);
        list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
            // 绘制警告文本，使用橙色作为警告颜色
            renderer->drawString("  所以只能手动修改配置文件", false, x + 10, y + 30, 18, renderer->a({0xF, 0x8, 0x0, 0xF}));
        }), 30);
        list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
            // 绘制警告文本，使用橙色作为警告颜色
            renderer->drawString("  /config/FtpAutoBack/Confing.ini", false, x + 10, y + 30, 18, renderer->a({0xF, 0x8, 0x0, 0xF}));
        }), 30);
        
        frame->setContent(list);
        return frame;
    }
    
    virtual void update() override { }
    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override {
        return false;
    }
};







// ===========================================
// 主界面类 - 简洁的菜单，直接跳转到对应页面
// ===========================================

class MainGui : public tsl::Gui {
public:
    MainGui() { }

    // 创建UI界面
    virtual tsl::elm::Element* createUI() override {
        // 创建主框架
        auto frame = new tsl::elm::OverlayFrame("FtpAutoBack", GLOBAL_APP_VERSION);
        
        // 创建循环导航列表
        auto list = new tsl::elm::List(); // 替换为标准List组件

        // 添加功能列表项 - 直接跳转到对应的GUI页面
        list->addItem(new tsl::elm::CategoryHeader("基础功能"));
        // 使用普通ListItem实现点击切换功能，保持颜色一致
        // 这是测试代码，用来测试关闭功能是否正常的。
        g_statusItem = new tsl::elm::ListItem("插件状态", ModuleManager::getInstance().isRunning() ? "开" : "关");
         g_statusItem->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                // 根据当前状态切换模块的开/关
                bool isRunning = ModuleManager::getInstance().isRunning();
                
                if (isRunning) {
                    // 如果模块正在运行，则停止它
                    Result rc = ModuleManager::getInstance().stopModule();
                    if (R_SUCCEEDED(rc)) {
                        g_statusItem->setValue("关");
                    }
                } else {
                    // 如果模块已停止，则启动它
                    Result rc = ModuleManager::getInstance().startModule();
                    if (R_SUCCEEDED(rc)) {
                        g_statusItem->setValue("开");
                    }
                }
                
                return true;
            }
            return false;
        });
        list->addItem(g_statusItem);
        
        
        auto aboutItem = new tsl::elm::ListItem("关于应用");
        aboutItem->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                // 创建应用关于信息的多彩文本
                std::vector<TextSegment> coloredAbout = {
                    // 应用标题
                    {"FtpAutoBack  ", TextColors::CYAN, 24},
                    {GLOBAL_APP_VERSION, TextColors::CYAN, 16},
                    {"\nNintendo Switch FTP和存档备份工具", TextColors::WHITE, 18},
                    
                    // 版本信息
                    {"\n\n作者:", TextColors::CYAN, 20},
                    {"\n• ", TextColors::GRAY, 18},
                    {"TOM", TextColors::WHITE, 18},

                    // 插件功能
                    {"\n\n功能介绍:", TextColors::CYAN, 20},
                    {"\n• ", TextColors::GRAY, 18},
                    {"调整相关设置", TextColors::WHITE, 18},
                    {"\n• ", TextColors::GRAY, 18},
                    {"便捷启停插件", TextColors::WHITE, 18},
                    
                    // 技术信息
                    {"\n\n技术栈:", TextColors::CYAN, 20},
                    {"\n• ", TextColors::GRAY, 18},
                    {"C++20", TextColors::GREEN, 18},
                    {" 核心开发语言", TextColors::WHITE, 18},
                    {"\n• ", TextColors::GRAY, 18},
                    {"Tesla", TextColors::GREEN, 18},
                    {" 界面框架", TextColors::WHITE, 18},
                    {"\n• ", TextColors::GRAY, 18},
                    {"libnx", TextColors::GREEN, 18},
                    {" Switch系统库", TextColors::WHITE, 18},
                    
                    // 版权信息
                    {"\n\n© 2025 TOM", TextColors::GRAY, 16},
                    {"\n开源项目 - MIT许可证", TextColors::GRAY, 16}
                };
                
                // 跳转到关于页面
                tsl::changeTo<TextDisplayGui>("关于应用", GLOBAL_APP_VERSION, coloredAbout);
                return true;
            }
            return false;
        });
        list->addItem(aboutItem);

        list->addItem(new tsl::elm::CategoryHeader("FTP功能"));
        auto networkItem = new tsl::elm::ListItem("FTP网络");
        networkItem->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                tsl::changeTo<NetworkSettingsGui>(); // 直接跳转到网络设置页面
                return true;
            }
            return false;
        });
        list->addItem(networkItem);

        auto ftpItem = new tsl::elm::ListItem("FTP设置");
        ftpItem->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                tsl::changeTo<FTPSettingsGui>(); // 直接跳转到设置页面
                return true;
            }
            return false;
        });
        list->addItem(ftpItem);
        

        list->addItem(new tsl::elm::CategoryHeader("备份功能"));
        auto WebDAVItem = new tsl::elm::ListItem("网盘网络");
        WebDAVItem->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                tsl::changeTo<WebDAVGui>(); 
                return true;
            }
            return false;
        });
        list->addItem(WebDAVItem);
        auto backupItem = new tsl::elm::ListItem("备份设置");
        backupItem->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                tsl::changeTo<BackupSettingsGui>(); // 直接跳转到备份设置页面
                return true;
            }
            return false;
        });
        list->addItem(backupItem);

        // 设置内容并返回
        frame->setContent(list);
        return frame;
    }

    virtual void update() override { }
    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override {
        return false;
    }
};

class OverlayTest : public tsl::Overlay {
public:
                                             // libtesla 已经初始化了 fs, hid, pl, pmdmnt, hid:sys 和 set:sys
    virtual void initServices() override {   // 在开始时调用以初始化此覆盖层所需的所有服务
        // 初始化nifm服务用于获取IP地址
        nifmInitialize(NifmServiceType_User);
        // 初始化pmshell服务用于管理模块
        pmshellInitialize();
    }
    virtual void exitServices() override {   // 在结束时调用以清理之前初始化的所有服务
        // 清理pmshell服务
        pmshellExit();
        // 清理nifm服务
        nifmExit();
    }

    virtual void onShow() override {}    // 在覆盖层想要从不可见状态变为可见状态之前调用
    virtual void onHide() override {}    // 在覆盖层想要从可见状态变为不可见状态之前调用

    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<MainGui>();  // 要加载的初始 Gui。可以像这样向其构造函数传递参数
    }
};

int main(int argc, char **argv) {
    return tsl::loop<OverlayTest>(argc, argv);
}
