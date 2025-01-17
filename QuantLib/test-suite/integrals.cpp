/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2003 RiskMap srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include "integrals.hpp"
#include "utilities.hpp"
#include <ql/math/functional.hpp>
#include <ql/math/integrals/filonintegral.hpp>
#include <ql/math/integrals/segmentintegral.hpp>
#include <ql/math/integrals/simpsonintegral.hpp>
#include <ql/math/integrals/trapezoidintegral.hpp>
#include <ql/math/integrals/kronrodintegral.hpp>
#include <ql/math/integrals/gausslobattointegral.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/termstructures/volatility/abcd.hpp>
#include <ql/math/integrals/twodimensionalintegral.hpp>

#include <boost/lambda/bind.hpp>
#include <boost/lambda/lambda.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

namespace {

    Real tolerance = 1.0e-6;

    template <class T>
    void testSingle(const T& I, const std::string& tag,
                    const boost::function<Real (Real)>& f,
                    Real xMin, Real xMax, Real expected) {
        Real calculated = I(f,xMin,xMax);
        if (std::fabs(calculated-expected) > tolerance) {
            BOOST_FAIL(std::setprecision(10)
                       << "integrating " << tag
                       << "    calculated: " << calculated
                       << "    expected:   " << expected);
        }
    }

    template <class T>
    void testSeveral(const T& I) {
        testSingle(I, "f(x) = 1",
                   constant<Real,Real>(1.0), 0.0, 1.0, 1.0);
        testSingle(I, "f(x) = x",
                   identity<Real>(),           0.0, 1.0, 0.5);
        testSingle(I, "f(x) = x^2",
                   square<Real>(),             0.0, 1.0, 1.0/3.0);
        testSingle(I, "f(x) = sin(x)",
                   std::ptr_fun<Real,Real>(std::sin), 0.0, M_PI, 2.0);
        testSingle(I, "f(x) = cos(x)",
                   std::ptr_fun<Real,Real>(std::cos), 0.0, M_PI, 0.0);
        testSingle(I, "f(x) = Gaussian(x)",
                   NormalDistribution(), -10.0, 10.0, 1.0);
        testSingle(I, "f(x) = Abcd2(x)",
                   AbcdSquared(0.07, 0.07, 0.5, 0.1, 8.0, 10.0), 5.0, 6.0,
                   AbcdFunction(0.07, 0.07, 0.5, 0.1).covariance(5.0, 6.0, 8.0, 10.0));
    }

}


void IntegralTest::testSegment() {
    BOOST_TEST_MESSAGE("Testing segment integration...");
    testSeveral(SegmentIntegral(10000));
}

void IntegralTest::testTrapezoid() {
    BOOST_TEST_MESSAGE("Testing trapezoid integration...");
    testSeveral(TrapezoidIntegral<Default>(tolerance, 10000));
}

void IntegralTest::testMidPointTrapezoid() {
    BOOST_TEST_MESSAGE("Testing mid-point trapezoid integration...");
    testSeveral(TrapezoidIntegral<MidPoint>(tolerance, 10000));
}

void IntegralTest::testSimpson() {
    BOOST_TEST_MESSAGE("Testing Simpson integration...");
    testSeveral(SimpsonIntegral(tolerance, 10000));
}

void IntegralTest::testGaussKronrodAdaptive() {
    BOOST_TEST_MESSAGE("Testing adaptive Gauss-Kronrod integration...");
    Size maxEvaluations = 1000;
    testSeveral(GaussKronrodAdaptive(tolerance, maxEvaluations));
}

void IntegralTest::testGaussLobatto() {
    BOOST_TEST_MESSAGE("Testing adaptive Gauss-Lobatto integration...");
    Size maxEvaluations = 1000;
    testSeveral(GaussLobattoIntegral(maxEvaluations, tolerance));
}

void IntegralTest::testGaussKronrodNonAdaptive() {
    BOOST_TEST_MESSAGE("Testing non-adaptive Gauss-Kronrod integration...");
    Real precision = tolerance;
    Size maxEvaluations = 100;
    Real relativeAccuracy = tolerance;
    GaussKronrodNonAdaptive gaussKronrodNonAdaptive(precision, maxEvaluations,
                                                    relativeAccuracy);
    testSeveral(gaussKronrodNonAdaptive);
}

