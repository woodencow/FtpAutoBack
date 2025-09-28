#define TESLA_INIT_IMPL // 如果有多个文件使用tesla头文件，只在主文件中定义此宏
#include <tesla.hpp>    // Tesla头文件


class GuiSecondary : public tsl::Gui {
public:
    GuiSecondary() {}

    virtual tsl::elm::Element* createUI() override {
        auto *rootFrame = new tsl::elm::OverlayFrame("Tesla Example", "v1.3.2 - Secondary Gui");

        rootFrame->setContent(new tsl::elm::DebugRectangle(tsl::Color{ 0x8, 0x3, 0x8, 0xF }));

        return rootFrame;
    }
};

class GuiTest : public tsl::Gui {
public:
    GuiTest(u8 arg1, u8 arg2, bool arg3) { }

    // 当此GUI被加载时调用以创建UI
    // 在堆上分配所有元素。libtesla会确保在不需要时清理它们
    virtual tsl::elm::Element* createUI() override {
        // OverlayFrame是每个覆盖层的基础元素。它会绘制默认的标题和副标题。
        // 如果需要在标题中显示更多信息或改变外观，请使用HeaderOverlayFrame。
        auto frame = new tsl::elm::OverlayFrame("Tesla Example", "v1.3.2");

        // 可以包含子元素并处理滚动的列表
        auto list = new tsl::elm::List();

        // 列表项
        list->addItem(new tsl::elm::CategoryHeader("List items"));

        auto *clickableListItem = new tsl::elm::ListItem("Clickable List Item", "...");
        clickableListItem->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                tsl::changeTo<GuiSecondary>();
                return true;
            }

            return false;
        });

        list->addItem(clickableListItem);
        list->addItem(new tsl::elm::ListItem("Default List Item"));
        list->addItem(new tsl::elm::ListItem("Default List Item with an extra long name to trigger truncation and scrolling"));
        list->addItem(new tsl::elm::ToggleListItem("Toggle List Item", true));

        // 自定义绘制器，一个可以直接访问渲染器的元素
        list->addItem(new tsl::elm::CategoryHeader("Custom Drawer", true));
        list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawCircle(x + 40, y + 40, 20, true, renderer->a(0xF00F));
            renderer->drawCircle(x + 50, y + 50, 20, true, renderer->a(0xF0F0));
            renderer->drawRect(x + 130, y + 30, 60, 40, renderer->a(0xFF00));
            renderer->drawString("Hello :)", false, x + 250, y + 70, 20, renderer->a(0xFF0F));
            renderer->drawRect(x + 40, y + 90, 300, 10, renderer->a(0xF0FF));
        }), 100);

        // 轨道条
        list->addItem(new tsl::elm::CategoryHeader("Track bars"));
        list->addItem(new tsl::elm::TrackBar("\u2600"));
        list->addItem(new tsl::elm::StepTrackBar("\uE13C", 20));
        list->addItem(new tsl::elm::NamedStepTrackBar("\uE132", { "Selection 1", "Selection 2", "Selection 3" }));

        // 将列表添加到框架中以便绘制
        frame->setContent(list);

        // 返回框架使其成为此GUI的顶级元素
        return frame;
    }

    // 每帧调用一次以更新数值
    virtual void update() override {

    }

    // 每帧调用一次以处理其他UI元素未处理的输入
    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override {
        return false;   // 在此返回true表示输入已被消费
    }
};

class OverlayTest : public tsl::Overlay {
public:
                                             // libtesla已经初始化了fs, hid, pl, pmdmnt, hid:sys和set:sys
    virtual void initServices() override {}  // 在开始时调用以初始化此覆盖层所需的所有服务
    virtual void exitServices() override {}  // 在结束时调用以清理之前初始化的所有服务

    virtual void onShow() override {}    // 在覆盖层要从不可见状态变为可见状态之前调用
    virtual void onHide() override {}    // 在覆盖层要从可见状态变为不可见状态之前调用

    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<GuiTest>(1, 2, true);  // 要加载的初始GUI。可以像这样向其构造函数传递参数
    }
};

int main(int argc, char **argv) {
    return tsl::loop<OverlayTest>(argc, argv);
}
