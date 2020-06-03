#include <iostream>
#include "UserInputHandler.h"

namespace osp
{

//constexpr ButtonTermConfig::ButtonTermConfig(int device, int devEnum,
//                                   TermTrigger trigger,  bool invert,
//                                   TermOperator nextOp) :

//{
    //m_bits = (uint8_t(trigger) << 0)
    //       | (uint8_t(invert) << 1);
    //       | (uint8_t(nextOp) << 2)
//}


ButtonControlHandle::ButtonControlHandle(UserInputHandler *to, int index) :
        m_to(to), m_index(index)
{
    std::cout << "ButtonControlHandle created\n";
    to->m_controls[index].m_reference_count ++;
}

ButtonControlHandle::~ButtonControlHandle()
{
    std::cout << "ButtonControlHandle is dead\n";

    m_to->m_controls[m_index].m_reference_count --;
}

bool ButtonControlHandle::triggered()
{
    return m_to->m_controls[m_index].m_triggered;
}

bool ButtonControlHandle::trigger_hold()
{
    return m_to->m_controls[m_index].m_triggerHold;
}


ButtonTerm::ButtonTerm(ButtonMap::iterator button, ButtonTermConfig cfg) :
    m_button(button),
    m_trigger(cfg.m_trigger),
    m_invert(cfg.m_invert),
    m_nextOp(cfg.m_nextOp) {}


UserInputHandler::UserInputHandler(int deviceCount) :
    m_deviceToButtonRaw(deviceCount)
{

}

void UserInputHandler::config_register_control(std::string const& name,
        std::initializer_list<ButtonTermConfig> terms)
{
    std::vector<ButtonTermConfig> termVector(terms);
    m_controlConfigs[name] = {std::move(termVector), false, 0};
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

    int index;

    // Check if the control was already created before
    if (cfgIt->second.m_enabled)
    {
        // Use existing ButtonControl
        return ButtonControlHandle(this, cfgIt->second.m_index);
        //m_controls[index].m_reference_count ++;
    }
    else
    {
        // Create a new ButtonControl

        std::vector<ButtonTermConfig> &termConfigs = cfgIt->second.m_terms;
        ButtonControl &control = m_controls.emplace_back();

        control.m_terms.reserve(termConfigs.size());

        for (ButtonTermConfig &termCfg : termConfigs)
        {
            // Each loop, create a ButtonTerm from the ButtonConfig and emplace
            // it into control.m_terms

            // Map of buttons for the specified device
            ButtonMap &btnMap = m_deviceToButtonRaw[termCfg.m_device];

            // try inserting a new button index
            auto btnInsert = btnMap.insert(std::make_pair(termCfg.m_devEnum,
                                                          ButtonRaw()));
            ButtonRaw &btnRaw = btnInsert.first->second;

            // Either a new ButtonRaw was inserted into btnMap, or an existing
            // one was chosen
            if (btnInsert.second)
            {
                // new ButtonRaw created
                //btnRaw.m_pressed = 0;
                btnRaw.m_reference_count = 1;
            }
            else
            {
                // ButtonRaw already exists, add reference count
                btnRaw.m_reference_count ++;
            }

            //uint8_t bits = (uint8_t(termCfg.m_trigger) << 0)
            //             | (uint8_t(termCfg.m_invert) << 1)
            //             | (uint8_t(termCfg.m_nextOp) << 2);

            control.m_terms.emplace_back(btnInsert.first, termCfg);
        }

        // New control has been created, now return a pointer to it

        return ButtonControlHandle(this, m_controls.size() - 1);
    }
}

void UserInputHandler::event_clear()
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

    //std::cout << "controls\n";

    for (ButtonControl &control : m_controls)
    {
        using TermOperator = ButtonTerm::TermOperator;
        using TermTrigger = ButtonTerm::TermTrigger;

        bool holdPresent = false;
        bool current = false, currentHold = false;
        bool trigger = false, triggerHold = false;
        TermOperator prevOp = TermOperator::OR;

        //std::cout << "* [ ";

        for (ButtonTerm &term : control.m_terms)
        {
            // Test if the current term is true

            bool detect, detectHold;

            ButtonRaw &btnRaw = term.m_button->second;

            switch(term.m_trigger)
            {
            case TermTrigger::PRESSED:
                if (term.m_invert)
                {
                    detect = btnRaw.m_justReleased;
                }
                else
                {
                    detect = btnRaw.m_justPressed;
                }
                detectHold = true;
                break;
            case TermTrigger::HOLD:
                // "boolIn != bool" is a conditional invert
                // 1 != 1 = 0
                // 0 != 1 = 1
                // 0 != 0 = 0
                // 1 != 0 = 1
                detect = detectHold = btnRaw.m_pressed != term.m_invert;
                holdPresent = true;
                break;
            }

            switch (prevOp)
            {
            case TermOperator::OR:
                trigger = trigger || current;
                current = detect;
                if (holdPresent)
                {
                    triggerHold = triggerHold || currentHold;
                    currentHold = detectHold;
                }
                break;
            case TermOperator::AND:
                current = current && detect;
                currentHold = currentHold && detectHold;
                break;
            }

            //std::cout << detect << ", ";

            prevOp = term.m_nextOp;

        }

        trigger = trigger || current;
        triggerHold = triggerHold || currentHold;

        //std::cout << "] trigger: " << trigger
        //          << ", hold: " << triggerHold << "\n";

        control.m_triggered = trigger;
        control.m_triggerHold = triggerHold;
    }
}

}
