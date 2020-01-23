#pragma once

#include <Magnum/Math/Vector3.h>
#include <memory>

namespace osp
{

typedef int64_t Coordinate;

// A Vector3 for space
typedef Magnum::Math::Vector3<Coordinate> Vector3s;


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
    const Vector3s& get_position() const { return m_position; }

    //void set_object(SatelliteObject *obj);
    template <class T, class... Args>
    T& create_object(Args&& ... args);

protected:

    std::unique_ptr<SatelliteObject> m_object;
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
