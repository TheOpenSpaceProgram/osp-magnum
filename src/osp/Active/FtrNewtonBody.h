#ifndef FTRNEWTONBODY_H
#define FTRNEWTONBODY_H


#include "../scene.h"

#include <Magnum/SceneGraph/Scene.h>
#include <Magnum/SceneGraph/AbstractGroupedFeature.h>


#include <Newton.h>

using Magnum::SceneGraph::AbstractGroupedFeature;
using Magnum::SceneGraph::FeatureGroup;
using Magnum::Float;

namespace osp
{

class SatActiveArea;
class FtrNewtonBody;

typedef FeatureGroup<3, FtrNewtonBody, Float> GroupFtrNewtonBody;
typedef AbstractGroupedFeature<3, FtrNewtonBody, Float> AbstractFtrNewtonBody;

// Manages a NewtonBody
class FtrNewtonBody: public AbstractFtrNewtonBody
{
public:
    FtrNewtonBody(Object3D& object, SatActiveArea& group);

    ~FtrNewtonBody();

    NewtonBody* get_body() { return m_body; }

protected:
    NewtonBody *m_body;
};

}

#endif // FTRNEWTONBODY_H
