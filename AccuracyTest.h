/////////////////////////////////////////////////////////////////////////////
// AccuracyTest.h: headless "/accuracytest" self-test entry point.
//
// ColorHCFR.exe /accuracytest [quick] [report.txt]
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
//
// bQuick runs a reduced white/grid subset (~1/3 the combos) for fast
// iteration. It is a PRE-FLIGHT, not a substitute: it keeps every color
// space, transfer function, generator and intensity, and it still fires every
// known-fail entry - so its exit code means the same thing - but it drops the
// DCI white and the 10-bit grids, where several modeling gaps are
// level-dependent. Run the full matrix before committing.
int RunAccuracyTest ( const char * pReportPath, bool bQuick = false );

#endif
