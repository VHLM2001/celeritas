//----------------------------------*-C++-*----------------------------------//
// Copyright 2021-2024 UT-Battelle, LLC, and other Celeritas developers.
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)
//---------------------------------------------------------------------------//
//! \file orange/surf/detail/InvoluteSolver.hh
//---------------------------------------------------------------------------//
#pragma once

#include <cmath>
#include <iostream>
#include <vector>

#include "corecel/Constants.hh"
#include "corecel/Types.hh"
#include "corecel/cont/Array.hh"
#include "corecel/math/Algorithms.hh"
#include "orange/OrangeTypes.hh"

namespace celeritas
{
namespace detail
{
using constants::pi;
//---------------------------------------------------------------------------//
/*!
 * Find positive, real, nonzero roots for involute intersection function.
 *
 * The involute intersection equation \f[
   r_b * [v{cos(t+a)+tsin(t+a)} + u{sin(t+a)-tcos(t+a)}] + xv - yu = 0
 * \f]
 * has n solutions mathematically, but we only want solutions where t results
 * in a real, positive, and in the defined bounds.  Furthermore the equation is
 * subject to catastrophic roundoff due to floating point precision (see
 * \c Tolerance::sqrt_quadratic and the derivation in \c CylAligned ).
 *
 * \return An Intersections array where each item is a positive valid
 * intersection or the sentinel result \c no_intersection() .
 */
class InvoluteSolver
{
  public:
    //!@{
    //! \name Type aliases
    using Intersections = Array<real_type, 3>;

  public:
    // Construct Involute from parameters
    inline CELER_FUNCTION InvoluteSolver(real_type r_b_,
                                         real_type a_,
                                         real_type sign_,
                                         real_type tmin_,
                                         real_type tmax_);

    // Solve fully general case
    inline CELER_FUNCTION Intersections operator()(Real3 const& pos,
                                                   Real3 const& dir) const;

    //// ACCESSORS ////

    //! Get involute parameters
    CELER_FUNCTION real_type r_b() const { return r_b_; }
    CELER_FUNCTION real_type a() const { return a_; }
    CELER_FUNCTION real_type sign() const { return sign_; }

    //! Get bounds of the involute
    CELER_FUNCTION real_type tmin() const { return tmin_; }
    CELER_FUNCTION real_type tmax() const { return tmax_; }

  private:
    //// DATA ////
    // Involute parameters
    real_type r_b_;
    real_type a_;
    real_type sign_;

