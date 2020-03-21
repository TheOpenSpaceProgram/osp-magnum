#include "blueprints.h"


using namespace osp;

void VehicleBlueprint::debug_add_part(
                        Resource<PartPrototype>& prototype, const Vector3& translation,
                        const Quaternion& rotation, const Vector3& scale)
{
    unsigned partIndex = m_prototypes.size();

    // check if the part is added already.
    for (unsigned i = 0; i < partIndex; i ++)
    {
        const ResDependency<PartPrototype>& dep = m_prototypes[i];

        // see if they're the same data, find a better way eventually
        if (dep.list() == &(prototype))
        {
            // prototype was already added to the list
            partIndex = i;
            break;
        }
    }

    if (partIndex == m_prototypes.size())
    {
        // Add the unlisted prototype to the end
        m_prototypes.emplace_back(prototype);
    }

    // now we have a part index.

    // just create a new part blueprint will all the data

    PartBlueprint blueprint{partIndex, translation, rotation, scale};

    m_blueprints.push_back(std::move(blueprint));

}
