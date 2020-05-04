#include <iostream>


#include "../Satellites/SatActiveArea.h"

#include "FtrNewtonBody.h"

namespace osp
{


FtrNewtonBody::FtrNewtonBody(Object3D& object, SatActiveArea& area):
        AbstractGroupedFeature(object, &area)
{

    NewtonWorld* world = area.get_newton_world();
    Matrix4 matrix(object.transformationMatrix());

    NewtonCollision const *ball = NewtonCreateSphere(world, 1, 0, NULL);

    m_body = NewtonCreateDynamicBody(world, ball, matrix.data());

    NewtonBodySetMassMatrix(m_body, 1.0f, 1, 1, 1);
}


FtrNewtonBody::~FtrNewtonBody()
{
    NewtonDestroyBody(m_body);
}

void FtrNewtonBody::nwt_set_matrix(Matrix4 const& matrix)
{
    NewtonBodySetMatrix(m_body, matrix.data());
}

}
