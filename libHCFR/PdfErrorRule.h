/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2012 Association Homecinema Francophone.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////
//
//  This file is subject to the terms of the GNU General Public License as
//  published by the Free Software Foundation.  A copy of this license is
//  included with this software distribution in the file COPYING.htm. If you
//  do not have a copy, you may obtain a copy by writing to the Free
//  Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details
/////////////////////////////////////////////////////////////////////////////
//  Author(s):
//  PhoenixF15E
/////////////////////////////////////////////////////////////////////////////

// PdfErrorRule.h: what a libharu error during the PDF report export means to
// the user. Header only, no MFC, so ColorMathTest (T63) checks the same
// function Export.cpp's error_handler calls.
//
// libharu reports an error as two numbers (libharu/include/hpdf_error.h):
// error_no, the library's own code, and detail_no. Export.cpp hands libharu
// three kinds of files:
// - the report itself, opened for writing by HPDF_SaveToFile. That is where
//   the errors the user can act on come from;
// - the logo, %APPDATA%\color\logo.png, which is optional;
// - the graph images, %APPDATA%\color\temp.png, which the export writes just
//   before it embeds each one.
// The two image kinds never reach this function: Export.cpp checks each file
// is there before asking libharu to load it, and an error while one loads is
// silent (the image is left out). So a file error here is the report's.
//
// The file codes, from libharu/src/hpdf_streams.c:
// - HPDF_FILE_OPEN_ERROR (0x1017): fopen failed. detail_no is the C errno
//   (HPDF_FileWriter_New, HPDF_FileReader_New). With the Microsoft C runtime,
//   EACCES (13) is both "access denied" and a sharing violation, which is what
//   a PDF viewer holding the file open gives; ENOENT (2) is a path that does
//   not exist (the folder is gone).
// - HPDF_FILE_IO_ERROR (0x1016): a write to the open file failed. detail_no
//   is ferror(), a flag, not an errno, so it says nothing about the cause.

#if !defined(PDFERRORRULE_H_INCLUDED)
#define PDFERRORRULE_H_INCLUDED

#include <errno.h>
#include "../libharu/include/hpdf_error.h"

namespace PdfErrorRule
{
    enum Cause
    {
        OutputInUse,            // open in another program (a PDF viewer), or read-only
        OutputFolderMissing,    // the folder the report goes to does not exist
        OutputCannotOpen,       // fopen refused the report for any other reason
        OutputWriteFailed,      // the report was open, a write to it failed
        OutOfMemory,
        Other                   // anything else: the generic text and both codes
    };

    // errorNo and detailNo exactly as libharu passes them to the error
    // handler; savingReport is true while HPDF_SaveToFile runs, the only
    // place the report file is opened.
    inline Cause Classify ( unsigned long errorNo, unsigned long detailNo, bool savingReport )
    {
        if ( errorNo == HPDF_FAILD_TO_ALLOC_MEM )
            return OutOfMemory;
        if ( ! savingReport )
            return Other;
        if ( errorNo == HPDF_FILE_OPEN_ERROR )
        {
            if ( detailNo == EACCES )
                return OutputInUse;
            if ( detailNo == ENOENT )
                return OutputFolderMissing;
            return OutputCannotOpen;
        }
        if ( errorNo == HPDF_FILE_IO_ERROR )
            return OutputWriteFailed;
        return Other;
    }
}

#endif // !defined(PDFERRORRULE_H_INCLUDED)
