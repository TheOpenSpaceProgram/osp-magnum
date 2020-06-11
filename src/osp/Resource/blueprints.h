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
    BlueprintWire(unsigned fromPart, unsigned fromMachine, WireOutPort fromPort,
                  unsigned toPart, unsigned toMachine, WireOutPort toPort) :
        m_fromPart(fromPart),
        m_fromMachine(fromMachine),
        m_fromPort(fromPort),
        m_toPart(toPart),
        m_toMachine(toMachine),
        m_toPort(toPort)
    {}

    unsigned m_fromPart;
    unsigned m_fromMachine;
    WireOutPort m_fromPort;

    unsigned m_toPart;
    unsigned m_toMachine;
    WireInPort m_toPort;
};

struct BlueprintMachine
{
    // TODO specific settings for a machine
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

    void add_wire(unsigned fromPart, unsigned fromMachine, WireOutPort fromPort,
                  unsigned toPart, unsigned toMachine, WireOutPort toPort);

    std::vector<ResDependency<PrototypePart> >& get_prototypes()
    {
        return m_prototypes;
    }

    std::vector<BlueprintPart>& get_blueprints()
    {
        return m_blueprints;
    }

    std::vector<BlueprintWire>& get_wires()
    {
        return m_wires;
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
