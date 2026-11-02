#include "ArgyllMeterWrapper.h"
#include "SpectralSample.h"
#include <stdexcept>
#include <cppunit/config/SourcePrefix.h>
#include <cppunit/extensions/HelperMacros.h>
#include <map>
#include <fstream>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <string>

// this is not really a unit test
// but really a description of how to use the interface
// of the class and a test harness if no meter is attached it passes

#define THIS_TEST_CASE ArgyllMeterWrapperTestCase

class THIS_TEST_CASE : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE(THIS_TEST_CASE);
    CPPUNIT_TEST( autoDetectMeter );
    CPPUNIT_TEST( correlationCsvRoundTrip );
    CPPUNIT_TEST( correlationCsvShortRowThrows );
    CPPUNIT_TEST( correlationCsvNormIsOne );
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp()
    {
    }

protected:
    // Correlation CSV -> SpectralSample -> .ccss -> back. Exercises
    // BOM tolerance, trailing-comma tolerance, and empty-row dropping, and that
    // the result survives an Argyll write/read round-trip.
    void correlationCsvRoundTrip()
    {
        const char* csvPath = "cs_roundtrip_tmp.csv";
        const char* ccssPath = "cs_roundtrip_tmp.ccss";
        {
            std::ofstream f(csvPath, std::ios::binary);
            f << "\xEF\xBB\xBF";                       // UTF-8 BOM on the first line
            for (int r = 0; r < 4; ++r)
            {
                double peak = 400.0 + r * 80.0;       // 4 distinct "colors"
                for (int i = 0; i < 401; ++i)
                {
                    double wl = 380.0 + i;
                    double v = std::exp(-((wl-peak)*(wl-peak)) / (2*20.0*20.0));
                    f << v << (i < 400 ? "," : "");
                }
                f << ", \n";                          // trailing comma + space
            }
            for (int i = 0; i < 401; ++i)             // an all-zero row (must be dropped)
                f << "0" << (i < 400 ? "," : "");
            f << "\n";
        }

        SpectralSample ss;
        CPPUNIT_ASSERT( ss.createFromCorrelationCSV(csvPath) );
        CPPUNIT_ASSERT_EQUAL( 4, ss.getNumSamples() );          // empty row dropped

        CPPUNIT_ASSERT( ss.Write(ccssPath) );
        SpectralSample ss2;
        CPPUNIT_ASSERT( ss2.Read(ccssPath) );
        CPPUNIT_ASSERT_EQUAL( 4, ss2.getNumSamples() );         // survives write/read

        remove(csvPath);
        remove(ccssPath);
    }

    // nRows full 401-value spectra, shaped as correlationCsvRoundTrip writes them.
    static void writeGaussianRows(std::ofstream& f, int nRows)
    {
        for (int r = 0; r < nRows; ++r)
        {
            double peak = 400.0 + r * 80.0;
            for (int i = 0; i < 401; ++i)
            {
                double wl = 380.0 + i;
                f << std::exp(-((wl-peak)*(wl-peak)) / (2*20.0*20.0)) << (i < 400 ? "," : "");
            }
            f << "\n";
        }
    }

    // A row of 81 values is a 5 nm file, not a 1 nm spectrum that stops early.
    // Zero-padded it loaded as 380-460 nm and corrected every reading silently;
    // createFromCorrelationCSV must refuse the file instead.
    void correlationCsvShortRowThrows()
    {
        const char* csvPath = "cs_shortrow_tmp.csv";
        {
            std::ofstream f(csvPath, std::ios::binary);
            CPPUNIT_ASSERT( f.is_open() );
            writeGaussianRows(f, 4);
            for (int i = 0; i < 81; ++i)
                f << 0.5 << (i < 80 ? "," : "");
            f << "\n";
        }
        // The file must be there: a "Cannot open" throw would satisfy a bare
        // any-logic_error check for the wrong reason.
        {
            std::ifstream probe(csvPath, std::ios::binary);
            CPPUNIT_ASSERT( probe.is_open() );
        }
        bool threw = false;
        std::string what;
        try
        {
            SpectralSample ss;
            ss.createFromCorrelationCSV(csvPath);
        }
        catch (std::logic_error& e)
        {
            threw = true;
            what = e.what();
        }
        remove(csvPath);
        CPPUNIT_ASSERT( threw );
        CPPUNIT_ASSERT( what.find("fewer than 401 values") != std::string::npos );
    }

    // Argyll divides a spectrum by its norm wherever it integrates it through
    // getval_xspec (xspect.c), but the Spyder calibration sums the sensor side with
    // the unnormalized value_xspect (spyd2.c), so a set whose norm is not 1.0 is
    // inconsistent there; Argyll's own .ccss carry SPECTRAL_NORM 1.0. The writer
    // stores samples[0].norm verbatim (ccss.c), so read it back from the file.
    void correlationCsvNormIsOne()
    {
        const char* csvPath = "cs_norm_tmp.csv";
        const char* ccssPath = "cs_norm_tmp.ccss";
        {
            std::ofstream f(csvPath, std::ios::binary);
            writeGaussianRows(f, 4);
        }
        SpectralSample ss;
        CPPUNIT_ASSERT( ss.createFromCorrelationCSV(csvPath) );
        CPPUNIT_ASSERT( ss.Write(ccssPath) );

        double norm = -1.0;
        {
            std::ifstream in(ccssPath);
            std::string line;
            while (std::getline(in, line))
            {
                if (line.compare(0, 14, "SPECTRAL_NORM ") == 0)
                {
                    size_t q = line.find('"');
                    if (q != std::string::npos)
                        norm = atof(line.c_str() + q + 1);
                }
            }
        }
        remove(csvPath);
        remove(ccssPath);
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 1.0, norm, 0.0 );
    }

