#include "../testapp.h"

namespace lander
{
    using testapp::TestApp;

    using RendererSetupFunc_t = void (*)(TestApp &);
    using MagnumDrawFunc_t = void (*)(TestApp &, osp::Session const &, osp::Session const &, osp::Session const &);

    RendererSetupFunc_t setup_lander_scenario(TestApp &rTestApp);
} // namespace lander