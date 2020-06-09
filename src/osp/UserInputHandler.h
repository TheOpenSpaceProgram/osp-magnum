#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace osp
{

class UserInputHandler;
struct ButtonRaw;

using ButtonMap = std::map<int, ButtonRaw>;

class ButtonControlHandle
{
public:
    ButtonControlHandle() : m_to(nullptr), m_index(0) {}
    ButtonControlHandle(UserInputHandler *to, int index);
    ~ButtonControlHandle();

    bool triggered();
    bool trigger_hold();

private:
    UserInputHandler *m_to;
    //ButtonMap::iterator m_button;
    int m_index;
    
};

struct ButtonRaw
{
    bool m_pressed, m_justPressed, m_justReleased;
    uint8_t m_reference_count;
};

struct ButtonTermConfig;

struct ButtonTerm
{

    enum class TermTrigger { HOLD = 0, PRESSED = 1 };
    enum class TermOperator { OR = 0, AND = 1 };

    ButtonTerm(ButtonMap::iterator button, TermTrigger trigger,
               bool invert, TermOperator nextOp);

    ButtonTerm(ButtonMap::iterator button, ButtonTermConfig cfg);

    //        m_button(button), m_bits(bits) {}

    // Bit 0: Trigger Mode
    //   0. Always true when button is held
    //   1. True only when button had just been pressed
    // Bit 1: Invert
    //   0. Detect hold down or pressing down
    //   1. Invert, detect button up, or unpress instead
    // Bit 2: Operator
    //   0. OR with next condition
    //   1. AND with next condition
    // Bit 3: Terminate?
    //   0. Last condition, terminate
    //   1. Next condition exists
    //uint8_t m_bits;

    ButtonMap::iterator m_button;
    TermTrigger m_trigger;
    bool m_invert;
    TermOperator m_nextOp;
};

struct ButtonTermConfig
{
    using TermTrigger = ButtonTerm::TermTrigger;
    using TermOperator = ButtonTerm::TermOperator;

    constexpr ButtonTermConfig(int device, int devEnum, TermTrigger trigger,
                               bool invert, TermOperator nextOp) :
            m_device(device),
            m_devEnum(devEnum),
            m_trigger(trigger),
            m_invert(invert),
            m_nextOp(nextOp) {}

    int m_device;
    int m_devEnum;
    TermTrigger m_trigger;
    bool m_invert;
    TermOperator m_nextOp;
    //uint8_t m_bits;
};

struct ButtonConfig
{
    std::vector<ButtonTermConfig> m_terms;
    bool m_enabled;
    int m_index;
};

struct ButtonControl
{
    uint16_t m_reference_count;

    // hold is true if just all the hold conditions are true.
    // ignore the press/releases

    bool m_detectHold;
    bool m_triggered, m_triggerHold;
    //std::array<ButtonTerm, 10> m_terms;
    std::vector<ButtonTerm> m_terms;
};

class UserInputHandler
{
    friend ButtonControlHandle;

public:

    typedef int DeviceId;

    enum class ButtonRawEvent
    {
        PRESSED,
        RELEASED
    };


    UserInputHandler(int deviceCount);

    // TODO: deal with joystick 1D and 2D axis
    // axis should have different modes:
    // * Absolute: [mouse position, slider, touch screen, etc..]
    // * Relative: something directional [joystick, mouse movement, etc...]
    // TODO: think about a multitouch interface

    /**
     * Register a new control into the config.
     * @param name Name used for identification
     * @param terms Expression needed to activate the control
     */
    void config_register_control(std::string const& name,
            std::initializer_list<ButtonTermConfig> terms);

    /**
     *
     * @return ButtonHandle that can be used to read a ButtonControl
     */
    ButtonControlHandle config_get(std::string const& name);

    /**
     *
     */
    void event_clear();

    /**
     *
     * @param deviceId
     * @param buttonEnum
     * @param dir
     */
    void event_raw(DeviceId deviceId, int buttonEnum, ButtonRawEvent dir);

    /**
     *
     */
    void update_controls();

private:

    std::vector<ButtonMap> m_deviceToButtonRaw;
    std::map<std::string, ButtonConfig> m_controlConfigs;

    //std::vector<ButtonRaw> m_buttons;
    std::vector<ButtonControl> m_controls;

    std::vector<ButtonMap::iterator> m_btnPressed;
    std::vector<ButtonMap::iterator> m_btnReleased;

    //std::map<std::string, int> m_controlActive;
};

}
