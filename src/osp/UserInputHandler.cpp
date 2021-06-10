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
#include "UserInputHandler.h"
#include "string_concat.h"

#include <iostream>

namespace osp::input
{

UserInputHandler::UserInputHandler(int deviceCount) :
    m_deviceToButtonRaw(deviceCount)
{
    p_logger = spdlog::get("userinput");
}

bool UserInputHandler::eval_button_expression(
        ButtonExpr_t const& rExpr,
        ButtonExpr_t* pReleaseExpr)
{

    bool totalOn = false;
    bool termOn = false;

    VarOperator prevOp = VarOperator::OR;
    auto termStart = rExpr.begin();

    //for (ButtonVar const& var : rExpression)
    for (auto it =  rExpr.begin(); it != rExpr.end(); it ++)
    {
        ButtonVar const& var = *it;
        ButtonRaw const& btnRaw = var.m_button->second;

        bool varOn;

        // Get the value the ButtonVar specifies
        switch(var.m_trigger)
        {
        case VarTrigger::PRESSED:
            if (var.m_invert)
            {
                varOn = btnRaw.m_justReleased;
            }
            else
            {
                varOn = btnRaw.m_justPressed;
            }

            break;
        case VarTrigger::HOLD:
            // "boolIn != bool" is a conditional invert
            // 1 != 1 = 0
            // 0 != 1 = 1
            // 0 != 0 = 0
            // 1 != 0 = 1

            varOn = (btnRaw.m_pressed != var.m_invert);
            break;
        }

        bool lastVar = ((it + 1) == rExpr.end());

        // Deal with operations on that value
        switch (prevOp)
        {
        case VarOperator::OR:
            // Current var is the start of a new term.

            // Add the previous term to the total
            totalOn = totalOn || termOn;
            termOn = varOn;
            lastVar = true;

            break;
        case VarOperator::AND:
            termOn = termOn && varOn;
            break;
        }

        if (prevOp == VarOperator::OR || lastVar)
        {
            // if the previous term contributes to the expression being true
            // and pReleaseExpr exists
            if (termOn && pReleaseExpr)
            {

                auto termEnd = lastVar ? rExpr.end() : it;

                // loop through the previous term, and add inverted
                // just-pressed buttons to trigger the release
                for (auto itB = termStart; itB != termEnd; itB ++)
                {
                    if (itB->m_trigger != VarTrigger::PRESSED)
                    {
                        continue;
                    }
                    pReleaseExpr->emplace_back(
                            itB->m_button, VarTrigger::PRESSED,
                            !(itB->m_invert), VarOperator::OR);
                }
            }

            termStart = it;
        }

        prevOp = var.m_nextOp;

    }

    // consider the ver last term
    totalOn = totalOn || termOn;

    return totalOn;
}

void UserInputHandler::config_register_control(std::string const& name, bool holdable, std::vector<ButtonVarConfig> vars)
{
    m_btnControlCfg.insert_or_assign(name, ButtonConfig{ std::move(vars), holdable, false, 0 });
}
void UserInputHandler::config_register_control(std::string&& name, bool holdable, std::vector<ButtonVarConfig> vars)
{
    m_btnControlCfg.insert_or_assign(std::move(name), ButtonConfig{ std::move(vars), holdable, false, 0 });
}

ButtonControlIndex UserInputHandler::button_subscribe(std::string_view name)
{

    // check if a config exists for the name given
    auto cfgIt = m_btnControlCfg.find(name);

    if (cfgIt == m_btnControlCfg.end())
    {
        // Config not found, no way to have an empty key so far, so throw an exception
        SPDLOG_LOGGER_ERROR(p_logger, "No config for{}", name);
        throw std::runtime_error(string_concat("Error: no config with ", name));
    }

    // Check if the control was already created before
    if (cfgIt->second.m_enabled)
    {
        // Use existing ButtonControl
        size_t index = cfgIt->second.m_index;
        m_btnControls[index].m_referenceCount ++;
        return ButtonControlIndex(index);
    }
    else
    {
        // Create a new ButtonControl

        std::vector<ButtonVarConfig> const &varConfigs = cfgIt->second.m_press;
        ButtonControl &rControl = m_btnControls.emplace_back();

        rControl.m_holdable = (cfgIt->second.m_holdable);
        rControl.m_held = false;

        rControl.m_exprPress.reserve(varConfigs.size());

        for (ButtonVarConfig const &varCfg : varConfigs)
        {
            // Each loop, create a ButtonVar from the ButtonConfig and emplace
            // it into control.m_vars

            // Map of buttons for the specified device
            ButtonMap_t &btnMap = m_deviceToButtonRaw[varCfg.m_device];

            // try inserting a new button index
            auto btnInsert = btnMap.insert(std::make_pair(varCfg.m_devEnum,
                                                          ButtonRaw()));
            ButtonRaw &btnRaw = btnInsert.first->second;

            // Either a new ButtonRaw was inserted into btnMap, or an existing
            // one was chosen
            if (btnInsert.second)
            {
                // new ButtonRaw created
                //btnRaw.m_pressed = 0;
                btnRaw.m_referenceCount = 1;
            }
            else
            {
                // ButtonRaw already exists, add reference count
                btnRaw.m_referenceCount ++;
            }

            //uint8_t bits = (uint8_t(varCfg.m_trigger) << 0)
            //             | (uint8_t(varCfg.m_invert) << 1)
            //             | (uint8_t(varCfg.m_nextOp) << 2);

            rControl.m_exprPress.emplace_back(btnInsert.first, varCfg);
        }

        // New control has been created, now return a pointer to it

        return ButtonControlIndex(m_btnControls.size() - 1);
    }
}

void UserInputHandler::button_unsubscribe(ButtonControlIndex index)
{
    uint16_t &rRefCount = m_btnControls.at(size_t(index)).m_referenceCount;
    if (0 == rRefCount)
    {
        throw std::runtime_error("Below zero reference count");
    }

    rRefCount --;
}

void UserInputHandler::clear_events()
{
    // remove any just pressed / just released flags

    for (ButtonMap_t::iterator btnIt : m_btnPressed)
    {
        btnIt->second.m_justPressed = false;
    }

    for (ButtonMap_t::iterator btnIt : m_btnReleased)
    {
        btnIt->second.m_justReleased = false;
    }

    // clear the arrays
    m_btnPressed.clear();
    m_btnReleased.clear();

    // Clear mouse delta
    m_mouseMotion.m_rawDelta = Vector2i(0);

    // Clear scroll offset
    m_scrollOffset.offset = Vector2i(0);
}

void UserInputHandler::event_raw(DeviceId deviceId, int buttonEnum,
                                 ButtonRawEvent dir)
{
    // Check if the button is being listened to
    ButtonMap_t::iterator btnIt = m_deviceToButtonRaw[deviceId].find(buttonEnum);

    if (btnIt == m_deviceToButtonRaw[deviceId].end())
    {
        return; // button not registered
    }

    SPDLOG_LOGGER_TRACE(p_logger, "sensitive button pressed");
        
    ButtonRaw &btnRaw = btnIt->second;

    switch (dir)
    {
    case ButtonRawEvent::PRESSED:
        btnRaw.m_pressed = true;
        btnRaw.m_justPressed = true;
        m_btnPressed.push_back(btnIt);
        break;
    case ButtonRawEvent::RELEASED:
        btnRaw.m_pressed = false;
        btnRaw.m_justReleased = true;
        m_btnReleased.push_back(btnIt);
        break;
    }
}

void UserInputHandler::update_controls()
{
    // Loop through controls and see which ones are triggered

    for (ButtonControl &control : m_btnControls)
    {
        ButtonExpr_t* pExprRelease = nullptr;

        // tell eval_button_expression to generate a release expression,
        // if the control is holdable and is not held
        if (control.m_holdable && !control.m_held)
        {
            pExprRelease = &(control.m_exprRelease);
        }

        control.m_triggered = eval_button_expression(control.m_exprPress,
                                                     pExprRelease);

        if (!control.m_holdable)
        {
            continue;
        }

        if (control.m_held)
        {
            // if currently held
            control.m_held = !eval_button_expression(control.m_exprRelease);

            // if just released
            if (!control.m_held)
            {
                control.m_exprRelease.clear();
                SPDLOG_LOGGER_TRACE(m_to->p_logger, "RELEASE");
            }
        }
        else if (control.m_triggered)
        {
            // start holding down the control. control.m_exprRelease should
            // have been generated earlier
            control.m_held = true;
            SPDLOG_LOGGER_TRACE(m_to->p_logger, "HOLD");
        }
    }

    /* Check for mouse motion: if the mouse moved last frame (nonzero_delta),
       apply smoothing and update smoothed output.

       This does smooth out the stuttering at the hardware DPI limit at the cost
       of the smoothed output having some "inertia" and moving after the mouse
       stops at low response factors
    */
    m_mouseMotion.m_smoothDelta = Magnum::Math::lerp(
        m_mouseMotion.m_smoothDelta,
        static_cast<Vector2>(m_mouseMotion.m_rawDelta),
        m_mouseMotion.m_responseFactor);
}

void UserInputHandler::mouse_delta(Vector2i delta)
{
    m_mouseMotion.m_rawDelta = delta;
}

void UserInputHandler::scroll_delta(Vector2i offset)
{
    m_scrollOffset.offset = offset;
}

ControlSubscriber::~ControlSubscriber()
{
    for (ButtonControlIndex const index : m_subscribedButtons)
    {
        m_pInputHandler->button_unsubscribe(index);
    }
}

ButtonControlIndex ControlSubscriber::button_subscribe(std::string_view name)
{
    ButtonControlIndex const index = m_pInputHandler->button_subscribe(name);
    if (ButtonControlIndex::NONE != index)
    {
        m_subscribedButtons.push_back(index);
    }
    return index;
}


}
