#pragma once

#include "../../types.h"

#include "../Active/SysWire.h"
#include "../Resource/Resource.h"
#include "../Resource/PrototypePart.h"

namespace osp
{


struct BlueprintPart
{ 
    // Specific information on a part:
    // * Which kind of part
    // * Enabled/disabled properties
    // * Transformation

    unsigned m_partIndex; // index to BlueprintVehicle's m_partsUsed


    Vector3 m_translation;
    Quaternion m_rotation;
    Vector3 m_scale;

    // put some sort of config here

};

struct BlueprintWire
{
    BlueprintWire(unsigned partFrom, WireOutPort portFrom,
                                 unsigned partTo, WireInPort portTo) :
        m_partFrom(partFrom),
        m_portFrom(portFrom),
        m_partTo(partTo),
        m_portTo(portTo)
    {}

    unsigned m_partFrom;
    WireOutPort m_portFrom;

    unsigned m_partTo;
    WireInPort m_portTo;
};

struct BlueprintMachine
{

};

class BlueprintVehicle
{
    // Specific information on a vehicle
    // * List of part blueprints
    // * Attachments
    // * Wiring

public:

    void add_part(Resource<PrototypePart>& part,
                  const Vector3& translation,
                  const Quaternion& rotation,
                  const Vector3& scale);

    void add_wire(unsigned partFrom, WireOutPort portFrom,
                  unsigned partTo, WireInPort portTo);

    std::vector<ResDependency<PrototypePart> >& get_prototypes()
    {
        return m_prototypes;
    }

    std::vector<BlueprintPart>& get_blueprints()
    {
        return m_blueprints;
    }



private:
    // Unique part Resources used
    std::vector<ResDependency<PrototypePart> > m_prototypes;

    // Arrangement of Individual Parts
    std::vector<BlueprintPart> m_blueprints;

    // Wires to connect
    std::vector<BlueprintWire> m_wires;
};

}
