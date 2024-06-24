/**
 * Open Space Program
 * Copyright © 2019-2024 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#pragma once

#include "forcefactors.h"

#include <osp/activescene/basic.h>
#include <osp/core/id_map.h>


#include <Jolt/Jolt.h>
//suppress common warnings triggered by Jolt
JPH_SUPPRESS_WARNING_PUSH
JPH_SUPPRESS_WARNINGS

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Collision/Shape/CompoundShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/MutableCompoundShape.h>
#include <Jolt/Physics/PhysicsStepListener.h>

JPH_SUPPRESS_WARNING_POP

#include <longeron/id_management/registry_stl.hpp>
#include <longeron/id_management/id_set_stl.hpp>

#include <entt/core/any.hpp>

#include <spdlog/spdlog.h>

#include <iostream>
#include <cstdarg>
#include <osp/util/logging.h>

#include "osp/core/strong_id.h"

namespace ospjolt
{
using namespace JPH;

using JoltBodyPtr_t = std::unique_ptr< Body >;

using BodyId = osp::StrongId<uint32, struct DummyForBodyId>;

inline JPH::BodyID BToJolt(const BodyId& bodyId)
{
    return JPH::BodyID(bodyId.value, 0);
}

inline osp::Vector3 Vec3JoltToMagnum(Vec3 in)
{
    return osp::Vector3(in.GetX(), in.GetY(), in.GetZ());
}

inline Vec3 Vec3MagnumToJolt(osp::Vector3 in)
{
    return Vec3(in.x(), in.y(), in.z());
}

inline Quat QuatMagnumToJolt(osp::Quaternion in)
{
    return Quat(in.vector().x(), in.vector().y(), in.vector().z(), in.scalar());
}

inline osp::Quaternion QuatJoltToMagnum(Quat in)
{
    return osp::Quaternion(osp::Vector3(in.GetX(), in.GetY(), in.GetZ()), in.GetW());
}

#ifdef JPH_ENABLE_ASSERTS

// Callback for asserts, connect this to your own assert handler if you have one
static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, uint inLine)
{
    // Print to the TTY
    std::cout << inFile << ":" << inLine << ": (" << inExpression << ") " << (inMessage != nullptr? inMessage : "") << std::endl;

    // Breakpoint
    return true;
};

#endif // JPH_ENABLE_ASSERTS

static void TraceImpl(const char *inFMT, ...)
{
     // Format the message
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
    va_end(list);

    // Print to the TTY
    OSP_LOG_TRACE(buffer);
}


/**
 * @brief The different physics layers for the simulation
 */
namespace Layers
{
    static constexpr ObjectLayer NON_MOVING = 0;
    static constexpr ObjectLayer MOVING = 1;
    static constexpr ObjectLayer NUM_LAYERS = 2;
};
/**
 * @brief Class that determines if two object layers can collide
 */
