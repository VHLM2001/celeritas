//----------------------------------*-C++-*----------------------------------//
// Copyright 2022 UT-Battelle, LLC, and other Celeritas developers.
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)
//---------------------------------------------------------------------------//
//! \file celeritas/geo/HeuristicGeoTestBase.hh
//---------------------------------------------------------------------------//
#pragma once

#include <string>
#include <vector>

#include "corecel/Assert.hh"
#include "corecel/Types.hh"
#include "corecel/cont/Span.hh"
#include "corecel/data/Collection.hh"

#include "../GlobalGeoTestBase.hh"
#include "HeuristicGeoData.hh"

namespace celeritas
{
template<template<Ownership, MemSpace> class S, MemSpace M>
class CollectionStateStore;

namespace test
{
//---------------------------------------------------------------------------//
/*!
 * Manage a "heuristic" stepper-like test that accumulates path length.
 */
class HeuristicGeoTestBase : public celeritas_test::GlobalGeoTestBase
{
  public:
    //!@{
    //! \name Type aliases
    template<MemSpace M>
    using StateStore = CollectionStateStore<HeuristicGeoStateData, M>;
    template<MemSpace M>
    using PathLengthRef
        = celeritas::Collection<real_type, Ownership::reference, M, VolumeId>;
    using SpanConstReal = Span<const real_type>;
    using SpanConstStr  = Span<const std::string>;
    //!@}

  public:
    //!@{
    //! Most param construction will not be used.
    SPConstParticle build_particle() override { CELER_ASSERT_UNREACHABLE(); }
    SPConstCutoff   build_cutoff() override { CELER_ASSERT_UNREACHABLE(); }
    SPConstPhysics  build_physics() override { CELER_ASSERT_UNREACHABLE(); }
    SPConstAction   build_along_step() override { CELER_ASSERT_UNREACHABLE(); }
    SPConstMaterial build_material() override { CELER_ASSERT_UNREACHABLE(); }
    SPConstGeoMaterial build_geomaterial() override
    {
        CELER_ASSERT_UNREACHABLE();
    }
    //!@}

    //// INTERFACE ////

    //! Construct problem-specific attributes (sampling box etc)
    virtual HeuristicGeoScalars build_scalars() const = 0;
    //! Get the number of steps to execute
    virtual size_type num_steps() const = 0;
    //! Build a list of volumes to compare average paths
    virtual SpanConstStr reference_volumes() const;
    //! Return the vector of path lengths mapped by sorted volume name
    virtual SpanConstReal reference_avg_path() const;

  protected:
    //// TEST EXECUTION ////

    //!@{
    //! Run tracks on device or host and compare the resulting path length
    void run_host(size_type num_states, real_type tolerance);
    void run_device(size_type num_states, real_type tolerance);
    //!@}

  private:
    //// DATA ////

    // Backend data for default reference_volumes implementation
    mutable std::vector<std::string> temp_str_;

    //// HELPER FUNCTIONS ////

    template<MemSpace M>
    HeuristicGeoParamsData<Ownership::const_reference, M> build_test_params();

    template<MemSpace M>
    std::vector<real_type>
    get_avg_path(PathLengthRef<M> path, size_type num_states) const;

    std::vector<real_type> get_avg_path_impl(const std::vector<real_type>& path,
                                             size_type num_states) const;
};

//---------------------------------------------------------------------------//
//! Run on device
void heuristic_test_launch(const DeviceCRef<HeuristicGeoParamsData>&,
                           const DeviceRef<HeuristicGeoStateData>&);

//---------------------------------------------------------------------------//
} // namespace test
} // namespace celeritas
