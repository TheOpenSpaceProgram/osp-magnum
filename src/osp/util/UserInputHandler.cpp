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

#include "../core/string_concat.h"
#include "logging.h"

#include <Magnum/Math/Functions.h>

namespace osp::input
{

UserInputHandler::UserInputHandler(std::size_t deviceCount)
 : m_deviceToButtonRaw(deviceCount)
{ }

bool UserInputHandler::eval_button_expression(
        ControlExpr_t const& expression,
        ControlExpr_t* pReleaseExpr)
{

    bool totalOn = false;
    bool termOn = false;

    EVarOperator prevOp = EVarOperator::Or;
    auto termStart = expression.begin();

    //for (ButtonVar const& var : expressionession)
    for (auto it =  expression.begin(); it != expression.end(); it ++)
    {
        ControlTerm const& var = *it;
        ButtonRaw const& btnRaw = var.m_button->second;

        bool varOn;

        // Get the value the ButtonVar specifies
        switch(var.m_trigger)
        {
        case EVarTrigger::Pressed:
            if (var.m_invert)
            {
                varOn = btnRaw.m_justReleased;
            }
            else
            {
                varOn = btnRaw.m_justPressed;
            }

            break;
        case EVarTrigger::Hold:
            // "boolIn != bool" is a conditional invert
            // 1 != 1 = 0
            // 0 != 1 = 1
            // 0 != 0 = 0
            // 1 != 0 = 1

            varOn = (btnRaw.m_pressed != var.m_invert);
            break;
        }

        bool lastVar = ((it + 1) == expression.end());

        // Deal with operations on that value
        switch (prevOp)
        {
        case EVarOperator::Or:
            // Current var is the start of a new term.

            // Add the previous term to the total
            totalOn = totalOn || termOn;
            termOn = varOn;
            lastVar = true;

            break;
        case EVarOperator::And:
            termOn = termOn && varOn;
            break;
        }

        if (prevOp == EVarOperator::Or || lastVar)
        {
            // if the previous term contributes to the expression being true
            // and pReleaseExpr exists
            if (termOn && (nullptr != pReleaseExpr))
            {

                auto termEnd = lastVar ? expression.end() : it;

                // loop through the previous term, and add inverted
                // just-pressed buttons to trigger the release
                for (auto itB = termStart; itB != termEnd; itB ++)
                {
                    if (itB->m_trigger != EVarTrigger::Pressed)
                    {
                        continue;
                    }
                    pReleaseExpr->emplace_back(
                            ControlTerm{itB->m_button, EVarTrigger::Pressed,
                                        EVarOperator::Or, !(itB->m_invert)});
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

void UserInputHandler::config_register_control(
        std::string const& name, bool holdable, ControlExprConfig_t vars)
{
    m_btnControlCfg.insert_or_assign(
                name,
                ButtonControlConfig{ std::move(vars), holdable, false, 0 });
}
void UserInputHandler::config_register_control(
        std::string&& name, bool holdable, ControlExprConfig_t vars)
{
    m_btnControlCfg.insert_or_assign(
                std::move(name),
                ButtonControlConfig{ std::move(vars), holdable, false, 0 });
}

EButtonControlIndex UserInputHandler::button_subscribe(std::string_view name)
{

    // check if a config exists for the name given
    auto cfgIt = m_btnControlCfg.find(name);

    if (cfgIt == m_btnControlCfg.end())
    {
        // Config not found, no way to have an empty key so far, so throw an exception
        OSP_LOG_ERROR("No config for{}", name);
        throw std::runtime_error(string_concat("Error: no config with ", name));
    }

    // Check if the control was already created before
    if (cfgIt->second.m_enabled)
    {
        // Use existing ButtonControl
        size_t const index = cfgIt->second.m_index;
        m_btnControls[index].m_referenceCount ++;
        return EButtonControlIndex(index);
    }
    else
    {
        // Create a new ButtonControl

        ControlExprConfig_t const &varConfigs = cfgIt->second.m_press;
        ButtonControl &rControl = m_btnControls.emplace_back();

        rControl.m_holdable = (cfgIt->second.m_holdable);

        rControl.m_exprPress.reserve(varConfigs.size());

        for (ControlTermConfig const &varCfg : varConfigs)
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

            rControl.m_exprPress.emplace_back(varCfg.create(btnInsert.first));
        }

        // New control has been created, now return a pointer to it

        return EButtonControlIndex(m_btnControls.size() - 1);
    }
}

void UserInputHandler::button_unsubscribe(EButtonControlIndex index)
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

    // Clear the button pressed and released events
    m_btnPressed.clear();
    m_btnReleased.clear();

    // Clear mouse delta
    m_mouseMotion.m_rawDelta = Vector2i(0);

    // Clear scroll offset
    m_scrollOffset.offset = Vector2i(0);

    // Clear control events
    m_btnControlEvents.clear();
}

void UserInputHandler::event_raw(DeviceId deviceId, int buttonEnum,
                                 EButtonEvent dir)
{
    // Check if the button is being listened to
    ButtonMap_t::iterator btnIt = m_deviceToButtonRaw[deviceId].find(buttonEnum);

    if (btnIt == m_deviceToButtonRaw[deviceId].end())
    {
        return; // button not registered
    }

    OSP_LOG_TRACE("sensitive button pressed");
        
    ButtonRaw &btnRaw = btnIt->second;

    switch (dir)
    {
    case EButtonEvent::Pressed:
        btnRaw.m_pressed = true;
        btnRaw.m_justPressed = true;
        m_btnPressed.push_back(btnIt);
        break;
    case EButtonEvent::Released:
        btnRaw.m_pressed = false;
        btnRaw.m_justReleased = true;
        m_btnReleased.push_back(btnIt);
        break;
    }
}

void UserInputHandler::update_controls()
{

    // Loop through controls and see which ones are triggered

    for (auto it = std::begin(m_btnControls); it != std::end(m_btnControls);
         std::advance(it, 1))
    {
        ButtonControl &rControl = *it;
        EButtonControlIndex const index = EButtonControlIndex(
                    std::distance(std::begin(m_btnControls), it));

        ControlExpr_t* pExprRelease = nullptr;

        // tell eval_button_expression to generate a release expression,
        // if the control is holdable and is not held
        if (rControl.m_holdable && !rControl.m_held)
        {
            pExprRelease = &(rControl.m_exprRelease);
        }

        rControl.m_triggered = eval_button_expression(rControl.m_exprPress,
                                                     pExprRelease);

        if (rControl.m_triggered)
        {
            m_btnControlEvents.emplace_back(index,
                                            EButtonControlEvent::Triggered);
        }

        if (!rControl.m_holdable)
        {
            continue;
        }

        if (rControl.m_held)
        {
            // if currently held, evaluate the release expression
            rControl.m_held = !eval_button_expression(rControl.m_exprRelease);

            // if just released
            if (!rControl.m_held)
            {
                rControl.m_exprRelease.clear();
                m_btnControlEvents.emplace_back(index,
                                                EButtonControlEvent::Released);
                OSP_LOG_TRACE("RELEASE");
            }
        }
        else if (rControl.m_triggered)
        {
            // start holding down the control. control.m_exprRelease should
            // have been generated earlier
            rControl.m_held = true;
            OSP_LOG_TRACE("HOLD");
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

//-----------------------------------------------------------------------------

ControlSubscriber::~ControlSubscriber()
{
    unsubscribe();
}

EButtonControlIndex ControlSubscriber::button_subscribe(std::string_view name)
{
    EButtonControlIndex const index = m_pInputHandler->button_subscribe(name);
    if (EButtonControlIndex::NONE != index)
    {
        m_subscribedButtons.push_back(index);
    }
    return index;
}

void ControlSubscriber::unsubscribe()
{
    for (EButtonControlIndex const index : m_subscribedButtons)
    {
        m_pInputHandler->button_unsubscribe(index);
    }
    m_subscribedButtons.clear();
}

} // namespace osp::input
