#pragma once

#include <Magnum/Math/Vector3.h>
#include <memory>

#include "../types.h"

namespace osp
{

class Satellite;
class Universe;

class SatelliteObject
{
    friend Satellite;

public:

    struct Id
    {
        // might put more stuff here
        std::string const& m_name;
    };


    SatelliteObject();
    virtual ~SatelliteObject() {}

    virtual Id const& get_id() = 0;

    virtual bool is_activatable() const { return false; }
    
    Satellite* get_satellite() { return m_sat; }


protected:

    //static Id m_identity;
    Satellite* m_sat;
};



class Satellite
{

public:
    Satellite(Universe* universe);
    Satellite(Satellite&& move);
    ~Satellite();

    Satellite& operator=(Satellite&& move);

    bool is_activatable() const;


    float get_load_radius() const { return m_loadRadius; }

    /**
     * @return Display name for this Satellite
     */
    const std::string& get_name() const { return m_name; }

    /**
     * @return Position (relative to parent)
     */
    const Vector3sp& get_position() const { return m_position; }

    SatelliteObject* get_loader() { return m_activatedBy; }

    Universe* get_universe() { return m_universe; }


    void set_loader(SatelliteObject* sat) { m_activatedBy = sat; }

    void set_position(Vector3sp const& position) { m_position = position; }

    void set_name(std::string const& name) { m_name = name; }



    Vector3sp position_relative_to(Satellite& referenceFrame) const;

    SatelliteObject* get_object() { return m_object.get(); }

    // too lazy to implement rn
    //void set_object(SatelliteObject *obj);

    /**
     * Create and set a new object
     * @param args... Arguments to pass to SatelliteObject constructor
     * @return Pointer to Satellite object
     */
    template <class T, class... Args>
    T& create_object(Args&& ... args);



protected:

    // true for things that describe something that actually has mass
    // * planets, stars, maybe spacecraft etc...
    //
    // false for things that aren't real, but can still have 'mass'
    // * barycenters, reference frames, waypoints
    bool m_physical;

    // In meters. describes a sphere (or maybe a cube) around the satellite,
    // that when it intersects with the sphere of an Active Area, it loads it.
    float m_loadRadius;

    // in kg
    float m_mass;

    // Pointer to [probably an active area] when activated
    SatelliteObject* m_activatedBy;

    // Describes the functionality of this Satellite.
    std::unique_ptr<SatelliteObject> m_object;

    // Universe this satellite is part of. the only time this pointer will be
    // invalid is the end of the universe.
    Universe* m_universe;

    // TODO: Describes how this satellite travels through space (orbit)
    //std::unique_ptr<Trajectory> m_trajectory;

    // Nice display name for this satellite
    // ie. "Earth", "Voyager 2", ...
    std::string m_name;

    // TODO: Tree structure, and some identification method

    Vector3sp m_position;

    //int m_precision;
};


// maybe move this into its own header, but not now
template <class T, class... Args>
T& Satellite::create_object(Args&& ... args)
{
    m_object = std::make_unique<T>(std::forward<Args>(args)...);
    m_object->m_sat = this;
    return static_cast<T&>(*m_object);
}


}