void IntegralTest::testTwoDimensionalIntegration() {
    BOOST_TEST_MESSAGE("Testing two dimensional adaptive "
                       "Gauss-Lobatto integration...");

    const Size maxEvaluations = 1000;
    const Real calculated = TwoDimensionalIntegral(
        boost::shared_ptr<Integrator>(
            new TrapezoidIntegral<Default>(tolerance, maxEvaluations)),
        boost::shared_ptr<Integrator>(
            new TrapezoidIntegral<Default>(tolerance, maxEvaluations)))(
        std::multiplies<Real>(),
        std::make_pair(0.0, 0.0), std::make_pair(1.0, 2.0));

    const Real expected = 1.0;
    if (std::fabs(calculated-expected) > tolerance) {
        BOOST_FAIL(std::setprecision(10)
                   << "two dimensional integration: "
                   << "\n    calculated: " << calculated
                   << "\n    expected:   " << expected);
    }
}

void IntegralTest::testFolinIntegration() {
    BOOST_TEST_MESSAGE("Testing Folin's integral formulae...");

    // Examples taken from
    // http://www.tat.physik.uni-tuebingen.de/~kokkotas/Teaching/Num_Methods_files/Comp_Phys5.pdf
    const Size nr[] = { 4, 8, 16, 128, 256, 1024, 2048 };
    const Real expected[] = { 4.55229440e-5,4.72338540e-5, 4.72338540e-5,
                              4.78308678e-5,4.78404787e-5, 4.78381120e-5,
                              4.78381084e-5};

    const Real t = 100;
    const Real o = M_PI_2/t;

    const boost::function<Real(Real)> sineF = boost::lambda::bind(
        std::ptr_fun<Real,Real>(std::exp), -0.5*(boost::lambda::_1 - o));
    const boost::function<Real(Real)> cosineF = boost::lambda::bind(
        std::ptr_fun<Real,Real>(std::exp), -0.5*boost::lambda::_1);

    const Real tol = 1e-12;

    for (Size i=0; i < LENGTH(nr); ++i) {
        const Size n = nr[i];
        const Real calculatedCosine
            = FilonIntegral(FilonIntegral::Cosine, t, n)(cosineF,0,2*M_PI);
        const Real calculatedSine
            = FilonIntegral(FilonIntegral::Sine, t, n)
                (sineF, o,2*M_PI + o);

        if (std::fabs(calculatedCosine-expected[i]) > tol) {
            BOOST_FAIL(std::setprecision(10)
                << "Filon Cosine integration failed: "
                << "\n    calculated: " << calculatedCosine
                << "\n    expected:   " << expected[i]);
        }
        if (std::fabs(calculatedSine-expected[i]) > tol) {
            BOOST_FAIL(std::setprecision(10)
                << "Filon Sine integration failed: "
                << "\n    calculated: " << calculatedCosine
                << "\n    expected:   " << expected[i]);
        }
    }
}


test_suite* IntegralTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("Integration tests");
    suite->add(QUANTLIB_TEST_CASE(&IntegralTest::testSegment));
    suite->add(QUANTLIB_TEST_CASE(&IntegralTest::testTrapezoid));
    suite->add(QUANTLIB_TEST_CASE(&IntegralTest::testMidPointTrapezoid));
    suite->add(QUANTLIB_TEST_CASE(&IntegralTest::testSimpson));
    suite->add(QUANTLIB_TEST_CASE(&IntegralTest::testGaussKronrodAdaptive));
    suite->add(QUANTLIB_TEST_CASE(&IntegralTest::testGaussKronrodNonAdaptive));
    suite->add(QUANTLIB_TEST_CASE(&IntegralTest::testGaussLobatto));
    suite->add(QUANTLIB_TEST_CASE(&IntegralTest::testTwoDimensionalIntegration));
    suite->add(QUANTLIB_TEST_CASE(&IntegralTest::testFolinIntegration));
    return suite;
}

