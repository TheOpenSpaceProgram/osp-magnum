#pragma once

#include <Magnum/Math/Vector3.h>
#include <memory>

#include "Types.h"

namespace osp
{

class SatelliteObject
{
public:
    SatelliteObject();
    virtual ~SatelliteObject() {};

    virtual int on_load() { return 0; };
};



class Satellite
{
public:
    Satellite();
    Satellite(Satellite&& sat);
    ~Satellite() { m_object.release(); };

    /**
     * @return Display name for this Satellite
     */
    const std::string& get_name() const { return m_name; }

    /**
     * @return Position (relative to parent)
     */
    const Vector3s& get_position() const { return m_position; }

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

    // Describes the functionality of this Satellite.
    std::unique_ptr<SatelliteObject> m_object;

    // TODO: Describes how this satellite travels through space (orbit)
    //std::unique_ptr<Trajectory> m_trajectory;

    // Nice display name for this satellite
    // ie. "Earth", "Voyager 2", ...
    std::string m_name;

    // TODO: Tree structure, and some identification method

    Vector3s m_position;
};


// maybe move this into its own header, but not now
template <class T, class... Args>
T& Satellite::create_object(Args&& ... args)
{
    m_object = std::make_unique<T>(std::forward<Args>(args)...);
    return static_cast<T&>(*m_object);
}


}
