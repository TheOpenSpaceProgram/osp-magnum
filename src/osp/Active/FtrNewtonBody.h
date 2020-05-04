#pragma once


#include "../scene.h"
#include "../../types.h"


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

    void nwt_set_matrix(Matrix4 const& matrix);

protected:
    NewtonBody *m_body;
};

}
