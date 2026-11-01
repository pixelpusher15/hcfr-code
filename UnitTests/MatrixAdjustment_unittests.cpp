#include "Color.h"
#include <stdexcept>
#include <cppunit/config/SourcePrefix.h>
#include <cppunit/extensions/HelperMacros.h>

#define THIS_TEST_CASE MatrixAdjustTestCase

class THIS_TEST_CASE : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE(THIS_TEST_CASE);
    CPPUNIT_TEST( MatrixAdjust );
    CPPUNIT_TEST( BodnerThreeMatrixRoundTrip );
    CPPUNIT_TEST( BodnerSubGamutSelection );
    CPPUNIT_TEST( RawXYZRecalibration );
    CPPUNIT_TEST( RawlessReadingsAcrossEntrances );
    CPPUNIT_TEST( RawlessReadingAfterSpectralLeave );
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp()
    {
    }

protected:
    void checkSamexy(const ColorxyY& ref, const ColorxyY& toTest)
    {
        CPPUNIT_ASSERT_DOUBLES_EQUAL( ref[0], toTest[0], 0.00000000000001 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( ref[1], toTest[1], 0.00000000000001 );
    }



    void MatrixAdjust()
    {
        // setup a test of the adjustment matrix
        // test is as given by kjgarrison on AVS thread
        ColorXYZ references[3] = {
                                    ColorXYZ(0.64, 0.33, 0.03),
                                    ColorXYZ(0.30, 0.60, 0.10),
                                    ColorXYZ(0.15, 0.06, 0.79)
                                 };
        ColorxyY whiteRefxyY(0.312712, 0.329008, 0.99);
        ColorXYZ whiteRef(whiteRefxyY);
        ColorXYZ measurements[3] = {
                                    ColorXYZ(0.633, 0.334, 0.033),
                                    ColorXYZ(0.294, 0.606, 0.100),
                                    ColorXYZ(0.150, 0.060, 0.790)
                                   };
        ColorxyY whiteTesyxyY(0.305712, 0.329008, 1.00);
        ColorXYZ whiteTest(whiteTesyxyY);

        Matrix convMatrix = ComputeConversionMatrix(measurements, references, whiteTest, whiteRef, false);

        // check the white point comes through exactly back to the ref
        // and that the other points the xy values are the same
        ColorXYZ testWhite(convMatrix * whiteTest);
        ColorxyY testWhitexyY(testWhite);
        checkSamexy(whiteRefxyY, testWhitexyY);
        CPPUNIT_ASSERT_DOUBLES_EQUAL( whiteRefxyY[2], testWhitexyY[2], 0.00000000000001 );

        ColorxyY testResults[3] = {
                                ColorxyY(ColorXYZ(convMatrix * measurements[0])),
                                ColorxyY(ColorXYZ(convMatrix * measurements[1])),
                                ColorxyY(ColorXYZ(convMatrix * measurements[2]))
                            };

        ColorxyY testExpected[3] = {
                                ColorxyY(references[0]),
                                ColorxyY(references[1]),
                                ColorxyY(references[2])
                            };

        for(int i(0); i < 3; ++i)
        {
            checkSamexy(testExpected[i], testResults[i]);
        }
    }

    void BodnerThreeMatrixRoundTrip()
    {
        // "True" RGBW primaries
        ColorXYZ referencesRGBW[4] = {
                                    ColorXYZ(0.64, 0.33, 0.03),
                                    ColorXYZ(0.30, 0.60, 0.10),
                                    ColorXYZ(0.15, 0.06, 0.79),
                                    ColorXYZ(0.31, 0.32, 0.99)
                                 };

        // A fixed linear colorimeter measurement error, applied to every primary,
        // so raw = errorMatrix * true. The three sub-gamut matrices Bodner's
        // method builds should each exactly undo this error for their own
        // primaries.
        Matrix errorMatrix(0.0, 3, 3);
        errorMatrix(0,0)=1.02; errorMatrix(0,1)=0.01;  errorMatrix(0,2)=-0.01;
        errorMatrix(1,0)=0.00; errorMatrix(1,1)=0.99;  errorMatrix(1,2)=0.02;
        errorMatrix(2,0)=0.01; errorMatrix(2,1)=-0.02; errorMatrix(2,2)=1.03;

        ColorXYZ measuresRGBW[4];
        for ( int i = 0; i < 4; i++ )
            measuresRGBW[i] = ColorXYZ(errorMatrix * referencesRGBW[i]);

        Matrix rawMatrix[3], calMatrix[3];
        ComputeBodnerThreeMatrices(measuresRGBW, referencesRGBW, rawMatrix, calMatrix);

        static const int idx[3][2] = { {0,1}, {1,2}, {0,2} };
        for ( int k = 0; k < 3; k++ )
        {
            ColorXYZ correctedP1(calMatrix[k] * measuresRGBW[idx[k][0]]);
            ColorXYZ correctedP2(calMatrix[k] * measuresRGBW[idx[k][1]]);
            ColorXYZ correctedW (calMatrix[k] * measuresRGBW[3]);

            for ( int c = 0; c < 3; c++ )
            {
                CPPUNIT_ASSERT_DOUBLES_EQUAL( referencesRGBW[idx[k][0]][c], correctedP1[c], 0.000000001 );
                CPPUNIT_ASSERT_DOUBLES_EQUAL( referencesRGBW[idx[k][1]][c], correctedP2[c], 0.000000001 );
                CPPUNIT_ASSERT_DOUBLES_EQUAL( referencesRGBW[3][c],         correctedW[c],  0.000000001 );
            }
        }
    }

    void BodnerSubGamutSelection()
    {
        ColorXYZ referencesRGBW[4] = {
                                    ColorXYZ(0.64, 0.33, 0.03),
                                    ColorXYZ(0.30, 0.60, 0.10),
                                    ColorXYZ(0.15, 0.06, 0.79),
                                    ColorXYZ(0.31, 0.32, 0.99)
                                 };

        // A DIFFERENT linear error per primary (and for white). With one error shared by
        // all four, every sub-gamut matrix equals its inverse and the three are identical,
        // so the result is the same whichever one is selected - a test built that way
        // passes with "always sub-gamut 0" and cannot see a wrong pick.
        Matrix err[4] = { Matrix(0.0, 3, 3), Matrix(0.0, 3, 3), Matrix(0.0, 3, 3), Matrix(0.0, 3, 3) };
        static const double e[4][9] = {
            { 1.02,  0.01, -0.01,   0.00, 0.99,  0.02,   0.01, -0.02, 1.03 },
            { 0.97,  0.03,  0.00,   0.02, 1.04, -0.01,  -0.01,  0.00, 0.98 },
            { 1.01, -0.02,  0.02,   0.01, 0.96,  0.03,   0.02,  0.01, 1.05 },
            { 1.00,  0.01,  0.01,  -0.01, 1.01,  0.00,   0.00,  0.02, 0.99 } };
        for ( int i = 0; i < 4; i++ )
            for ( int r = 0; r < 3; r++ )
                for ( int c = 0; c < 3; c++ )
                    err[i](r,c) = e[i][r*3+c];

        ColorXYZ measuresRGBW[4];
        for ( int i = 0; i < 4; i++ )
            measuresRGBW[i] = ColorXYZ(err[i] * referencesRGBW[i]);

        Matrix rawMatrix[3], calMatrix[3];
        ComputeBodnerThreeMatrices(measuresRGBW, referencesRGBW, rawMatrix, calMatrix);

        // The premise: the three correction matrices differ, so the pick matters.
        double diff01 = 0.0, diff12 = 0.0;
        for ( int r = 0; r < 3; r++ )
            for ( int c = 0; c < 3; c++ )
            {
                diff01 += fabs(calMatrix[0](r,c) - calMatrix[1](r,c));
                diff12 += fabs(calMatrix[1](r,c) - calMatrix[2](r,c));
            }
        CPPUNIT_ASSERT( diff01 > 1e-6 && diff12 > 1e-6 );

        // A raw reading strictly inside the rgw sub-gamut (positive weights on raw R, G
        // and W only) must be corrected by sub-gamut 0's matrix. By linearity that is
        // also the same weighting of the REFERENCE R, G, W; and the other two matrices
        // give something else, which is what makes a wrong pick visible.
        ColorXYZ rawInsideRGW( measuresRGBW[0]*0.5 + measuresRGBW[1]*0.3 + measuresRGBW[3]*0.2 );
        ColorXYZ expected( referencesRGBW[0]*0.5 + referencesRGBW[1]*0.3 + referencesRGBW[3]*0.2 );
        ColorXYZ byOwn( calMatrix[0] * rawInsideRGW );
        ColorXYZ byOther( calMatrix[1] * rawInsideRGW );
        ColorXYZ actual = SelectAndApplyBodnerMatrix(rawInsideRGW, rawMatrix, calMatrix);

        double offOther = 0.0;
        for ( int c = 0; c < 3; c++ )
        {
            CPPUNIT_ASSERT_DOUBLES_EQUAL( expected[c], byOwn[c],  0.000000001 );
            CPPUNIT_ASSERT_DOUBLES_EQUAL( expected[c], actual[c], 0.000000001 );
            offOther += fabs(byOther[c] - expected[c]);
        }
        CPPUNIT_ASSERT( offOther > 1e-6 );

        // Likewise for a point strictly inside the gbw sub-gamut (index 1).
        ColorXYZ rawInsideGBW( measuresRGBW[1]*0.4 + measuresRGBW[2]*0.4 + measuresRGBW[3]*0.2 );
        ColorXYZ expectedGBW( referencesRGBW[1]*0.4 + referencesRGBW[2]*0.4 + referencesRGBW[3]*0.2 );
        ColorXYZ byOtherGBW( calMatrix[0] * rawInsideGBW );
        ColorXYZ actualGBW = SelectAndApplyBodnerMatrix(rawInsideGBW, rawMatrix, calMatrix);

        double offOtherGBW = 0.0;
        for ( int c = 0; c < 3; c++ )
        {
            CPPUNIT_ASSERT_DOUBLES_EQUAL( expectedGBW[c], actualGBW[c], 0.000000001 );
            offOtherGBW += fabs(byOtherGBW[c] - expectedGBW[c]);
        }
        CPPUNIT_ASSERT( offOtherGBW > 1e-6 );
    }

    void RawXYZRecalibration()
    {
        CColor c;
        ColorXYZ raw(1.0, 2.0, 3.0);
        c.SetXYZValue(raw);
        CPPUNIT_ASSERT( !c.HasRawXYZValue() );

        c.SetRawXYZValue(raw);
        CPPUNIT_ASSERT( c.HasRawXYZValue() );

        Matrix m1(0.0,3,3); m1(0,0)=1.1; m1(1,1)=1.2; m1(2,2)=1.3;
        Matrix m2(0.0,3,3); m2(0,0)=0.9; m2(1,1)=0.8; m2(2,2)=0.7;

        // Recalibration always recomputes from the stored raw value, so
        // applying m1 and then m2 must give the same result as applying m2
        // alone to raw - NOT m2*m1*raw, which is what composing a delta matrix
        // onto an already-corrected value would produce.
        c.SetXYZValue(ColorXYZ(m1 * c.GetRawXYZValue()));
        c.SetXYZValue(ColorXYZ(m2 * c.GetRawXYZValue()));

        ColorXYZ expected(m2 * raw);
        ColorXYZ actual = c.GetXYZValue();
        for ( int i = 0; i < 3; i++ )
            CPPUNIT_ASSERT_DOUBLES_EQUAL( expected[i], actual[i], 0.000000000001 );
    }

    // RawlessReadingsState (Color.h) across the entrances that change the
    // correction. A reading with no raw value may take delta = new * inverse(current)
    // only while it embodies the sensor's CURRENT single matrix; each block below
    // is one way that stops being true, and the recalibration after it must leave
    // the reading alone (bComposeLegacy false) instead of composing.
    void RawlessReadingsAcrossEntrances()
    {
        // Fresh sensor, matrix -> matrix: the readings move with every walk.
        {
            RawlessReadingsState s;
            CPPUNIT_ASSERT( s.ComposeOntoRawless() );
            s.SingleMatrixWalked(0);
            CPPUNIT_ASSERT( s.ComposeOntoRawless() );
        }
        // (a) A spectral Browse answered "leave them": the sensor matrix goes to
        // identity and the method to the default one, the readings keep M. The
        // .thc load, Manual or Existing that follows must skip them, and after
        // skipping them they are still stranded.
        {
            RawlessReadingsState s;
            s.SpectralLeft();
            CPPUNIT_ASSERT( !s.ComposeOntoRawless() );
            s.SingleMatrixWalked(5);
            CPPUNIT_ASSERT( !s.ComposeOntoRawless() );
        }
        // (b) Bodner, then a spectral "strip": the strip walk skips the raw-less
        // readings (they still embody the pre-Bodner matrix) and leaves the method
        // at the default one; the next matrix recalibration must skip them too.
        {
            RawlessReadingsState s;
            s.BodnerInstalled();
            CPPUNIT_ASSERT( !s.ComposeOntoRawless() );
            s.SingleMatrixWalked(3);
            CPPUNIT_ASSERT( !s.ComposeOntoRawless() );
        }
        // (c) Bodner -> matrix -> matrix: the second matrix recalibration composed
        // newMatrix * inverse(C1) onto readings that never took C1.
        {
            RawlessReadingsState s;
            s.BodnerInstalled();
            s.SingleMatrixWalked(3);
            CPPUNIT_ASSERT( !s.ComposeOntoRawless() );
            s.SingleMatrixWalked(3);
            CPPUNIT_ASSERT( !s.ComposeOntoRawless() );
        }
        // DDE SetSensorMatrix writes a matrix without walking the readings.
        {
            RawlessReadingsState s;
            s.MatrixSetWithoutWalk();
            CPPUNIT_ASSERT( !s.ComposeOntoRawless() );
        }
        // A matrix walk that found no raw-less reading to skip leaves nothing
        // stranded: every reading now embodies the new matrix, so composing is
        // right again (for a reading edited by hand later, say).
        {
            RawlessReadingsState s;
            s.SpectralLeft();
            s.SingleMatrixWalked(0);
            CPPUNIT_ASSERT( s.ComposeOntoRawless() );
        }
        // A Bodner fit never re-arms it, even after a clean matrix walk: a reading
        // edited under Bodner embodies the sub-gamut correction, not the identity
        // single matrix the sensor then carries.
        {
            RawlessReadingsState s;
            s.SingleMatrixWalked(0);
            s.BodnerInstalled();
            CPPUNIT_ASSERT( !s.ComposeOntoRawless() );
        }
        // The value CSensor stores (Get) restores the same answer (Set).
        {
            RawlessReadingsState s, t;
            s.SpectralLeft();
            t.Set(s.Get());
            CPPUNIT_ASSERT( !t.ComposeOntoRawless() );
            s.SingleMatrixWalked(0);
            t.Set(s.Get());
            CPPUNIT_ASSERT( t.ComposeOntoRawless() );
        }
    }

    // Scenario (a) of the review with numbers: a document recorded before raw
    // capture, calibrated with M; a ccss Browsed and "leave them" answered; then
    // a .thc carrying L loaded (OnLoadCalibrationFile passes L as the delta). The
    // raw-less reading must stay M*r. Composing gave L*M*r, corrected twice.
    // The compose step is CColor::applyAdjustmentMatrix, the call the raw-less
    // branch of ReapplyAdjustmentMatrix (Measure.cpp) makes.
    void RawlessReadingAfterSpectralLeave()
    {
        ColorXYZ r(0.40, 0.50, 0.60);
        Matrix M(0.0,3,3); M(0,0)=1.10; M(1,1)=0.95; M(2,2)=1.20; M(0,1)=0.03;
        Matrix L(0.0,3,3); L(0,0)=0.90; L(1,1)=1.05; L(2,2)=0.85; L(1,0)=0.02;
        CColor c;
        c.SetXYZValue(ColorXYZ(M * r));
        CPPUNIT_ASSERT( !c.HasRawXYZValue() );

        RawlessReadingsState s;
        s.SpectralLeft();
        if ( s.ComposeOntoRawless() )
            c.applyAdjustmentMatrix(L);

        ColorXYZ expected(M * r);
        ColorXYZ actual = c.GetXYZValue();
        for ( int i = 0; i < 3; i++ )
            CPPUNIT_ASSERT_DOUBLES_EQUAL( expected[i], actual[i], 0.000000000001 );
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(THIS_TEST_CASE);
