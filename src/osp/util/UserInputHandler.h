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

#include "../core/copymove_macros.h"
#include "../core/math_types.h"

#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace osp::input
{

enum class EButtonEvent : uint8_t { Pressed = 0, Released = 1 };

enum class EButtonControlEvent : uint8_t { Triggered = 0, Released = 1 };

enum class EButtonControlIndex : uint32_t
{
    NONE = std::numeric_limits<uint32_t>::max()
};

//-----------------------------------------------------------------------------

/**
 * @brief Raw button state mapped directly from a device enum
 */
struct ButtonRaw
{
    bool m_pressed;
    bool m_justPressed;
    bool m_justReleased;
    uint8_t m_referenceCount;
};

using ButtonMap_t = std::map<int, ButtonRaw, std::less<>>;

/**
 * @brief Mouse motion state
 */
struct MouseMotion
{
    Vector2i m_rawDelta;
    Vector2 m_smoothDelta{ 0.0f };

    /*
     * Mouse responsiveness - float between (0.0f, 1.0f]
     * larger numbers -> less smooth, smaller numbers -> more floaty
     * Recommend leaving this around 0.5
    */
    float m_responseFactor{ 0.5f };

    uint8_t m_referenceCount;
};

/**
 * @brief Raw scroll state
 */
struct ScrollRaw
{
    Vector2i offset;

    uint8_t m_referenceCount;
};

//-----------------------------------------------------------------------------

enum class EVarTrigger : uint8_t { Hold = 0, Pressed = 1 };
enum class EVarOperator : uint8_t { Or = 0, And = 1 };

/**
 * @brief An invidual term in a boolean expression representing a (raw button)
 */
struct ControlTerm
{
    ButtonMap_t::iterator m_button;
    EVarTrigger m_trigger;
    EVarOperator m_nextOp;
    bool m_invert;
};

/**
 * @brief The ButtonVarConfig struct
 */
struct ControlTermConfig
{

    ControlTerm create(ButtonMap_t::iterator button) const
    {
        return ControlTerm{button, m_trigger, m_nextOp, m_invert};
    }

    int m_device;
    int m_devEnum;
    EVarTrigger m_trigger;
    EVarOperator m_nextOp;
    bool m_invert;
};

/*
 * Control expressions describes a boolean expression of conditions that can be
 * evaluated to check if a control has been triggered.
 *
 * ie: undo and redo
 * undo = (Ctrl Held) AND (Z Pressed) AND (NOT Shift Held)
 * redo = (Ctrl Held) AND (Z Pressed) AND (Shift Held)
 */
using ControlExpr_t = std::vector<ControlTerm>;
using ControlExprConfig_t = std::vector<ControlTermConfig>;

//-----------------------------------------------------------------------------

/**
 * @brief Active Button control
 */
struct ButtonControl
{
    uint16_t m_referenceCount{1};

    // hold is true if just all the hold conditions are true.
    // ignore the press/releases

    bool m_held{false};
    bool m_holdable{false};
    bool m_triggered{false};

    ControlExpr_t m_exprPress{};
    ControlExpr_t m_exprRelease{};
};

struct ButtonControlConfig
{
    std::vector<ControlTermConfig> m_press;
    bool m_holdable;
    bool m_enabled;
    int m_index;
};

//-----------------------------------------------------------------------------

struct ButtonControlEvent
{
// This constructor should only be needed if aggregate initialization via
// parenthesis isn't supported by the compiler.
#if ! defined( __cpp_aggregate_paren_init )
    constexpr ButtonControlEvent(EButtonControlIndex index,
                                 EButtonControlEvent event) noexcept
     : m_index(index)
     , m_event(event)
    { }
#endif // ! defined( __cpp_aggregate_paren_init )

    EButtonControlIndex m_index;
    EButtonControlEvent m_event;
};

//-----------------------------------------------------------------------------

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

public:

    using DeviceId = uint32_t;

    UserInputHandler(size_t deviceCount);

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
            ControlExpr_t const& expression,
            ControlExpr_t* pReleaseExpr = nullptr);

    /**
     * Register a new control into the config.
     * @param name Name used for identification
     * @param vars Expression needed to activate the control
     */
    void config_register_control(std::string const& name,
        bool holdable,
        std::vector<ControlTermConfig> vars);

    void config_register_control(std::string&& name,
        bool holdable,
        std::vector<ControlTermConfig> vars);

    EButtonControlIndex button_subscribe(std::string_view name);

    void button_unsubscribe(EButtonControlIndex index);

    ButtonControl const& button_state(EButtonControlIndex index) const
    {
        return m_btnControls.at(size_t(index));
    }

    constexpr MouseMotion const& mouse_state() const { return m_mouseMotion; }

    constexpr ScrollRaw const& scroll_state() const  { return m_scrollOffset; }

    constexpr std::vector<ButtonControlEvent> const& button_events() const noexcept
    {
        return m_btnControlEvents;
    }

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
    void event_raw(DeviceId deviceId, int buttonEnum, EButtonEvent dir);

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

    std::vector<ButtonMap_t> m_deviceToButtonRaw;
    std::map<std::string, ButtonControlConfig, std::less<> > m_btnControlCfg;

    // Mouse inputs
    MouseMotion m_mouseMotion;
    ScrollRaw m_scrollOffset;

    std::vector<ButtonMap_t::iterator> m_btnPressed;
    std::vector<ButtonMap_t::iterator> m_btnReleased;

    // Currently active controls being listened to
    std::vector<ButtonControl> m_btnControls;

    std::vector<ButtonControlEvent> m_btnControlEvents;

}; // class UserInputHandler

// temporary-ish: fixed device IDs for keyboard and mouse
const UserInputHandler::DeviceId sc_keyboard = 0;
const UserInputHandler::DeviceId sc_mouse = 1;

//-----------------------------------------------------------------------------

class ControlSubscriber
{
public:
    ControlSubscriber() = default;
    ControlSubscriber(UserInputHandler *pInputHandler) noexcept
     : m_pInputHandler(pInputHandler)
    { }

    OSP_MOVE_ONLY_CTOR_ASSIGN(ControlSubscriber);

    ~ControlSubscriber();

    EButtonControlIndex button_subscribe(std::string_view name);

    void unsubscribe();

    bool button_triggered(EButtonControlIndex index) const
    {
        return m_pInputHandler->button_state(index).m_triggered;
    }

    bool button_held(EButtonControlIndex index) const
    {
        return m_pInputHandler->button_state(index).m_held;
    }

    constexpr std::vector<ButtonControlEvent> const& button_events()
    {
        return m_pInputHandler->button_events();
    }

    UserInputHandler* get_input_handler() noexcept { return m_pInputHandler; };
    UserInputHandler const* get_input_handler() const noexcept
    { return m_pInputHandler; };

private:
    UserInputHandler *m_pInputHandler{nullptr};
    std::vector<EButtonControlIndex> m_subscribedButtons;
};

}