class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter
{
public:
    bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override
    {
        switch (inObject1)
        {
        case Layers::NON_MOVING:
            return inObject2 == Layers::MOVING; // Non moving only collides with moving
        case Layers::MOVING:
            return true; // Moving collides with everything
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};
/**
 * @brief The different broad phase layers (currently identical to physics Layers, but might change in the future)
 */
namespace BroadPhaseLayers
{
    static constexpr BroadPhaseLayer NON_MOVING(0);
    static constexpr BroadPhaseLayer MOVING(1);
    static constexpr uint NUM_LAYERS(2);
};

/**
 * @brief Class that defines a mapping between object and broadphase layers.
 */
class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface
{
public:
    BPLayerInterfaceImpl()
    {
        // Create a mapping table from object to broad phase layer
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    uint GetNumBroadPhaseLayers() const override
    {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override
    {
        JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    /// Get the user readable name of a broadphase layer (debugging purposes)
    const char * GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override
    {
        if (inLayer == BroadPhaseLayers::NON_MOVING) {
            return "NON_MOVING";
        }
        else {
            return "MOVING";
        }
    }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
    BroadPhaseLayer					mObjectToBroadPhase[Layers::NUM_LAYERS];
};
/**
 * @brief Class that determines if an object layer can collide with a broadphase layer
 */
class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter
{
public:
    bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override
    {
        switch (inLayer1)
        {
        case Layers::NON_MOVING:
            return inLayer2 == BroadPhaseLayers::MOVING;
        case Layers::MOVING:
            return true;
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

struct ACtxJoltWorld;

//The physics callback to sync bodies between osp and jolt
class PhysicsStepListenerImpl : public PhysicsStepListener
{
public:
    PhysicsStepListenerImpl(ACtxJoltWorld* pContext) : m_context(pContext) {};
    void OnStep(float inDeltaTime, PhysicsSystem& rPhysicsSystem) override;

private:
    ACtxJoltWorld* m_context;
};

using ShapeStorage_t = osp::Storage_t<osp::active::ActiveEnt, Ref<Shape>>;

/**
 * @brief Represents an instance of a Jolt physics world in the scene
 */
struct ACtxJoltWorld
{
public:
    struct ForceFactorFunc
    {
        using UserData_t = std::array<void*, 6u>;
        using Func_t = void (*)(BodyId bodyId, ACtxJoltWorld const&, UserData_t, osp::Vector3&, osp::Vector3&) noexcept;

        Func_t      m_func{nullptr};
        UserData_t  m_userData{nullptr};
    };
    // The default values are the one suggested in the Jolt hello world exemple for a "real" project.
    // It might be overkill here.
    ACtxJoltWorld(  int threadCount = 2,
                    uint maxBodies = 65536, 
                    uint numBodyMutexes = 0, 
                    uint maxBodyPairs = 65536, 
                    uint maxContactConstraints = 10240
                ) : m_pPhysicsSystem(std::make_unique<PhysicsSystem>()), 
                    m_temp_allocator(10 * 1024 * 1024),
                    m_joltJobSystem(std::make_unique<JobSystemThreadPool>(cMaxPhysicsJobs, cMaxPhysicsBarriers, threadCount))
    {
        m_pPhysicsSystem->Init(maxBodies, 
                    numBodyMutexes, 
                    maxBodyPairs, 
                    maxContactConstraints, 
                    m_bPLInterface, 	
                    m_objectVsBPLFilter, 
                    m_objectLayerFilter);
        //gravity is handled on the OSP side
        m_pPhysicsSystem->SetGravity(Vec3Arg::sZero());
        m_listener = std::make_unique<PhysicsStepListenerImpl>(this);
        m_pPhysicsSystem->AddStepListener(m_listener.get());
    }

    //mandatory jolt initialization steps
    //Should this be called on world creation ? not ideal in case of multiple worlds.
    static void initJoltGlobal()
    {
        std::call_once(ACtxJoltWorld::initFlag, ACtxJoltWorld::initJoltGlobalInternal);
    }

    

    TempAllocatorImpl                                   m_temp_allocator;
    ObjectLayerPairFilterImpl                           m_objectLayerFilter;
    BPLayerInterfaceImpl                                m_bPLInterface;
    ObjectVsBroadPhaseLayerFilterImpl                   m_objectVsBPLFilter;
    std::unique_ptr<JobSystem>                          m_joltJobSystem;

    std::unique_ptr<PhysicsSystem>                      m_pPhysicsSystem;

    std::unique_ptr<PhysicsStepListenerImpl>		    m_listener;

    lgrn::IdRegistryStl<BodyId>                         m_bodyIds;
    osp::IdMap_t<BodyId, ForceFactors_t>                m_bodyFactors;
    lgrn::IdSetStl<BodyId>                              m_bodyDirty;

    osp::IdMap_t<BodyId, osp::active::ActiveEnt>        m_bodyToEnt;
    osp::IdMap_t<osp::active::ActiveEnt, BodyId>        m_entToBody;

    std::vector<ForceFactorFunc>                        m_factors;
    ShapeStorage_t                                      m_shapes;

    osp::active::ACompTransformStorage_t                *m_pTransform{nullptr};

private:

    static void initJoltGlobalInternal() 
    {
        
        RegisterDefaultAllocator();

        // Install trace and assert callbacks
        JPH::Trace = &TraceImpl;
        JPH_IF_ENABLE_ASSERTS(AssertFailed = &AssertFailedImpl;)

        // Create a factory, this class is responsible for creating instances of classes based on their name or hash and is mainly used for deserialization of saved data.
        // It is not directly used in this example but still required.
        Factory::sInstance = new Factory();

        // Register all physics types with the factory and install their collision handlers with the CollisionDispatch class.
        // If you have your own custom shape types you probably need to register their handlers with the CollisionDispatch before calling this function.
        // If you implement your own default material (PhysicsMaterial::sDefault) make sure to initialize it before this function or else this function will create one for you.
        RegisterTypes();

    }


    inline static std::once_flag initFlag;
    
};


} //namespace ospjolt

