#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <vector>

namespace osp
{

class UserInputHandler;

enum class ButtonEvent
{
    PRESSED,
    RELEASED
};

class ButtonHandle
{
public:
    ButtonHandle(UserInputHandler *to, int buttonIndex) :
            m_to(to), m_buttonIndex(buttonIndex) {};
    bool single_press();
    bool single_release();
    bool is_pressed();

private:
    UserInputHandler *m_to;
    int m_buttonIndex;
};

struct ButtonInput
{
    bool m_down;
    uint8_t m_reference_count;
};

struct ButtonTerm
{

    uint16_t m_buttonIndex;

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
    uint8_t m_bits;
};


struct ButtonTermConfig
{
    enum class TermTrigger { HOLD, PRESSED };
    enum class TermOperator { OR, AND };

    ButtonTermConfig(int device, int devEnum, TermTrigger trigger,
                     bool invert, TermOperator nextOp);

    int m_device;
    int m_devEnum;
    uint8_t m_bits;
};

struct ButtonControl
{
    uint16_t reference_count;

    // hold is true if just all the hold conditions are true.
    // ignore the press/releases

    bool m_detectHold;
    bool m_active, m_activeHold;
    std::array<ButtonTerm, 10> m_conditions;
};

// [buttons that need to be held down]

class UserInputHandler
{
public:
    UserInputHandler(int deviceCount);

    /**
     * Register a new control
     * @param name
     * @param terms
     */
    void config_register_control(std::string const& name,
                                 ButtonTermConfig terms...);

    ButtonHandle config_get(std::string const& name);

    void event_clear();
    void event_raw(int deviceId, int buttonEnum, ButtonEvent dir);
    void update_controls();

private:
    std::vector<ButtonInput> m_buttons;
    std::vector<std::map<int, int>> m_devices;
};

}
