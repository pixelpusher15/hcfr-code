/////////////////////////////////////////////////////////////////////////////
// AccuracyTest.h: headless "/accuracytest" self-test entry point.
//
// ColorHCFR.exe /accuracytest [report.txt]
//
// Runs the reference-vs-simulated-sensor accuracy matrix (see
// AccuracyTest.cpp for the full description) and exits the process with
// code 0 (all combos passed) or 1 (unexpected failures - see the report).
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_ACCURACYTEST_H__INCLUDED_)
#define AFX_ACCURACYTEST_H__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif

// pReportPath may be NULL: defaults to "accuracytest_report.txt" in the
// current directory. Returns the process exit code (0 pass / 1 fail).
int RunAccuracyTest ( const char * pReportPath );

#endif