protected:

    // show how calibration works
    // we need to keep showing the user instructions
    // and then allowing the user to control when the calibration function
    // is called. On exit the meter should be calibrated
    void doCalibration(ArgyllMeterWrapper& meter)
    {
        ArgyllMeterWrapper::eMeterState state(ArgyllMeterWrapper::NEEDS_MANUAL_CALIBRATION);
        while(state != ArgyllMeterWrapper::READY)
        {
            std::string meterInstructions(meter.getCalibrationInstructions(FALSE));
            if(meterInstructions.empty())
            {
                break;
            }
            std::cout << meterInstructions << std::endl;
            // would normally pause here
            state = meter.calibrate();
            if(state == ArgyllMeterWrapper::INCORRECT_POSITION)
            {
                std::cout << meter.getIncorrectPositionInstructions() << std::endl;
                // would normally pause here
            }
        }
    }

    // demonstrate how to open a meter, calibrate it
    // and then take a reading coping with all possible
    // normal returns from the functions
    // the sections showing where to pause would be where
    // the UI is expected to show the relevant text and allow the
    // user to indicate when they are ready to move to the next step
    void autoDetectMeter()
    {
        std::string errorDescription;
        ArgyllMeterWrapper::ArgyllMeterWrappers meters = ArgyllMeterWrapper::getDetectedMeters(errorDescription);
        if(meters.size() == 0)
        {
            if(!errorDescription.empty())
            {
                std::cout << errorDescription << std::endl;
            }
            return;
        }
        ArgyllMeterWrapper& meter = *meters[0];

        if(!meter.connectAndStartMeter(errorDescription, ArgyllMeterWrapper::DISPLAY, "Default", true, 0.0, false) )
        {
            std::cout << errorDescription << std::endl;
            return;
        }

        std::cout << meter.getMeterName() << " meter found" << std::endl;

        if(meter.doesMeterSupportCalibration())
        {
            doCalibration(meter);
        }
        ArgyllMeterWrapper::eMeterState state(ArgyllMeterWrapper::NEEDS_MANUAL_CALIBRATION);
        while(state != ArgyllMeterWrapper::READY)
        {
            state = meter.takeReading("Default");
            if(state == ArgyllMeterWrapper::NEEDS_MANUAL_CALIBRATION)
            {
                doCalibration(meter);
            }
            if(state == ArgyllMeterWrapper::INCORRECT_POSITION)
            {
                std::cout << meter.getIncorrectPositionInstructions() << std::endl;
                // would normally pause here
            }
        }
        CColor reading(meter.getLastReading());
        std::cout << reading << std::endl;
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(THIS_TEST_CASE);