    // Bounds
    real_type tmin_;
    real_type tmax_;
};
//---------------------------------------------------------------------------//
// INLINE DEFINITIONS
//---------------------------------------------------------------------------//

//---------------------------------------------------------------------------//
/*!
 * Construct from involute parameters.
 */
CELER_FUNCTION InvoluteSolver::InvoluteSolver(
    real_type r_b, real_type a, real_type sign, real_type tmin, real_type tmax)
    : r_b_(r_b), a_(a), sign_(sign), tmin_(tmin), tmax_(tmax)
{
    CELER_EXPECT(r_b > 0);
    CELER_EXPECT(a > 0);
    CELER_EXPECT(std::fabs(tmax) < 2 * pi + std::fabs(tmin));
}

//---------------------------------------------------------------------------//

/*!
 * Find all roots for involute surfaces that are within the bounds and result
 * in positive distances. Performed by doing a Regular Falsi Iteration on the
 * root function, f(t) = r_b * [v{cos(a+t) + tsin(a+t)} + u{sin(a+t)
 * -tcos(a+t)}] + xv - yu where the Regular Falsi Iteration is given by: tc =
 * [ta*f(tb) - tb*f(ta)] / [f(tb) - f(ta)] where tc replaces the bound with the
 * same sign.
 */
CELER_FUNCTION auto
InvoluteSolver::operator()(Real3 const& pos,
                           Real3 const& dir) const -> Intersections
{
    // Flatten pos and dir in xyz and uv respectively
    real_type const x = pos[0];
    real_type const y = pos[1];
    real_type const z = pos[2];

    real_type const u = dir[0];
    real_type const v = dir[1];
    real_type const w = dir[2];

    // Lambda used for calculating the roots using Regular Flasi Iteration
    auto root = [](real_type t,
                   real_type x,
                   real_type y,
                   real_type u,
                   real_type v,
                   real_type r_b_,
                   real_type a_) {
        real_type a = u * std::sin(t + a_) - v * std::cos(t + a_);
        real_type b = t * (u * std::cos(t + a_) + v * std::sin(t + a_));
        real_type c = r_b_ * (a - b);
        return c + x * v - y * u;
    };
    /*
     * Results initalization
     */
    Intersections result;
    // Initial result vector.
    result = {no_intersection(), no_intersection(), no_intersection()};

    // Return result if particle is travelling along z-axis.
    if (u == 0 && v == 0)
    {
        return result;
    }

    // Initialize distance vecctor with root counter j
    Array<real_type, 3> dist;
    real_type j = 0;

    // Convertion constant for 2-D distance to 3-D distance

    real_type convert = std::sqrt(ipow<2>(v) + ipow<2>(u) + ipow<2>(w))
                        / std::sqrt(ipow<2>(v) + ipow<2>(u));

    /*
     * Define tolerances.
     * tolPoint gives the tolerence level for a point and is set to 1e-7,
     * account for the floating point error when performing square roots.
     * tolConv gives the tolerence for the Regular Falsi iteration,
     * ensuring that the root found produces a result that within 1e-8 of 0.
     */
    real_type const tolPoint = 1e-7;
    real_type const tolConv = 1e-8;

    // Check if particle is on a surface within tolerence, and add a 0 distance

    real_type const rxy2 = ipow<2>(x) + ipow<2>(y);
    real_type const tPoint = std::sqrt((rxy2 / (ipow<2>(r_b_))) - 1);
    real_type angle = tPoint + a_;
    real_type xInv = r_b_ * (std::cos(angle) + tPoint * std::sin(angle));
    real_type yInv = r_b_ * (std::sin(angle) - tPoint * std::cos(angle));
    if (std::fabs(x - xInv) < tolPoint && std::fabs(y - yInv) < tolPoint)
    {
        dist[j] = 0;
        j++;
    }

    // Line angle parameter

    real_type beta;

    if (u != 0)
    {
        // Standard method
        beta = std::atan(-v / u);
    }  // Edge case
    else if (-v < 0)
    {
        beta = pi * -0.5;
    }
    else
    {
        beta = pi * 0.5;
    }

    // Setting first interval bounds, needs to be done to ensure roots are
    // found
    real_type tLower;
    real_type tUpper;
    if (sign_ > 0)
    {
        tLower = 0;
        tUpper = beta - a_;
        // Move tUpper to be  first bound above 0
        tUpper += std::fmax(0, -std::floorf(tUpper / pi)) * pi;
    }
    else if (sign_ < 0)
    {
        tUpper = 0;
        tLower = beta - a_ + 2 * pi;
        // Move tLower to be first bound below 0
        tLower -= std::fmax(0, std::ceilf(tLower / pi)) * pi;
    }

    // Parameters that will be used in loop
    int i = 1;
    real_type talpha;
    real_type tbeta;
    real_type tgamma;
    real_type ftalpha;
    real_type ftbeta;
    real_type ftgamma;
    real_type t;
    real_type u2;
    real_type v2;
    real_type dot;

    // Iterate on roots
    while ((tLower < tmax_ && sign_ > 0)
           | (std::fabs(tUpper) < tmax_ && sign_ < 0))
    {
        // Set bounds on current iteration
        talpha = tLower;
        tbeta = tUpper;

        // Find value in root function
        ftalpha = root(talpha, x, y, u, v, r_b_, a_);
        ftbeta = root(tbeta, x, y, u, v, r_b_, a_);

        // If bounds exceed tmax break
        if ((talpha > tmax_ && sign_ == 1.0)
            | (std::fabs(tbeta) > std::fabs(tmax_) && sign_ == -1.0))
        {
            break;
        }

        // Only iterate when roots have different signs
        if ((0 < ftalpha) - (ftalpha < 0) != (0 < ftbeta) - (ftbeta < 0))
        {
            // Regula Falsi Iteration
            ftgamma = 1;
            while (std::fabs(ftgamma) >= tolConv)
            {
                // Iterate on root
                tgamma = (talpha * ftbeta - tbeta * ftalpha)
                         / (ftbeta - ftalpha);

                // Obtain root value of iterated root
                ftgamma = root(tgamma, x, y, u, v, r_b_, a_);

                // Update bounds with iterated root
                if ((0 < ftbeta) - (ftbeta < 0)
                    == (0 < ftgamma) - (ftgamma < 0))
                {
                    tbeta = tgamma;
                    ftbeta = root(tbeta, x, y, u, v, r_b_, a_);
                }
                else
                {
                    talpha = tgamma;
                    ftalpha = root(talpha, x, y, u, v, r_b_, a_);
                }
            }
            // Extract root and calculate point on involute
            t = tgamma;
            angle = t + a_;
            xInv = r_b_ * (std::cos(angle) + t * std::sin(angle));
            yInv = r_b_ * (std::sin(angle) - t * std::cos(angle));

            // Check if point is interval
            if (std::fabs(tgamma) >= std::fabs(tmin_) - tolPoint
                && std::fabs(tgamma) <= std::fabs(tmax_))
            {
                // Obatin direction to point on Involute
                u2 = xInv - x;
                v2 = yInv - y;

                // Dot with direction of particle
                dot = u * u2 + v * v2;
                // Obtain distance to point
                real_type newdist = std::sqrt(ipow<2>(u2) + ipow<2>(v2));
                // Only record distance if dot product is positive
                if (dot >= 0 && newdist > tolPoint)
                {
                    dist[j] = newdist;
                    j++;
                }
            }

            // Set next interval bounds
            if (sign_ == 1)
            {
                tLower = tUpper;
                tUpper += pi;
            }
            else if (sign_ == -1)
            {
                tUpper = tLower;
                tLower -= pi;
            }
        }
        else
        {
            // Incremet interval slowly until root is in interval
            // i used to slow down approach to next root
            if (sign_ == 1)
            {
                tLower = tUpper;
                tUpper += pi / i;
            }
            else if (sign_ == -1)
            {
                tUpper = tLower;
                tLower -= pi / i;
            }
            i++;
        }

        for (int k = 0; k < j; k++)
        {
            // Convert 2-D distances to 3-D distances
            result[k] = dist[k] * convert;
        }
    }
    return result;
}

}  // namespace detail
}  // namespace celeritas