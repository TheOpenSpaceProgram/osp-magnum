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

#include "physics.h"
#include "basic.h"

#include "../types.h"
#include "../universetypes.h"

namespace osp::universe { struct UCompActiveArea; }

namespace osp::active
{


struct ACtxAreaLink
{    
    universe::Satellite m_areaSat;

    //struct Enter { universe::Satellite m_sat; };
    //struct Leave { universe::Satellite m_sat; };
    //std::vector< std::variant<Enter, Leave> > m_satQueue;

    std::vector<universe::Satellite> m_enter;
    std::vector<universe::Satellite> m_leave;

    Vector3 m_move;
};

struct ACompActivatedSat
{
    universe::Satellite m_sat;
};


/**
 * System used to associate an ActiveScene to a UCompActiveArea in the Universe
 */
class SysAreaAssociate
{
public:

    /**
     * @brief Consume data queued in the universe's UCompActiveArea to be used
     *        by other active systems
     *
     * @param rScene [ref] Scene assicated with ActiveArea
     */
    static void update_consume(
            ACtxAreaLink& rLink, universe::UCompActiveArea& rAreaUComp);

    /**
     * @brief Translates all entities with ACompFloatingOrigin based on
     *        UCompActiveArea::m_moved read during update_consume
     *
     * @param rScene [ref] Scene assicated with ActiveArea
     */
    static void update_translate(
            ACtxAreaLink& rLink, universe::UCompActiveArea& rAreaUComp);

private:


    /**
     * @brief Translate all entities in an ActiveScene that contain an
     *        ACompFloatingOrigin
     *
     * @param rScene      [ref] Scene containing entities that can be translated
     * @param translation [in] Meters to translate entities by
     */
    static void floating_origin_translate(
            acomp_view_t<ACompFloatingOrigin const> viewFloatingOrigin,
            acomp_view_t<ACompTransform> viewTf,
            acomp_view_t<ACompTransformControlled const> viewTfControlled,
            acomp_view_t<ACompTransformMutable> viewTfMutable,
            ACtxPhysics& rCtxPhys,
            Vector3 translation);

};

}
