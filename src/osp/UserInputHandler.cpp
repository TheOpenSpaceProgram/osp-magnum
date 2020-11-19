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
#include <iostream>
#include "UserInputHandler.h"
#include <toml.hpp>
#include "osp/Resource/Package.h"
#include "osp/Resource/AssetImporter.h"

namespace osp
{

//constexpr ButtonVarConfig::ButtonVarConfig(int device, int devEnum,
//                                   VarTrigger trigger,  bool invert,
//                                   VarOperator nextOp) :

//{
    //m_bits = (uint8_t(trigger) << 0)
    //       | (uint8_t(invert) << 1);
    //       | (uint8_t(nextOp) << 2)
//}


ButtonControlHandle::ButtonControlHandle(UserInputHandler *to, int index) :
        m_to(to), m_index(index)
{
    std::cout << "ButtonControlHandle created\n";
    to->m_controls[index].m_referenceCount ++;
}

ButtonControlHandle::~ButtonControlHandle()
{
    std::cout << "ButtonControlHandle is dead\n";
    m_to->m_controls[m_index].m_referenceCount --;
}

bool ButtonControlHandle::triggered()
{
    return m_to->m_controls[m_index].m_triggered;
}

bool ButtonControlHandle::trigger_hold()
{
    return m_to->m_controls[m_index].m_held;
}

MouseMovementHandle::MouseMovementHandle(UserInputHandler *to) : m_to(to)
{
    std::cout << "MouseMovementHandle created\n";
    m_to->m_mouseMotion.m_referenceCount++;
}

MouseMovementHandle::~MouseMovementHandle()
{
    std::cout << "MouseMovementHandle is dead\n";
    m_to->m_mouseMotion.m_referenceCount--;
}

float MouseMovementHandle::dxSmooth() const
{
    return m_to->m_mouseMotion.m_smoothDelta.x();
}

float MouseMovementHandle::dySmooth() const
{
    return m_to->m_mouseMotion.m_smoothDelta.y();
}

int MouseMovementHandle::dxRaw() const
{
    return m_to->m_mouseMotion.m_rawDelta.x();
}

int MouseMovementHandle::dyRaw() const
{
    return m_to->m_mouseMotion.m_rawDelta.y();
}

ScrollInputHandle::ScrollInputHandle(UserInputHandler *to) : m_to(to)
{
    std::cout << "ScrollInputHandle created\n";
    m_to->m_scrollOffset.m_referenceCount++;
}

ScrollInputHandle::~ScrollInputHandle()
{
    std::cout << "ScrollInputHandle is dead\n";
    m_to->m_scrollOffset.m_referenceCount--;
}

int ScrollInputHandle::dx() const
{
    return m_to->m_scrollOffset.offset.x();
}

int ScrollInputHandle::dy() const
{
    return m_to->m_scrollOffset.offset.y();
}

ButtonVar::ButtonVar(ButtonMap::iterator button, VarTrigger trigger,
                     bool invert, VarOperator nextOp) :
        m_button(button),
        m_trigger(trigger),
        m_invert(invert),
        m_nextOp(nextOp) {}

ButtonVar::ButtonVar(ButtonMap::iterator button, ButtonVarConfig cfg) :
        m_button(button),
        m_trigger(cfg.m_trigger),
        m_invert(cfg.m_invert),
        m_nextOp(cfg.m_nextOp) {}


UserInputHandler::UserInputHandler(int deviceCount) :
        m_deviceToButtonRaw(deviceCount)
{
    m_controls.reserve(7);
}

bool UserInputHandler::eval_button_expression(
        ButtonExpr const& rExpr,
        ButtonExpr* pReleaseExpr)
{

    using VarOperator = ButtonVar::VarOperator;
    using VarTrigger = ButtonVar::VarTrigger;

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

        //std::cout << detect << ", ";
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


void UserInputHandler::config_register_control(std::string const& name,
        bool holdable,
        std::initializer_list<ButtonVarConfig> vars)
{
    config_register_control(name, holdable, std::vector(vars));
}

void UserInputHandler::config_register_control(std::string const& name,
    bool holdable,
    std::vector<ButtonVarConfig> vars) {
    m_controlConfigs[name] = ButtonConfig{std::move(vars), holdable,
                                          false, 0 };
}

ButtonControlHandle UserInputHandler::config_get(std::string const& name)
{

    // check if a config exists for the name given
    auto cfgIt = m_controlConfigs.find(name);

    if (cfgIt == m_controlConfigs.end())
    {
        // Config not found!
        std::cout << "config not found!\n";
        return ButtonControlHandle(nullptr, 0);
    }

    //int index;

    // Check if the control was already created before
    if (cfgIt->second.m_enabled)
    {
        // Use existing ButtonControl
        return ButtonControlHandle(this, cfgIt->second.m_index);
        //m_controls[index].m_referenceCount ++;
    }
    else
    {
        // Create a new ButtonControl

        std::vector<ButtonVarConfig> &varConfigs = cfgIt->second.m_press;
        ButtonControl &control = m_controls.emplace_back();

        control.m_holdable = (cfgIt->second.m_holdable);
        control.m_held = false;

        control.m_exprPress.reserve(varConfigs.size());

        for (ButtonVarConfig &varCfg : varConfigs)
        {
            // Each loop, create a ButtonVar from the ButtonConfig and emplace
            // it into control.m_vars

            // Map of buttons for the specified device
            ButtonMap &btnMap = m_deviceToButtonRaw[varCfg.m_device];

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

            control.m_exprPress.emplace_back(btnInsert.first, varCfg);
        }

        // New control has been created, now return a pointer to it

        return ButtonControlHandle(this, m_controls.size() - 1);
    }
}

MouseMovementHandle UserInputHandler::mouse_get()
{
    return MouseMovementHandle(this);
}

ScrollInputHandle UserInputHandler::scroll_get()
{
    return ScrollInputHandle(this);
}

void UserInputHandler::clear_events()
{
    // remove any just pressed / just released flags

    for (ButtonMap::iterator btnIt : m_btnPressed)
    {
        btnIt->second.m_justPressed = false;
    }

    for (ButtonMap::iterator btnIt : m_btnReleased)
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
    ButtonMap::iterator btnIt = m_deviceToButtonRaw[deviceId].find(buttonEnum);

    if (btnIt == m_deviceToButtonRaw[deviceId].end())
    {
        return; // button not registered
    }

    std::cout << "sensitive button pressed\n";

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

    for (ButtonControl &control : m_controls)
    {
        ButtonExpr* pExprRelease = nullptr;

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
                std::cout << "RELEASE\n";
            }
        }
        else if (control.m_triggered)
        {
            // start holding down the control. control.m_exprRelease should
            // have been generated earlier
            control.m_held = true;
            std::cout << "HOLD\n";
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

void UserInputHandler::save_config(std::string const& file)
{
    
}

void UserInputHandler::load_config(std::string const& file, Package& pack)
{
    AssetImporter::load_text_to_toml(file, pack);
    toml::value toml = *pack.get<toml::value>(file);
    //Load the config
    
}

}
