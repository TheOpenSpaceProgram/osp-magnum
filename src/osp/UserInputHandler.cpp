#include "UserInputHandler.h"

namespace osp
{

ButtonTermConfig::ButtonTermConfig(int device, int devEnum,
                                   TermTrigger trigger,  bool invert,
                                   TermOperator nextOp)
{

}

UserInputHandler::UserInputHandler(int deviceCount) :
    m_devices(deviceCount)
{

}

void UserInputHandler::config_register_control(std::string const& name,
                                               ButtonTermConfig terms...)
{

}

ButtonHandle UserInputHandler::config_get(std::string const& name)
{
    return ButtonHandle(this, 0);
}

void UserInputHandler::event_clear()
{

}
void UserInputHandler::event_raw(int deviceId, int buttonEnum, ButtonEvent dir)
{

}
void UserInputHandler::update_controls()
{

}

}
