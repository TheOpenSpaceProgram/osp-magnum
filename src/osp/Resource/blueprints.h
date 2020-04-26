#pragma once

#include "Package.h"

#include "../Satellites/SatActiveArea.h"
#include "../../types.h"

namespace osp
{

struct PartBlueprint
{ 
    // Specific information on a part:
    // * Which kind of part
    // * Enabled/disabled properties
    // * Transformation

    unsigned m_partIndex; // index to VehicleBlueprint's m_partsUsed


    Vector3 m_translation;
    Quaternion m_rotation;
    Vector3 m_scale;

    // put some sort of config here

};

class VehicleBlueprint
{
    // Specific information on a vehicle
    // * List of part blueprints
    // * Attachments
    // * Wiring

public:

    void debug_add_part(Resource<PartPrototype>& part,
                        const Vector3& translation,
                        const Quaternion& rotation,
                        const Vector3& scale);

    std::vector<ResDependency<PartPrototype> >& get_prototypes()
    {
        return m_prototypes;
    }

    std::vector<PartBlueprint>& get_blueprints()
    {
        return m_blueprints;
    }



private:
    // Unique part Resources used
    std::vector<ResDependency<PartPrototype> > m_prototypes;

    // Arrangement of Individual Parts
    std::vector<PartBlueprint> m_blueprints;

};

}
