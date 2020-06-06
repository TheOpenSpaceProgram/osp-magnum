#include "blueprints.h"


using namespace osp;

void BlueprintVehicle::add_part(
                        Resource<PrototypePart>& prototype, const Vector3& translation,
                        const Quaternion& rotation, const Vector3& scale)
{
    unsigned partIndex = m_prototypes.size();

    // check if the part is added already.
    for (unsigned i = 0; i < partIndex; i ++)
    {
        const ResDependency<PrototypePart>& dep = m_prototypes[i];

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

    BlueprintPart blueprint{partIndex, translation, rotation, scale};

    m_blueprints.push_back(std::move(blueprint));

}

void BlueprintVehicle::add_wire(unsigned partFrom, WireOutPort portFrom,
                                unsigned partTo, WireInPort portTo)
{
    m_wires.emplace_back(partFrom, portFrom, partTo, portTo);
}
