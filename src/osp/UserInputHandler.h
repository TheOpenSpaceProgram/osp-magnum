/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <optional>
#include "types.h"

namespace osp
{

class UserInputHandler;

struct ButtonVar;
struct ButtonVarConfig;
struct ButtonRaw;

using ButtonMap = std::map<int, ButtonRaw>;
using ButtonExpr = std::vector<ButtonVar>;

struct ButtonRaw
{
    bool m_pressed, m_justPressed, m_justReleased;
    uint8_t m_referenceCount;
};

struct ButtonVar
{
    enum class VarTrigger { HOLD = 0, PRESSED = 1 };
    enum class VarOperator { OR = 0, AND = 1 };

    ButtonVar(ButtonMap::iterator button, VarTrigger trigger,
               bool invert, VarOperator nextOp);

    ButtonVar(ButtonMap::iterator button, ButtonVarConfig cfg);

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
    // Bit 3: Varinate?
    //   0. Last condition, varinate
    //   1. Next condition exists
    //uint8_t m_bits;

    ButtonMap::iterator m_button;
    VarTrigger m_trigger;
    bool m_invert;
    VarOperator m_nextOp;
};

struct ButtonVarConfig
{
    using VarTrigger = ButtonVar::VarTrigger;
    using VarOperator = ButtonVar::VarOperator;

    constexpr ButtonVarConfig(int device, int devEnum, VarTrigger trigger,
                               bool invert, VarOperator nextOp) :
            m_device(device),
            m_devEnum(devEnum),
            m_trigger(trigger),
            m_invert(invert),
            m_nextOp(nextOp) {}

    int m_device;
    int m_devEnum;
    VarTrigger m_trigger;
    bool m_invert;
    VarOperator m_nextOp;
    //uint8_t m_bits;
};

struct ButtonConfig
{
    std::vector<ButtonVarConfig> m_press;
    bool m_holdable;
    bool m_enabled;
    int m_index;
};

// Primary owner of button state, one per button
struct ButtonControl
{
    uint16_t m_referenceCount;

    // hold is true if just all the hold conditions are true.
    // ignore the press/releases

    bool m_holdable;
    bool m_triggered, m_held;
    //std::array<ButtonVar, 10> m_vars;
    ButtonExpr m_exprPress;
    ButtonExpr m_exprRelease;
};

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

struct MouseMotion
{
public:
    Vector2i m_rawDelta;

    /* 
     * Mouse responsiveness - float between (0.0f, 1.0f]
     * larger numbers -> less smooth, smaller numbers -> more floaty
     * Recommend leaving this around 0.5
    */
    float m_responseFactor{ 0.5f };
    
    Vector2 m_smoothDelta{ 0.0f };
    uint8_t m_referenceCount;
};

class MouseMovementHandle
{
public:
    MouseMovementHandle() : m_to(nullptr) {}
    MouseMovementHandle(UserInputHandler *to);
    ~MouseMovementHandle();
    
    float dxSmooth() const;
    float dySmooth() const;
    int dxRaw() const;
    int dyRaw() const;
private:
    UserInputHandler *m_to;
};

struct ScrollRaw
{
    Vector2i offset;

    uint8_t m_referenceCount;
};

class ScrollInputHandle
{
public:
    ScrollInputHandle() : m_to(nullptr) {}
    ScrollInputHandle(UserInputHandler *to);
    ~ScrollInputHandle();

    int dx() const;
    int dy() const;

private:
    UserInputHandler *m_to;
};

/**
 * Roughly based on HotkeyHandler.as from urho-osp (but more sane),
 * UserInputHandler is intended to unite buttons and axis across all input
 * devices. It can be configured to trigger on various different combinations
 * of button pressess across devices like Ctrl+Click, and bind multiple
 * buttons to a single action.
 *
 *
 * For now, only button controls are implemented.
 *
 *
 * To use, register controls beforehand by assigning "Button Expressions" to
 * string identifiers, using config_register_control.
 *
 *
 * TODO: code example goes here
 *
 *
 * This adds configs to a map. ie:
 * * "move_up"    -> (keyboard W Pressed) OR (keyboard ArrowUp Pressed)
 * * "move_down"  -> (keyboard S Pressed) OR (keyboard ArrowDown Pressed)
 * * "copy"       -> (keyboard Ctrl Held) AND (keyboard C Pressed)
 * * "paste"      -> (keyboard Ctrl Held) AND (keyboard V Pressed)
 *
 *
 * Somewhere else in the program, where the control is needed, create a
 * ButtonControlHandle using config_get. It's useful to make this a member
 * variable. These handles tell the UserInputHandler that a control is needed,
 * and does reference counting on construction and destruction. This makes sure
 * that the UserInputHandler knows which buttons to listen to, and which
 * controls to evaluate each frame. The ButtonControlHandle can now be used
 * to check if the control is triggered.
 *
 *
 * code example goes here
 *
 *
 * Finally, the raw input from devices goes into event_raw.
 *
 *
 * code example goes here
 *
 *
 * TODO: holdable controls
 *
 *
 *
 * Not sure if this is overengineered, but the flexibility comes from using
 * a bit of boolean algebra, where variables are button pressed/released, or
 * hold/notheld
 *
 * more info here later
 *
 */
class UserInputHandler
{
    friend ButtonControlHandle;
    friend MouseMovementHandle;
    friend ScrollInputHandle;

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
     * Iterate through the vars of a Button Expression to evaluate it.
     *
     * If a Release Expression is provided (non-nullptr), then individual terms
     * that evaluated
     *
     * @param expression [in] Expression to evaluate
     * @param pReleaseExpr [out] optional Release Expression to generate
     * @return result of expression
     */
    static bool eval_button_expression(
            ButtonExpr const& expression,
            ButtonExpr* pReleaseExpr = nullptr);

    /**
     * Register a new control into the config.
     * @param name Name used for identification
     * @param vars Expression needed to activate the control
     */
    void config_register_control(std::string const& name,
        bool holdable,
        std::vector<ButtonVarConfig> vars);

    void config_register_control(std::string&& name,
        bool holdable,
        std::vector<ButtonVarConfig> vars);


    /**
     * Fetch a button configuration
     * 
     * Spawns a new ButtonControl to allow 
     * 
     * @return ButtonHandle that can be used to read a ButtonControl
     */
    ButtonControlHandle config_get(std::string const& name);

    MouseMovementHandle mouse_get();

    ScrollInputHandle scroll_get();

    /**
     * Resets per-frame control properties, like button "just pressed" states
     * and mouse motion deltas
     */
    void clear_events();

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

    /*
     * Update this frame's mouse motion (position delta)
     * @param delta Change in mouse position this frame
    */
    void mouse_delta(Vector2i delta);

    /*
     * Update this frame's scroll offset
     * @param delta Change in mouse position this frame
    */
    void scroll_delta(Vector2i offset);

private:

    std::vector<ButtonMap> m_deviceToButtonRaw;
    std::map<std::string, ButtonConfig> m_controlConfigs;
    std::vector<ButtonControl> m_controls;

    // Mouse inputs
    MouseMotion m_mouseMotion;
    ScrollRaw m_scrollOffset;

    std::vector<ButtonMap::iterator> m_btnPressed;
    std::vector<ButtonMap::iterator> m_btnReleased;

    //std::map<std::string, int> m_controlActive;
};

// temporary-ish
const UserInputHandler::DeviceId sc_keyboard = 0;
const UserInputHandler::DeviceId sc_mouse = 1;
}
