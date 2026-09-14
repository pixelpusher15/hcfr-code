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

        Matrix errorMatrix(0.0, 3, 3);
        errorMatrix(0,0)=1.02; errorMatrix(0,1)=0.01;  errorMatrix(0,2)=-0.01;
        errorMatrix(1,0)=0.00; errorMatrix(1,1)=0.99;  errorMatrix(1,2)=0.02;
        errorMatrix(2,0)=0.01; errorMatrix(2,1)=-0.02; errorMatrix(2,2)=1.03;

        ColorXYZ measuresRGBW[4];
        for ( int i = 0; i < 4; i++ )
            measuresRGBW[i] = ColorXYZ(errorMatrix * referencesRGBW[i]);

        Matrix rawMatrix[3], calMatrix[3];
        ComputeBodnerThreeMatrices(measuresRGBW, referencesRGBW, rawMatrix, calMatrix);

        // A raw reading strictly inside the rgw sub-gamut (positive weights on
        // R, G and W only) should be corrected using sub-gamut 0's matrix, and
        // therefore reproduce the same weighting of the *reference* R, G, W
        // primaries.
        ColorXYZ rawInsideRGW( measuresRGBW[0]*0.5 + measuresRGBW[1]*0.3 + measuresRGBW[3]*0.2 );
        ColorXYZ expected( referencesRGBW[0]*0.5 + referencesRGBW[1]*0.3 + referencesRGBW[3]*0.2 );
        ColorXYZ actual = SelectAndApplyBodnerMatrix(rawInsideRGW, rawMatrix, calMatrix);

        for ( int c = 0; c < 3; c++ )
            CPPUNIT_ASSERT_DOUBLES_EQUAL( expected[c], actual[c], 0.000000001 );

        // Likewise for a point strictly inside the gbw sub-gamut.
        ColorXYZ rawInsideGBW( measuresRGBW[1]*0.4 + measuresRGBW[2]*0.4 + measuresRGBW[3]*0.2 );
        ColorXYZ expectedGBW( referencesRGBW[1]*0.4 + referencesRGBW[2]*0.4 + referencesRGBW[3]*0.2 );
        ColorXYZ actualGBW = SelectAndApplyBodnerMatrix(rawInsideGBW, rawMatrix, calMatrix);

        for ( int c = 0; c < 3; c++ )
            CPPUNIT_ASSERT_DOUBLES_EQUAL( expectedGBW[c], actualGBW[c], 0.000000001 );
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
};

CPPUNIT_TEST_SUITE_REGISTRATION(THIS_TEST_CASE);
