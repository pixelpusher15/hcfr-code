#ifndef ICC_H
#define ICC_H

/* 
 * International Color Consortium Format Library (icclib)
 *
 * Author:  Graeme W. Gill
 * Date:    1999/11/29
 * Version: 3.2.0
 *
 * Copyright 1997 - 2024 Graeme W. Gill
 *
 * This material is licensed with an "MIT" free use license:-
 * see the License4.txt file in this directory for licensing details.
 */

/*
 *	TTBD:
 *
 *	~8 should better separate into implementation and public interface -
 *	i.e. all static functions should be implementation only etc.
 */

/* We can get some subtle errors if certain headers aren't included */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <limits.h>
#include <sys/types.h>

#if defined (NT)
# if !defined(WINVER) || WINVER < 0x0501
#  if defined(WINVER) 
#   undef WINVER
#  endif
#  define WINVER 0x0501
# endif
# if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0501
#  if defined(_WIN32_WINNT) 
#   undef _WIN32_WINNT
#  endif
#  define _WIN32_WINNT 0x0501
# endif
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <io.h>
#endif

#if defined(UNIX)
# include <unistd.h>
# include <pthread.h>
#endif

#ifdef __cplusplus
	extern "C" {
#endif

#undef CHECK_LOOKUP_PARTS			/* [und] Check icmLu4 3 and 5 part lookups against overall lu */

/* Version of icclib release */

#define ICCLIB_VERSION 0x030200			/* Format is MaMiBf */  
#define ICCLIB_VERSION_STR "3.2.0"

/*
 *  Note XYZ scaling to 1.0, not 100.0
 */

#undef ICC_DEBUG_MALLOC		/* Turns on partial support for filename and linenumber capture */

/* Make allowance for shared library use */
#ifdef ICCLIB_SHARED		/* Compiling or Using shared library version */
# ifdef ICCLIB_EXPORTS		/* Compiling shared library */
#  ifdef NT
#   define ICCLIB_API __declspec(dllexport)
#  endif /* NT */
# else						/* Using shared library */
#  ifdef NT
#   define ICCLIB_API __declspec(dllimport)
#   ifdef ICCLIB_DEBUG
#    pragma comment (lib, "icclibd.lib")
#   else
#    pragma comment (lib, "icclib.lib")
#   endif	/* DEBUG */
#  endif /* NT */
# endif
#else						/* Using static library */
# define ICCLIB_API	/* empty */
#endif

#if UINT_MAX < 0xffffffff
# error "icclib: integer size is too small, must be at least 32 bit"
#endif

/* Maximum file path length */
#ifndef MAXNAMEL
# define MAXNAMEL 1024
#endif

/* =========================================================== */
/* Platform specific primitive defines. */
/* This needs checking for each different platform. */
/* Using C99 and MSC covers a lot of cases, */
/* and the fallback default is pretty reliable with modern compilers and machines. */
/* Modern 32/64 bit compilers all have int = 32 bits. */
/* 64 bit MSWin is LLP64 == 32 bit long, while 64 bit OS X/Linux is LP64 == 64 bit long. */
/* so long shouldn't be used in any code outside these #defines.... */

/* Use __P64__ as cross platform 64 bit pointer #define */
#if defined(__LP64__) || defined(__ILP64__) || defined(__LLP64__) || defined(_WIN64)
# define __P64__ 1
#endif

#ifndef ORD32			/* If not defined elsewhere */

#if (__STDC_VERSION__ >= 199901L)	/* C99 */		\
 || defined(_STDINT_H_) || defined(_STDINT_H)		\
 || defined(_SYS_TYPES_H)

#include <stdint.h> 

#define INR8   int8_t			/* 8 bit signed */
#define INR16  int16_t			/* 16 bit signed */
#define INR32  int32_t			/* 32 bit signed */
#define INR64  int64_t			/* 64 bit signed - not used in icclib */
#define ORD8   uint8_t			/* 8 bit unsigned */
#define ORD16  uint16_t			/* 16 bit unsigned */
#define ORD32  uint32_t			/* 32 bit unsigned */
#define ORD64  uint64_t			/* 64 bit unsigned - not used in icclib */

#define PNTR intptr_t

#define PFSTPREC "z"        	/* size_t printf format precision specifier (ie %zu) */

#define PF64PREC "ll"			/* 64 bit printf format precision specifier */
#define CF64PREC(NNN) NNN##LL	/* 64 bit Constant precision specifier */

#ifndef ATTRIBUTE_NORETURN
# ifdef _MSC_VER
#  define ATTRIBUTE_NORETURN __declspec(noreturn)
# else
#  define ATTRIBUTE_NORETURN __attribute__((noreturn))
# endif
#endif

#else  /* !__STDC_VERSION__ */

#ifdef _MSC_VER

#define INR8   __int8				/* 8 bit signed */
#define INR16  __int16				/* 16 bit signed */
#define INR32  __int32				/* 32 bit signed */
#define INR64  __int64				/* 64 bit signed - not used in icclib */
#define ORD8   unsigned __int8		/* 8 bit unsigned */
#define ORD16  unsigned __int16		/* 16 bit unsigned */
#define ORD32  unsigned __int32		/* 32 bit unsigned */
#define ORD64  unsigned __int64		/* 64 bit unsigned - not used in icclib */

#define PNTR UINT_PTR				/* Integer that can hold a pointer */

#define PFSTPREC "I"                /* size_t printf format precision specifier (ie %Iu) */

#define PF64PREC "I64"				/* 64 bit printf format precision specifier */
#define CF64PREC(NNN) NNN##i64		/* 64 bit Constant precision specifier */

#define vsnprintf _vsnprintf
#define snprintf _snprintf

#ifndef ATTRIBUTE_NORETURN
# define ATTRIBUTE_NORETURN __declspec(noreturn)
#endif

#else  /* !_MSC_VER */

/* The following works on a lot of modern systems, including */
/* LLP64 and LP64 models, but won't work with ILP64 which needs int32 */

#define INR8   signed char		/* 8 bit signed */
#define INR16  signed short		/* 16 bit signed */
#define INR32  signed int		/* 32 bit signed */
#define ORD8   unsigned char	/* 8 bit unsigned */
#define ORD16  unsigned short	/* 16 bit unsigned */
#define ORD32  unsigned int		/* 32 bit unsigned */

#ifdef __GNUC__
# define INR64  long long			/* 64 bit signed - not used in icclib */
# define ORD64  unsigned long long	/* 64 bit unsigned - not used in icclib */
# define PF64PREC "ll"				/* 64 bit printf format precision specifier */
# define CF64PREC(NNN) NNN##LL		/* 64 bit Constant precision specifier */
#endif /* __GNUC__ */

#define PNTR unsigned long			/* Integer that can hold a pointer */ 

#ifndef ATTRIBUTE_NORETURN
# define ATTRIBUTE_NORETURN __attribute__((noreturn))
#endif

#endif /* !_MSC_VER */
#endif /* !__STDC_VERSION__ */
#endif /* !ORD32 */
 
typedef ORD32	icmUTF32;
typedef ORD16	icmUTF16;
typedef ORD8	icmUTF8;

/* =========================================================== */
#define ICM_SMALL_NUMBER    1.e-8		/* Matrix inversion limit */

#define FMLA_MIN 1e-22		/* Formula and Parametric numerical limit */
#define FMLA_MAX 1e+22

/* =========================================================== */

/* =========================================================== */
#include "iccV44.h"	/* ICC Version 4.4 definitions. */ 

/* The prefix icm is used for the native Machine */
/* equivalents of the ICC binary file structures, and other icclib */
/* specific definitions. */

/* =================================================== */

struct _icc;		/* Forward declaration */

/* =================================================== */
/* An error reporting context. */

/* Perhaps this could be expanded to save any error arguments and */
/* their type too, to allow multi-localisation. */
/* Make this fixed size to allow creaton on stack, struct copy etc. */

#define ICM_ERRM_SIZE 2000		/* Try and keep in sync with CGATS_ERRM_LENGTH */

typedef struct {
	int  c;            		/* Error code */
	char m[ICM_ERRM_SIZE];	/* Error message */
} icmErr;

/* Set the error code and error message in an icmErr context. */
/* Ignored if e == NULL */
/* Return err */
int icm_err_e(icmErr *e, int err, const char *format, ...);

/* Same as above using a va_list */
int icm_verr_e(icmErr *e, int err, const char *format, va_list vp);

/* most useful version... */
int icm_err(struct _icc *icp, int err, const char *format, ...);

/* Clear any error and message */
/* Ignored if e == NULL */
void icm_err_clear_e(icmErr *e);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* icclib Error codes */

/* General codes */
#define ICM_ERR_OK				0		/* No error */
#define ICM_ERR_MALLOC        	1		/* A malloc failed. (errm could be NULL) */

/* ICC low level codes */
#define ICM_ERR_FILE_OPEN		        0x101	/* Open failed */
#define ICM_ERR_FILE_SEEK		        0x102	/* Seek failed */
#define ICM_ERR_FILE_READ		        0x103	/* Read failed */
#define ICM_ERR_FILE_WRITE		        0x104	/* Write failed */
#define ICM_ERR_BUFFER_BOUND	        0x105	/* Attemp to access file outside buffer */
#define ICM_ERR_ENCODING		        0x106	/* Can't encode or decode value */
#define ICM_ERR_NOT_FOUND   	        0x107	/* A "not found" error */
#define ICM_ERR_DUPLICATE   	        0x108	/* Tag added is duplicate */
#define ICM_ERR_INTERNAL		        0x109	/* Internal inconsistency */
#define ICM_ERR_BADCS     		        0x10a	/* Unsupported colorspace */
#define ICM_ERR_BADCURVE   		        0x10b	/* Bad curve parameters */
#define ICM_ERR_PURPMISMATCH	        0x10c	/* Signature purposes don't match */
#define ICM_ERR_WFWENO	        		0x10d	/* set_WrFormatWarn_ENO out of space */
#define ICM_ERR_FOREIGN_TTYPE			0x110	/* icc_copy_ttype given foreign dest ttype */
#define ICM_ERR_UNIMP_TTYPE_COPY		0x111	/* icc_copy_ttype not implemented */
#define ICM_ERR_UNIMP_TTYPE_NOTSAME		0x112	/* icc_compare_ttype types not the same */
#define ICM_ERR_UNIMP_TTYPE_CMP			0x113	/* icc_compare_ttype not implemented */
#define ICM_ERR_TTYPE_SLZE				0x114	/* TagType no serialise() */
#define ICM_ERR_UNKNOWN_TTYPE			0x115	/* new_icmObject given unknown ttype */

/* Processing element error error codes */
#define ICM_ERR_PE_NOT_KNOWN			0x120	/* icmSigPe is unknown */
#define ICM_ERR_PE_UNKNOWN_TTYPE		0x123	/* icmSigPe ttype is unexpected or unknown */
#define ICM_ERR_NEW_PE_FAILED			0x124	/* icc_new_pe() returned NULL */
#define ICM_ERR_LUT2_NO_MX_CH			0x125	/* icmLut2 matrix has input channels != 3 */
#define ICM_ERR_LUT2_INOUT_CH			0x126	/* icmLut input and output channels don't match */

#define ICM_ERR_PECONT_BOUND			0x130	/* icmPeContainer index out of bounds */

#define ICM_ERR_PE_FMT_UNH_DEV			0x135	/* create_fmt() unhandled non-PCS */
#define ICM_ERR_PE_FMT_UNH_SNORM		0x136	/* create_fmt() unhandled src normalized */
#define ICM_ERR_PE_FMT_UNH_DNORM		0x137	/* create_fmt() unhandled dst normalized */
#define ICM_ERR_PE_FMT_UNEXP_CASE		0x138	/* create_fmt() unexpected case */

#define ICM_ERR_UNKNOWN_NORMSIG			0x139	/* new_icmNSig2NormPe got unhandled ColorSpace */

#define ICM_ERR_CRM_XFORMS_BADSIG		0x13a	/* icc_create_mono_xforms got unexpected sig */
#define ICM_ERR_CRM_XFORMS_BADTTYPE		0x13b	/* icc_create_mono_xforms got unexpected ttype */
#define ICM_ERR_CRM_XFORMS_BADNCHAN		0x13c	/* icc_create_mono_xforms got unxpctd no channels */
#define ICM_ERR_CRM_XFORMS_NOFWD		0x13d	/* icc_create_mono_xforms got no fwd table */

#define ICM_ERR_CRX_XFORMS_BADSIG		0x140	/* icc_create_matrix_xforms got unexpected sig */
#define ICM_ERR_CRX_XFORMS_BADTTYPE		0x141	/* icc_create_matrix_xforms got unexpected ttype */
#define ICM_ERR_CRX_XFORMS_BADNCHAN		0x142	/* icc_create_matrix_xforms got unxpctd no channels or output space */
#define ICM_ERR_CRX_XFORMS_NZCONST		0x142	/* icc_create_matrix_xforms got non-zero constant */
#define ICM_ERR_CRX_XFORMS_NOFWD		0x144	/* icc_create_matrix_xforms got no fwd table */
#define ICM_ERR_CRX_XFORMS_NOIMX		0x145	/* icc_create_matrix_xforms got can't invert mx */

#define ICM_ERR_CRL_XFORMS_BAD_CLUTRES	0x148	/* icc_create_lut_xforms got non-equal clutPoints */
#define ICM_ERR_CRL_XFORMS_BADSIG		0x149	/* icc_create_lut_xforms got unexpected sig */
#define ICM_ERR_CRL_XFORMS_BADTTYPE		0x14a	/* icc_create_lut_xforms got unexpected ttype */
#define ICM_ERR_CRL_XFORMS_C2MALLOC		0x14b	/* icc_create_lut_xforms got clut2 malloc */


#define ICM_ERR_APPEND_PES_INTERNAL		0x150	/* icmPeContainer_append_pes unhandled case */
#define ICM_ERR_SEARCH_PESEQ_INTERNAL	0x151	/* searcing icmPeSeq got unexpected icmPeSeq */

#define ICM_ERR_UNEX_MINMAXLU_FAIL		0x152	/* lookup of full range min/max to pch failed */

/* Format errors/warnings - plus sub-codes below */
#define ICM_ERR_RD_FORMAT    	        0x200	/* Read ICC file format error */
#define ICM_ERR_WR_FORMAT   	        0x300	/* Write ICC file format error */

/* Format/Transform sub-codes */
#define ICM_FMT_MASK					0xff	/* Format sub code mask */

#define ICM_FMT_TAGOLAP			        0x01	/* Tags overlap */
#define ICM_FMT_SIG2TYPE			    0x03	/* Type unexpected for Tag */

#define ICM_FMT_MAJV	    	        0x05	/* Header Major version error */
#define ICM_FMT_MINV	    	        0x06	/* Header Minor version error */
#define ICM_FMT_BFV		    	        0x07	/* Header Bugfix version error */
#define ICM_FMT_SCREEN	    	        0x08	/* Screen encodings error */
#define ICM_FMT_DEVICE	    	        0x09	/* Device attributes encodings error */
#define ICM_FMT_PROFFLGS	   	        0x0a	/* Profile Flags encodings error */
#define ICM_FMT_ASCBIN		   	        0x0b	/* Ascii/Binary encoding flag error */
#define ICM_FMT_PHCOL		   	        0x0c	/* Phosphor or Colorant sets encoding error */
#define ICM_FMT_GAMMA		   	        0x0d	/* Video Card Gamma Format encoding error */
#define ICM_FMT_TECH		   	        0x0e	/* Technology Signature error */
#define ICM_FMT_COLSP		   	        0x0f	/* Colorspace Signature error */
#define ICM_FMT_PROF		   	        0x10	/* Profile Class Signature error */
#define ICM_FMT_PLAT		   	        0x11	/* Platform Signature error */
#define ICM_FMT_RMGAM		   	        0x12	/* Reference Medium Gamut Signature error */
#define ICM_FMT_MESGEOM		   	        0x13	/* Measurement Geometry encoding error */
#define ICM_FMT_INTENT		   	        0x14	/* Rendering intent signature error */
#define ICM_FMT_SPSHAPE		   	        0x15	/* Spot Shape encoding error */
#define ICM_FMT_STOBS		   	        0x16	/* Standard Observer encoding error */
#define ICM_FMT_PRILL		   	        0x17	/* Predefined Illuminant encoding error */
#define ICM_FMT_LANG		   	        0x18	/* Language code error */
#define ICM_FMT_REGION		   	        0x19	/* Region code error */
#define ICM_FMT_MSDEVID		   	        0x1a	/* Microsoft platform device settings ID error */
#define ICM_FMT_MSMETYP		   	        0x1b	/* Microsoft platform media type error */
#define ICM_FMT_MSHALFTN				0x1c	/* Microsoft platform Halgtone encoding error */
#define ICM_FMT_RESCVUNITS				0x1d	/* ResponseCurve Measurement units Sig error */
#define ICM_FMT_COLPHOS					0x1e	/* Colorant and Phosphor Encoding error */
#define ICM_FMT_PARCVTYP				0x1f	/* Parametric curve type Encoding error */
#define ICM_FMT_DATETIME    	        0x20	/* Date & time endoding error */
#define ICM_FMT_FZ8STRING    	        0x21	/* Fixed length, null terminated 8 bit string */
#define ICM_FMT_VZ8STRING    	        0x22	/* Variable length, null terminated 8 bit string */
#define ICM_FMT_PARTIALEL  	            0x23	/* Tag has partial array element */
#define ICM_FMT_SHORTTAG  	            0x24	/* Tag is short of space allocated to it */ 

#define ICM_FMT_CHRMCHAN    	        0x30	/* Chromaticity channels mismatch */ 
#define ICM_FMT_CHRMENC    	            0x31	/* Chromaticity encoding mismatch */ 
#define ICM_FMT_CHRMVALS   	            0x33	/* Chromaticity encoding values error */ 

#define ICM_FMT_CORDCHAN    	        0x34	/* Colorant Order channels mismatch */ 
#define ICM_FMT_CORDVALS   	            0x35	/* Colorant Order values error */ 

#define ICM_FMT_DSSIZE   	            0x36	/* Device Settings setting size error */

#define ICM_FMT_LUICHAN   	            0x37	/* Lut input channels don't match colorspace */
#define ICM_FMT_LUOCHAN   	            0x38	/* Lut output channels don't match colorspace */
#define ICM_FMT_LUPURP 		  	        0x39	/* Lut unknown lut purpose */
#define ICM_FMT_LU8IOENT   	            0x3a	/* Lut8 input or output table entries not 256*/
#define ICM_FMT_LUIOENT   	            0x3b	/* Lut input or output table entries > 4096 */

#define ICM_FMT_DATA_FLAG  	            0x40	/* Data type has unknown flag value */
#define ICM_FMT_DATA_TERM  	            0x41	/* ASCII data is not null terminated */
#define ICM_FMT_TEXT_ANOTTERM           0x42	/* Text ASCII string is not nul terminate */
#define ICM_FMT_TEXT_ASHORT             0x43	/* Text ASCII string shorter than space */
#define ICM_FMT_TEXT_UERR           	0x44	/* Text unicode string translation error */
#define ICM_FMT_TEXT_SCRTERM            0x45	/* Text scriptcode string not terminated */

#define ICM_FMT_MATRIX_SCALE            0x48	/* Matrix profile values have wrong scale */

#define ICM_FMT_REQUIRED_TAG            0x49	/* Missing a required tag for this profile class */
#define ICM_FMT_UNEXPECTED_TAG          0x4a	/* A tag is unexpected for this profile class */
#define ICM_FMT_UNKNOWN_CLASS           0x4b	/* Profile class is unknown */

#define ICM_FMT_MLU_RECSIZED            0x4c	/* MultiLocalizedUnicode record size is != 12 */
#define ICM_FMT_MLU_MISSINGSTRING       0x4d	/* MultiLocalizedUnicode has zero sized string */

#define ICM_FMT_DICT_NAMELEN   	        0x53	/* Dict name is zero characters */
#define ICM_FMT_DICT_NAMEUNQ   	        0x54	/* Dict name is not unique */ 
#define ICM_FMT_DICTDISPTYPE   	        0x55	/* Dict display name/value wrong tagtype */ 

#define ICM_FMT_FLARERANGE   	        0x5a	/* Measurement flare out of range */

#define ICM_FMT_NCOL_NCHAN   	        0x5b	/* Named Color no. of chan doesn't match header */

#define ICM_FMT_VCGT_FORMAT   	        0x60	/* Unknown VideoCardGamma format */
#define ICM_FMT_VCGT_ENTRYSZ   	        0x61	/* Unknown VideoCardGamma table entry size */

#define ICM_FMT_FCS_ENC					0x66	/* Unknown Formula Curve Segment encoding */
#define ICM_FMT_CIIS_UNK				0x67	/* Unknown Colorimetric Intent Image State */
#define ICM_FMT_RMGS_UNK				0x68	/* Unknown Reference Medium Gamut signature */

#define ICM_FMT_PAR_TYPE_INVALID		0x69	/* Parent tagtype is invalid */ 
#define ICM_FMT_SUB_TYPE_INVALID		0x6a	/* Sub tagtype is invalid */
#define ICM_FMT_SUB_TYPE_UNKN			0x6b	/* Unknown sub-tagtype */ 
#define ICM_FMT_SUB_MISSING				0x6c	/* Sub-tag is missing */

#define ICM_FMT_CSET_SUBT				0x6e	/* icmPeCurveSet sub-tags are not correct. */

#define ICM_FMT_CURV_CTYPE				0x6f	/* icmPeCurve ctype is not correct. */
#define ICM_FMT_CURV_POIMATCH			0x70	/* icmPeCurve points don't match */
#define ICM_FMT_CURV_POINTS				0x71	/* icmPeCurve no. points too small */

#define ICM_FMT_CGRP_CHAN				0x72	/* icmPeCurveSegGroup in/out channel not 1 */
#define ICM_FMT_CGRP_BKPTS				0x73	/* icmPeCurveSegGroup breakpoints are decreasing */
#define ICM_FMT_CGRP_SUBT				0x74	/* icmPeCurveSegGroup sub-tags are not correct */
#define ICM_FMT_CGRP_IBRMM				0x75	/* icmPeCurveSegGroup inverted breakpoint mismatch  */
#define ICM_FMT_FMSG_CHAN				0x76	/* icmPeCurveFmlaSeg in/out channel not 1 */
#define ICM_FMT_FMSG_PARM				0x77	/* icmPeCurveFmlaSeg has bad parameter */

#define ICM_FMT_PACU_CHAN				0x78	/* icmPeParametricCurve in/out channel not 1 */
#define ICM_FMT_PACU_PARM				0x79	/* icmPeParametricCurve has bad parameter */

#define ICM_FMT_CURV_CHAN				0x7a	/* icmPeCurve in/out channel not 1 */

#define ICM_FMT_MATX_CHAN				0x7b	/* icmPeMatrix in/out channel not 3 */
#define ICM_FMT_MATX_CNST				0x7c	/* icmPeMatrix constant value != 0.0 */

#define ICM_FMT_CLUT_RES				0x7d	/* icmPeClut_check table res is < 2 */


#define ICM_FMT_A2B2A_CONFIG			0x86	/* AToB or BToA config is illegal */
#define ICM_FMT_B2A_PCS					0x87	/* BToA only legal if PCS is XYZ or Lab */

/* Any format error at or over this is fatal for the tag it happens in, irrespective */
/* of icmCFlagRdFormatWarn or icmCFlagWrFormatWarn being set. */
#define ICM_FMTF						0xf0	/* (fatal) */

#define ICM_FMTF_VRANGE			    	0xf0	/* A file value is out of range (fatal) */
#define ICM_FMTF_CLUT_LUTSIZE			0xf1	/* icmPeClut size overflowed (fatal) */
#define ICM_FMTF_CSET_CHAN				0xf2	/* icmPeCurveSet in/out channel mismatch (fatal) */
#define ICM_FMTF_CGRP_SEGS				0xf4	/* icmPeCurveSegGroup no. segs too small (fatal) */
#define ICM_FMTF_CURV_CTYPE				0xf5	/* icmPeCurve ctype illegal (fatal) */
#define ICM_FMTF_CURV_POINTS			0xf6	/* icmPeCurve no. points too small (fatal) */
#define ICM_FMTF_LUT2_BPV				0xf7	/* LutA2B/B2A bpv value illegal (fatal) */

/* Version errors/warnings - plus sub-codes below */
#define ICM_ERR_RD_VERSION    	        0x400	/* Read ICC file format error */
#define ICM_ERR_WR_VERSION   	        0x500	/* Write ICC file format error */

/* version sub-codes */
#define ICM_VER_SIGVERS				    0x01	/* Tag Sig not valid in profile version */
#define ICM_VER_TYPEVERS				0x02	/* TagType not valid in profile version */
#define ICM_VER_SIG2TYPEVERS		    0x03	/* Tag & TagType not valid comb. in prof. version */

/* Higher level error codes */
#define ICM_ERR_MAGIC_NUMBER	        0x801	/* ICC profile has bad magic number */
#define ICM_ERR_HEADER_VERSION	        0x802	/* Header has invalid version number */
#define ICM_ERR_HEADER_LENGTH	        0x803	/* Internal: Header is wrong legth */
#define ICM_ERR_UNKNOWN_VERSION	        0x804	/* ICC Version is not known */
#define ICM_ERR_UNKNOWN_COLORANT_ENUM	0x805	/* Unknown colorant enumeration */

#define ICM_ERR_GAMUT_INTENT			0x820	/* Intent is unexpected for Gamut table */


/* =========================================================== */
/* System Heap interface object. The defaults can be replaced */
/* for adaption to different system environments. */
/* Note that we guarantee certain semantics, such as */
/* malloc(0) != NULL, realloc(NULL, size) and free(NULL) working, */
/* was well as providing recalloc() */

#ifndef ICC_DEBUG_MALLOC

/* Heap allocator class interface definition */
#define ICM_ALLOC_BASE																		\
	/* Public: */																			\
																							\
	void *(*malloc)  (struct _icmAlloc *p, size_t size);									\
	void *(*realloc) (struct _icmAlloc *p, void *ptr, size_t size);							\
	void *(*calloc)  (struct _icmAlloc *p, size_t num, size_t size);						\
	void *(*recalloc)(struct _icmAlloc *p, void *ptr, size_t cnum, size_t csize,			\
	                                        size_t nnum, size_t nsize);						\
	void  (*free)    (struct _icmAlloc *p, void *ptr);										\
																							\
	/* Take a reference counted copy of the allocator */									\
	struct _icmAlloc *(*reference)(struct _icmAlloc *p);									\
																							\
	/* we're done with the allocator object */												\
	void (*del)(struct _icmAlloc *p);														\
																							\
	/* Private: */																			\
	int refcount;																			\

#else /* !ICC_DEBUG_MALLOC */

#define ICM_ALLOC_BASE																		\
	/* Public: */																			\
																							\
	void *(*dmalloc)  (struct _icmAlloc *p, size_t size, char *file, int line);				\
	void *(*drealloc) (struct _icmAlloc *p, void *ptr, size_t size, char *file, int line);	\
	void *(*dcalloc)  (struct _icmAlloc *p, size_t num, size_t size, char *file, int line);	\
	void *(*drecalloc)(struct _icmAlloc *p, void *ptr, size_t cnum, size_t csize,			\
	                                        size_t nnum, size_t nsize, char *file, int line); \
	void  (*dfree)    (struct _icmAlloc *p, void *ptr, char *file, int line);				\
																							\
	/* Take a reference counted copy of the allocator */									\
	struct _icmAlloc *(*reference)(struct _icmAlloc *p);									\
																							\
	/* we're done with the allocator object */												\
	void (*del)(struct _icmAlloc *p);														\
																							\
	/* Private: */																			\
	int refcount;																			\

#ifdef _ICC_IMP_	/* only inside implementation */
#define malloc( p, size )	    dmalloc( p, size, __FILE__, __LINE__ )
#define calloc( p, num, size )	dcalloc( p, num, size, __FILE__, __LINE__ )
#define realloc( p, ptr, size )	drealloc( p, ptr, size, __FILE__, __LINE__ )
#define recalloc( p, ptr, cnum, csize, nnum, size ) drecalloc( p, ptr, cnum, csize, nnum, size, __FILE__, __LINE__ )
#define free( p, ptr )	        dfree( p, ptr , __FILE__, __LINE__ )
#endif /* _ICC_IMP */

#endif /* ICC_DEBUG_MALLOC */

/* Common heap allocator interface class */
struct _icmAlloc {
	ICM_ALLOC_BASE
}; typedef struct _icmAlloc icmAlloc;

/* - - - - - - - - - - - - - - - - - - - - -  */

/* Implementation of heap class based on standard system malloc */
struct _icmAllocStd {
	ICM_ALLOC_BASE
}; typedef struct _icmAllocStd icmAllocStd;

/* Create a standard alloc object */
/* Note that this will fail if e has an error already set */
icmAlloc *new_icmAllocStd(icmErr *e);

/* =========================================================== */
/* System File interface object. The defaults can be replaced */
/* for adaption to different system environments. */

#define ICM_FILE_BASE																		\
	/* Public: */																			\
																							\
	/* Get the size of the file (Only valid for reading file). */							\
	size_t (*get_size) (struct _icmFile *p);												\
																							\
	/* Set current position to offset. Return 0 on success, nz on failure. */				\
	int    (*seek) (struct _icmFile *p, unsigned int offset);								\
																							\
	/* Read count items of size length. Return number of items successfully read. */ 		\
	size_t (*read) (struct _icmFile *p, void *buffer, size_t size, size_t count);			\
																							\
	/* write count items of size length. Return number of items successfully written. */ 	\
	size_t (*write)(struct _icmFile *p, void *buffer, size_t size, size_t count);			\
																							\
	/* printf to the file */																\
	int (*printf)(struct _icmFile *p, const char *format, ...);								\
																							\
	/* flush all write data out to secondary storage. Return nz on failure. */				\
	int (*flush)(struct _icmFile *p);														\
																							\
	/* Return the memory buffer. Error if not icmFileMem */									\
	int (*get_buf)(struct _icmFile *p, unsigned char **buf, size_t *len);					\
																							\
	/* Take a reference counted copy of the allocator */									\
	struct _icmFile *(*reference)(struct _icmFile *p);										\
																							\
	/* we're done with the file object, return nz on failure */								\
	int (*del)(struct _icmFile *p);															\
																							\
	/* Private: */																			\
	int refcount;																			\

/* Common file interface class */
struct _icmFile {
	ICM_FILE_BASE
}; typedef struct _icmFile icmFile;


/* - - - - - - - - - - - - - - - - - - - - -  */

/* These are avalailable if SEPARATE_STD is not defined: */

/* Implementation of file access class based on standard file I/O */
struct _icmFileStd {
	ICM_FILE_BASE

	/* Private: */
	icmAlloc *al;		/* Heap allocator reference */
	FILE     *fp;
	int   doclose;		/* nz if free should close fp */

	/* Private: */
	size_t size;    	/* Size of the file (For read) */

}; typedef struct _icmFileStd icmFileStd;

/* Create given a file name */
/* Note that this will fail if e has an error already set */
icmFile *new_icmFileStd_name(icmErr *e, char *name, char *mode);

/* Create given a (binary) FILE* */
/* Note that this will fail if e has an error already set */
icmFile *new_icmFileStd_fp(icmErr *e, FILE *fp);

/* Create given a file name and take allocator reference */
/* Note that this will fail if e has an error already set */
icmFile *new_icmFileStd_name_a(icmErr *e, char *name, char *mode, icmAlloc *al);

/* Create given a (binary) FILE* and take eallocator reference */
/* Note that this will fail if e has an error already set */
icmFile *new_icmFileStd_fp_a(icmErr *e, FILE *fp, icmAlloc *al);


/* - - - - - - - - - - - - - - - - - - - - -  */
/* Implementation of file access class based on a memory image */
/* The buffer is assumed to be allocated with the given heap allocator */
/* Pass base = NULL, length = 0 for no initial buffer */

struct _icmFileMem {
	ICM_FILE_BASE

	/* Private: */
	icmAlloc *al;		/* Heap allocator reference */
	int      del_buf;   /* NZ if memory file buffer should be deleted */
	unsigned char *start, *cur, *end, *aend;

}; typedef struct _icmFileMem icmFileMem;

/* Create a memory image file access class and take allocator reference */
/* Note that this will fail if e has an error already set */
icmFile *new_icmFileMem_a(icmErr *e, void *base, size_t length, icmAlloc *al);

/* Create a memory image file access class and take allocator reference */
/* and delete buffer when icmFile is deleted. */
icmFile *new_icmFileMem_ad(icmErr *e, void *base, size_t length, icmAlloc *al);

/* This is available if SEPARATE_STD is not defined: */

/* Create a memory image file access class */
icmFile *new_icmFileMem(icmErr *e, void *base, size_t length);

/* Create a memory image file access class */
/* and delete buffer when icmFile is deleted. */
icmFile *new_icmFileMem_d(icmErr *e, void *base, size_t length);

/* ================================= */
/* Some useful utilities: */

/* Default ICC profile file extension string */

#if defined (UNIX) || defined(__APPLE__)
#define ICC_FILE_EXT ".icc"
#define ICC_FILE_EXT_ND "icc"
#else
#define ICC_FILE_EXT ".icm"
#define ICC_FILE_EXT_ND "icm"
#endif

/* Return a string that represents a tag 32 bit value */
/* as a 4 character string or hex number. */
extern ICCLIB_API char *icmtag2str(unsigned int tag);

/* Return a tag signature created from a string */
extern ICCLIB_API unsigned int icmstr2tag(const char *str);

/* Utility to define a non-standard tag, arguments are 4 character constants */
#define icmMakeTag(A,B,C,D)	\
	(   (((ORD32)(ORD8)(A)) << 24)	\
	  | (((ORD32)(ORD8)(B)) << 16)	\
	  | (((ORD32)(ORD8)(C)) << 8)	\
	  |  ((ORD32)(ORD8)(D)) )

/* Return the number of channels for the given color space. Return 0 if unknown. */
extern ICCLIB_API unsigned int icmCSSig2nchan(icColorSpaceSignature sig);

#define CSSigType_PCS   0x0001		/* icSigXYZData or icSigLabData */
#define CSSigType_NDEV  0x0002		/* Not a device space (i.e. PCS, nPCS, Yxy, Luv etc.) */
#define CSSigType_DEV   0x0004		/* A device space (AKA N-component) */
#define CSSigType_NCOL  0x0008		/* An N-Color device space */

#define CSSigType_EXT   0x0010		/* icclib Extension */
#define CSSigType_gPCS  0x0020		/* full range or normalized PCS */
#define CSSigType_nPCS  0x0040		/* Normalised but not full range PCS */
#define CSSigType_gXYZ  0x0080		/* full range or normalized XYZ */
#define CSSigType_gLab  0x0100		/* full range or normalized Lab */

#define CSSigType_NORM  0x0200		/* Normalized/encoded space */

/* Return a mask classifying the color space */
extern ICCLIB_API unsigned int icmCSSig2type(icColorSpaceSignature sig);

/* ================================= */
/* Assumed constants                 */

#define MAX_CHAN 15		/* Maximum number of color channels */

/* ================================= */
/* tag and other compound structures */

typedef ORD32 icmSig;	/* Otherwise un-enumerated 4 byte signature */

/* ArgyllCMS Private tagSignature: */
/* Absolute to Media Relative Transformation Space matrix. */
/* Uses the icSigS15Fixed16ArrayType */
/* (Unit matrix for default ICC, Bradford matrix for Argll default) */
#define icmSigAbsToRelTransSpace ((icTagSignature) icmMakeTag('a','r','t','s'))


/* ArgyllCMS ICCLIB extensions */

/* Non-standard Color Space Signatures - will be incompatible outside icclib! */

/* A non-specific monochrome space */
#define icSig1colorData ((icColorSpaceSignature) icmMakeTag('1','C','L','R'))

/* A non-specific monochrome colorsync space */
#define icSigMch1Data ((icColorSpaceSignature) icmMakeTag('M','C','H','1'))


/* Y'u'v' perceptual CIE 1976 UCS diagram Yu'v' */
#define icmSigYuvData ((icColorSpaceSignature) icmMakeTag('Y','u','v',' '))

/* A modern take on Lab */
#define icmSigLptData ((icColorSpaceSignature) icmMakeTag('L','p','t',' '))


/* Pseudo Color Space Signatures - just used within icclib, not used in profiles. */
/* These represent encode/normalised full range values */

/* Pseudo named colorspace */
#define icmSigNamedData ((icColorSpaceSignature) icmMakeTag('N','a','m','e'))


/* Pseudo PCS colospace to signal 8 bit (u1.7) XYZ */
#define icmSigXYZ8Data ((icColorSpaceSignature) icmMakeTag('X','Y','Z','1'))

/* Pseudo PCS colospace to signal 16 bit (u1.15) XYZ */
#define icmSigXYZ16Data ((icColorSpaceSignature) icmMakeTag('X','Y','Z','2'))


/* Pseudo PCS colospace to signal 8 bit Lab */
#define icmSigLab8Data ((icColorSpaceSignature) icmMakeTag('L','a','b','8'))

/* Pseudo PCS colospace to signal V2 16 bit Lab */
#define icmSigLabV2Data ((icColorSpaceSignature) icmMakeTag('L','a','b','2'))


/* Pseudo device normalized encodings. These aren't actually defined by ICC... */
#define icmSigLuv16Data ((icColorSpaceSignature) icmMakeTag('L','u','v','2'))
#define icmSigYxy16Data ((icColorSpaceSignature) icmMakeTag('Y','x','y','2'))
#define icmSigYCbCr16Data ((icColorSpaceSignature) icmMakeTag('Y','C','b','2'))

#ifdef NEVER	/* Not fully implemented and not actually used */

/* A monochrome CIE L* space */
# define icmSigLData ((icColorSpaceSignature) icmMakeTag('L',' ',' ',' '))

/* A monochrome CIE Y space */
# define icmSigYData ((icColorSpaceSignature) icmMakeTag('Y',' ',' ',' '))


/* Pseudo PCS colospace to signal 8 bit (u1.7) Y */
# define icmSigY8Data ((icColorSpaceSignature) icmMakeTag('Y',' ',' ','1'))

/* Pseudo PCS colospace to signal 16 bit (u1.15) Y */
# define icmSigY16Data ((icColorSpaceSignature) icmMakeTag('Y',' ',' ','2'))


/* Pseudo PCS colospace to signal 8 bit L */
# define icmSigL8Data ((icColorSpaceSignature) icmMakeTag('L',' ',' ','8'))

/* Pseudo PCS colospace to signal V2 16 bit L */
# define icmSigLV2Data ((icColorSpaceSignature) icmMakeTag('L',' ',' ','2'))


#endif /* NEVER */


/* Alias for icSigColorantTableType found in LOGO profiles (Byte swapped clrt !) */ 
#define icmSigAltColorantTableType ((icTagTypeSignature)icmMakeTag('t','r','l','c'))

/* Non-standard Platform Signature for Linux and similiar */
#define icmSig_nix ((icPlatformSignature) icmMakeTag('*','n','i','x'))

/* Alias signature and tag type for shaper-monochrome transform */
#define icmSigShaperMono icSigGrayTRCTag
#define icmSigShaperMonoType icSigCurveType

/* Alias signature and tag type for RGB shaper-matrix transform, */
/* composed of icSigRedTRCTag, icSigGreenTRCTag, icSigBlueTRCTag, */
/* icSigRedColorantTag, icSigGreenColorantTag, icSigBlueColorantTag */
#define icmSigShaperMatrix icSigRedTRCTag
#define icmSigShaperMatrixType icSigCurveType

/* Tag signature, used to represent an unknown/not applicable */
#define icmSigUnknown ((icTagSignature) 0)

/* Tag Type signature, used to handle any tag that */
/* isn't handled with a specific type object. */
/* Also used in manufacturer & model header fields */
/* Old: #define icmSigUnknownType ((icTagTypeSignature)icmMakeTag('?','?','?','?')) */
#define icmSigUnknownType ((icTagTypeSignature) 0)

#define icmMaxOff 0xffffffff		/* An impossible offset */

typedef struct {
	ORD32 l;			/* High and low components of signed 64 bit */
	INR32 h;
} icmInt64;

typedef struct {
	ORD32 l,h;			/* High and low components of unsigned 64 bit */
} icmUInt64;

/* XYZ Number */
#ifdef NEVER
/* Old version */
typedef struct {
    double  X;
    double  Y;
    double  Z;
} icmXYZNumber;
#endif
#ifndef NEVER
/* Compatible new version (depends on anonomous unions) */
typedef struct {
	union {
		double XYZ[3];			/* New version */
		struct {
			double X, Y, Z;		/* For backwards compatibility */
		};
	};
} icmXYZNumber;
#endif
#ifdef NEVER
/* Incompatible new version */
typedef struct {
	double XYZ[3];
} icmXYZNumber;
#endif

/* CIE xy coordinate value */
typedef struct {
	double xy[2];
} icmxyCoordinate;

/* icmPositionNumber */
typedef struct {
	unsigned int	offset;	/* Offset to the data in bytes */
	unsigned int	size;	/* Size ofthe data in bytes */
} icmPositionNumber;

/* Response 16 number */
typedef struct {
	double	       deviceValue;	/* The device value in range 0.0 - 1.0 */
	double	       measurement;	/* The reading value */
} icmResponse16Number;

/* Date & time number */
struct _icmDateTimeNumber {
    unsigned int      year;
    unsigned int      month;
    unsigned int      day;
    unsigned int      hours;
    unsigned int      minutes;
    unsigned int      seconds;
}; typedef struct _icmDateTimeNumber icmDateTimeNumber;

/* Set the DateTime structure to the current date and time */
void icmDateTimeNumber_setcur(icmDateTimeNumber *p);

/* Convert a DateTimeNumber from UTC to local time */
void icmDateTimeNumber_tolocal(icmDateTimeNumber *d, icmDateTimeNumber *s);

/* ICC version version number */
typedef struct { 
	int majv, minv, bfv;		/* Version - major, minor, bug fix */
} icmVers;

/* =========================================================== */
/* Overflow protected unsigned int arithmatic */
/* These functions saturate rather than wrapping around */

/* Unsigned */
unsigned int sat_add(unsigned int a, unsigned int b);	/* a + b */
unsigned int sat_sub(unsigned int a, unsigned int b);	/* a - b */
unsigned int sat_mul(unsigned int a, unsigned int b);	/* a * b */
unsigned int sat_pow(unsigned int a, unsigned int b);	/* a ^ b */

/* Mixed sign */
unsigned int sat_add_us(unsigned int a, int b);	/* a + b */

/* Version that set the indicator flag if there was saturation, */
/* else indicator is unchanged */
unsigned int sati_add(int *ind, unsigned int a, unsigned int b);	/* a + b */
unsigned int sati_sub(int *ind, unsigned int a, unsigned int b);	/* a - b */
unsigned int sati_mul(int *ind, unsigned int a, unsigned int b);	/* a * b */
unsigned int sati_pow(int *ind, unsigned int a, unsigned int b);	/* a ^ b */

/* Float comparisons to certain precision */
int diff_u16f16(double a, double b);		/* u16f16 precision */

/* Counting down idioms: */

/* For signed count value where we want to access from last to first item: */
/* signed s, p->n; for (s = p->n-1; s >= 0; s--) { s: n-1 .. 0 } */

/* For unsigned count value where we want to access from last to first item: */
/* unsigned u, p->n; for (u = p->n; u-- > 0;) { u: n-1 .. 0 } */

/* For signed count value where we want to access from last to second item: */
/* signed s, p->n; for (s = p->n-1; s >= 1; s--) { s: n-1 .. 1 } */

/* For unsigned count value where we want to access from last to second item: */
/* unsigned u, p->n; for (u = p->n; u-- > 1;) { u: n-1 .. 1 } */

/* =========================================================== */

/* Errors and warnings:

	Errors are "sticky" and ultimately fatal for any particular
	operation. They can be cleared after that operation has returned.
	The first error is the one that is returned.

	Non-fatal Format errors are treated as fatal unless set to Warn.

	Non-fatal Version errors are treated as fatal unless set to Warn.

	Quirks are errors that would normally be fatal, but if the Quirk flag
	is set are repaired or ignored and treated as warnings.
*/

/* Flags that control the format & version checking behaviour. */
/* By default checking will be strict. */
typedef unsigned int icmCompatFlags;

#define icmCFlagRdFormatWarn ((icmCompatFlags)0x0001)
					/* Warn if read format is not strictly complient rather than error  */
#define icmCFlagWrFormatWarn ((icmCompatFlags)0x0002)
					/* Warn if write format is not strictly complient rather than error  */

#define icmCFlagRdVersionWarn ((icmCompatFlags)0x0004)
					/* Warn if read version has incompatibility problem rather than error  */

#define icmCFlagWrVersionWarn ((icmCompatFlags)0x0008)
					/* Warn if write version has incompatibility problem rather than error  */

#define icmCFlagRdAllowUnknown ((icmCompatFlags)0x0010)
					/* Allow reading unknown tags and tagtypes without warning or error. */
					/* Will warn if known tag uses unknown tagtype */

#define icmCFlagWrAllowUnknown ((icmCompatFlags)0x0020)
					/* Allow writing unknown tags without warning or error. */

#define icmCFlagWrFileRdWarn ((icmCompatFlags)0x0040)
					/* Warn if tags read from a file are not strictly complient rather than error */

#define icmCFlagAllowExtensions ((icmCompatFlags)0x0080)
					/* Allow ICCLIB extensions without warning or error. */

#define icmCFlagAllowQuirks ((icmCompatFlags)0x0100)
					/* Permit non-fatal deviations from spec., and allow for profile quirks */
					/* and emit warnings. */

#define icmCFlagAllowWrVersion ((icmCompatFlags)0x0200)
					/* Allow out of version Tags & TagTypes on write without warnings */
					/* over tag & tagtype version range defined by vcrange */
					/* (Use icp->set_vcrange() to set/unset) */

#define icmCFlagNoRequiredCheck ((icmCompatFlags)0x0400)
					/* Don't check for required tags on write */
					/* (This is intended only for testing) */

/* Flags that indicate that a warning has occured */
#define icmCFlagRdWarning ((icmCompatFlags)0x1000)
					/* A read warning was issued */
#define icmCFlagWrWarning ((icmCompatFlags)0x2000)
					/* A write warning was issued */

/* =================================================== */
/* Declaration of serialisation support functions. */

/* Operation that a serialisation routine should perform. */
/* Note that icmSnOp & icmSnResize will be true for Resize or Read */
typedef enum {
	/* Component flags - test is (icmSnOp & icmSnDumyBuf) etc. */
	icmSnDumyBuf   = 1,	/* A dummy buffer is used */
	icmSnSerialise = 2,	/* Full serialisation is needed, not just Size or Resize or Free */
	icmSnAlloc     = 4,	/* Need to alloc/realloc storage in struct */

	/* Individual states - test is (icmSnOp == icmSnResize) etc. */
    icmSnResize   = icmSnAlloc + icmSnDumyBuf,		/* 5 = Allocate & resize variable elements */
    icmSnSize     = icmSnSerialise + icmSnDumyBuf,	/* 3 = Compute file size elements would use */
    icmSnWrite    = icmSnSerialise,					/* 2 = Write elements */
    icmSnRead     = icmSnAlloc + icmSnSerialise,	/* 6 = Allocate and read elements */
    icmSnFree     = icmSnDumyBuf 					/* 1 = Free variable sized elements */
} icmSnOp;


/* File buffer class */
/* The serialisation functionaly uses this as a source or target. */
struct _icmFBuf {
	struct _icc *icp;	/* icc we're part of */
	struct _icmFBuf *super;	/* If not-NULL, we are a sub-buffer */

	icmSnOp op;			/* Operation being performed */
	unsigned int size;	/* size of tag (determines buffer size) */ 
	icmFile *fp;		/* Associated file */
	unsigned int of;	/* File offset of buffer */
	ORD8 *buf;			/* Pointer to buffer base */
	ORD8 *bp;			/* Pointer to next location to read/write */
	ORD8 *ep;			/* Pointer to location one past end of buffer */

	/* buf, bp, ep are dummy pointers used to compute size if op & icmSnDumyBuf */

	/* Offset the current buffer location by a relative amount */
	void (* roff)(struct _icmFBuf *p, INR32 off);

	/* Set the current location to an absolute location within the buffer */
	void (* aoff)(struct _icmFBuf *p, ORD32 off);

	/* Return the current absolute offset within the buffer */
	ORD32 (* get_off)(struct _icmFBuf *p);

	/* Return the currently available space within the buffer */
	ORD32 (* get_space)(struct _icmFBuf *p);

	/* Finalise (ie. Write), free object and buffer, return error and size used */
	ORD32 (* done)(struct _icmFBuf *p);

	/* Return a sub-buffer based at the current offset. */
	struct _icmFBuf * (* new_sub)(struct _icmFBuf *n, ORD32 size);

}; typedef struct _icmFBuf icmFBuf;


/* ==================================================== */
/* Convenience functions and macro's, to help implement */
/* tag types. */ 

/* Seting up the right kind of file buffer for each method type */
/* The buffer contains the type of processing flags. */

#define ICM_TAG_NEW_RESIZE_BUFFER											\
	icmFBuf *b;																\
	if ((b = new_icmFBuf(p->icp, icmSnResize, NULL, 0, 0)) == NULL)			\
		return p->icp->e.c;;												\

#define ICM_TAG_NEW_FREE_BUFFER												\
	icmFBuf *b;																\
	if ((b = new_icmFBuf(p->icp, icmSnFree, NULL, 0, 0)) == NULL)			\
		return;																\

#define ICM_TAG_NEW_SIZE_BUFFER												\
	icmFBuf *b;																\
	if ((b = new_icmFBuf(p->icp, icmSnSize, NULL, 0, 0)) == NULL)			\
		return 0;															\

#define ICM_TAG_NEW_READ_BUFFER												\
	icmFBuf *b;																\
	if ((b = new_icmFBuf(p->icp, icmSnRead,									\
	                         p->icp->rfp, of, size)) == NULL)				\
		return p->icp->e.c;

#define ICM_TAG_NEW_WRITE_BUFFER											\
	icmFBuf *b;																\
	if ((b = new_icmFBuf(p->icp, icmSnWrite,								\
	            p->icp->wfp, of, size + pad)) == NULL)						\
		return p->icp->e.c;													\


/* Implementation of array management utility function,         */
/* used to deal with the serialization actions on an array.   */

/* The allocation is sanity checked against the remaining file buffer size, */
/* so can't be used for allocations that aren't directly related to file contents. */

/* NOTE :- there is a potential memory leak with the current system -
 * any tagtype that has 2 or more levels of arrays will leak if
 * a base array is reduced in allocation. This doesn't usually happen,
 * since both icc_read and client API pattern is typically to only set or
 * increase array size. The best fix would be to add an option destructor
 * function argument to icmArrayRdAllocResize() and friends that is
 * called on each elemnt of an array that is being freed due to an
 * array size reduction. icmArrayRdAllocResize() could then
 * replace ICMSNFREEARRAY. This would also avoid a mismatch between these two!!!
 */

/* Array allocation count type */
typedef enum {
    icmAResizeByCount     = 0,  /* count is explicit */
    icmAResizeBySize      = 1   /* count set by available size */ 
} icmAResizeCountType;

/* The allocation is sanity checked against the remaining file buffer size, */
/* so can't be used for allocations that aren't directly related to file contents. */
/* Return zero on sucess, nz if there is an error */
static int icmArrayRdAllocResize(
	icmFBuf *b,					/* Buffer we're dealing with */
	icmAResizeCountType ct,		/* explicit count or count from buffer size ? */

	unsigned int *p_count,		/* Pointer to current count of items */
	unsigned int *pcount,		/* Pointer to new count of items */
	void **pdata,				/* Pointer to items memory allocation */
	unsigned int isize,			/* Memory size of each item */

	unsigned int maxsize,		/* Maximum size permitted for buffer (use UINT_MAX if not needed) */
	int fixedsize,				/* Fixed size to be allowed for in file buffer. */
	                            /* Buffer space allowed is min(maxsize, buf->space - fixedsize) */
	unsigned int bsize,			/* File buffer (ie. serialized) size of each item */
	char *tagdesc				/* Stringification of tag type used for warning/error reporting */
);

/* This version can be used where allocations aren't directly related to file contents. */
/* Return zero on sucess, nz if there is an error */
static int icmArrayAllocResize(
	icmFBuf *b,					/* Buffer we're dealing with */

	unsigned int *p_count,		/* Pointer to current count of items */
	unsigned int *pcount,		/* Pointer to new count of items */
	void **pdata,				/* Pointer to items allocation */
	unsigned int isize,			/* Item size */

	char *tagdesc				/* Stringification of tag type used for warning/error reporting */
);

/* Implement serialise() Array free functionality. */
/* B is pointer to icmFBuf */
/* _COUNT is the variable holding the allocated size of the array */
/* DATA is the variable holding the array */
/* (Should replace this with destructor argument to icmArrayRdAllocResize()) */
#define ICMSNFREEARRAY(B, _COUNT, DATA)													\
	{ if (b->op == icmSnFree) {															\
		B->icp->al->free(B->icp->al, DATA);												\
		DATA = NULL;																	\
		_COUNT = 0;																		\
	} }																					\


/* This version does an immediate realloc. */
/* Return zero on sucess, nz if there is an error */
static int icmArrayResize(
	struct _icc *icp,

	unsigned int *p_count,		/* Pointer to current count of items */
	unsigned int *pcount,		/* Pointer to new count of items */
	void **pdata,				/* Pointer to items allocation */
	unsigned int isize,			/* Item size */

	char *tagdesc				/* Stringification of tag type used for warning/error reporting */
);

/* Implement immediate Array free functionality. */
#define ICMFREEARRAY(ICP, _COUNT, DATA)													\
		ICP->al->free(ICP->al, DATA);													\
		DATA = NULL;																	\
		_COUNT = 0;																		\

#ifdef NEVER        // Not clear if this will get used...

/* A general matrix resizing utility function,         */
/* used within XXXX__serialise() functions.                   */

/* Return zero on sucess, nz if there is an error */
int icmMatrixRdAllocResize(
	icmFBuf *b,					/* Buffer we're dealing with */

	unsigned int *p_rcount,		/* Pointer to current row count */
	unsigned int *prcount,		/* Pointer to new row count */
	unsigned int *p_ccount,		/* Pointer to current column count */
	unsigned int *pccount,		/* Pointer to new column count */
	void ***pdata,				/* Pointer to matrix of items allocation */
	unsigned int isize,			/* Item size */

								/* Count value sanity check data: */
	unsigned int maxsize,		/* Maximum size permitted for buffer (use UINT_MAX if not less than
								/* remaining in buffer) */
	unsigned int fixedsize,		/* Further fixed size from current location before variable */
								/* elements. Space allowed for variable elements is */
								/* avail = min(maxsize, buf->space - fixedsize) */
	unsigned int bsize,			/* File buffer (ie. serialized) size of each item. */
								/* Use minumum if variable ? */
								/* Error if (*pcount * bsize) > avail */
	char *tagdesc				/* Stringification of tag type used for warning/error reporting */
);

/* Implement serialise() Matrix free functionality. */
/* B is pointer to icmFBuf */
/* _ROWS and _COLUMNS are the variables holding the allocated dimensions of the matrix */
/* DATA is the variable holding the matrix */
/* (Should replace this with destructor argument to icmArrayRdAllocResize() ?) */
#define ICMFREEMATRIX(B, _ROWS, _COLS, DATA)											\
	{ if (b->op == icmSnFree) {															\
		unsigned int j;																	\
		for (j = 0; j < prcount; j++)													\
			B->icp->al->free(B->icp->al, (DATA)[j]);									\
		B->icp->al->free(B->icp->al, DATA);												\
		DATA = NULL;																	\
		_ROWS = _COLS = 0;																\
	} }																					\

#endif /* NEVER */


/* On read, check the tag has been fully consumed */
/* Note that we can't use this if there is no directory above us, since */
/* we then don't know the expected size of this tag, */
/* and we also can't use it if the sub-elements of this tag are randomly */
/* seek'd to. (This implies that there must be a directory within the tag */
/* for the sub-elements) */
#define ICMRDCHECKCONSUMED(TAGTYPE)														\
	if (b->op == icmSnRead) {															\
		unsigned int tavail;															\
		tavail = b->get_space(b);														\
		if (tavail != 0) {																\
			icmFormatWarning(b->icp, ICM_FMT_SHORTTAG,									\
			         #TAGTYPE " tag array doesn't occupy all of tag (%u bytes short)",	\
			         tavail);															\
		}																				\
	}																					\

/* --------------------------------------------------- */

/*
 * Method serialise is implemented and get_size, read, write, del & allocate methods
 * are implemented by the icmGeneric* functions, which use serialise to
 * do all the work. 
 *
 *  Read and write method error codes:
 *  0 = sucess
 *  1 = file format/logistical error
 *  2 = system error
 */

#define ICM_BASE_MEMBERS(TCLASS)															\
	/* Private: */																			\
	icTagTypeSignature ttype;		/* The tag type signature of the object */				\
	struct _icc    *icp;			/* Pointer to ICC we're a part of */					\
	/* enum icmPeSignature */ unsigned int etype;											\
									/* If an icmPe, the processing element sig. */			\
	icTagSignature creatorsig;		/* tag that created this tagtype */						\
									/* (shared tagtypes will have been linked to others) */	\
	int	           touched;			/* Flag for write bookeeping */							\
    int            refcount;		/* Reference count for sharing tag instances */			\
	int            rdff;			/* Was read from file */								\
	int            dp;				/* dump() padding (if implemented) */					\
	int			   emb;				/* nz if embedded-tag */								\
																							\
	void           (*serialise)(TCLASS *p, icmFBuf *b);										\
									/* Usual Read/Write/Del/Allocate implementation, */		\
	unsigned int   (*get_size)(TCLASS *p);													\
									/* Return number of bytes needed to write this tag */	\
	int            (*read)(TCLASS *p, unsigned int size, unsigned int of);					\
									/* size is expected size to read */						\
									/* Offset is offset into file */						\
	int            (*write)(TCLASS *p, unsigned int size, unsigned int of,					\
					                  unsigned int pad);									\
									/* size is expected size to write */					\
									/* Offset is offset into file */						\
									/* pad is padding needed */								\
	TCLASS          *(*reference)(TCLASS *p);													\
									/* Take a reference to this object. */					\
									/* ->del() to remove reference. */						\
																							\
	void           (*del)(TCLASS *p);														\
									/* Free this object */									\
																							\
	/* Public: */																			\
	/* ?? should add get_ttype() so we can check ttype without looking at private ?? */		\
																							\
	void           (*dump)(TCLASS *p, icmFile *op, int verb);								\
									/* Dump a human readable description of this */			\
									/* object to the given output file. */					\
									/* 0 = silent, 1 = minimal, 2 = moderate, 3 = all */	\
	int            (*allocate)(TCLASS *p);													\
									/* Re-allocate space needed acording to sizes */		\
									/* in the object. Return an error code */				\
	int            (*check)(TCLASS *p, icTagSignature sig, int rd);							\
									/* Perform check of tag values before write/after */	\
	                                /* read for validity above that of the elements */		\
	                                /* it is composed of. May be NULL. Ret E.C. */			\
	                                /* rd flag NZ if reading, else writing */				\
																							\
	              /* Optional methods, NULL if none */										\
	int           (*cmp)(TCLASS *dst, TCLASS *src);	/* Compare another with this */			\
													/* (Used by icc_compare_ttype()) */		\
	int           (*cpy)(TCLASS *dst, struct _icmBase *src);	/* Copy/translate another */	\
	                                           /* to this. (used by icc->copy_ttype()) */	\

/* Serialisable init */
#define ICM_BASE_INIT(TCLASS, TSIG)														\
	p->ttype     = TSIG;																\
	p->icp       = icp;																	\
	p->refcount  = 1;																	\
	p->rdff      = icp->rdff;															\
	p->serialise = TCLASS##_serialise;													\
	p->get_size  = (unsigned int (*)(TCLASS *p))	icmGeneric_get_size;				\
	p->read      = (int (*)(TCLASS *p, unsigned int size, unsigned int of))				\
	               icmGeneric_read;														\
	p->write     = (int (*)(TCLASS *p, unsigned int size, unsigned int of, 				\
	                unsigned int pad)) icmGeneric_write;								\
	p->reference = (TCLASS *(*)(TCLASS *p)) icmGeneric_reference;						\
	p->del       = (void (*)(TCLASS *p)) icmGeneric_delete;								\
	p->dump      = TCLASS##_dump;														\
	p->allocate  = (int (*)(TCLASS *p)) icmGeneric_allocate;							\
	p->check     = TCLASS##_check;														\

/* Non-serialisable init */
#define ICM_BASE_NS_INIT(TCLASS)														\
	p->ttype     = icmSigUnknownType;													\
	p->icp       = icp;																	\
	p->refcount  = 1;																	\
	p->serialise = NULL;																\
	p->get_size  = NULL;																\
	p->read      = NULL;																\
	p->write     = NULL;																\
	p->reference = (TCLASS *(*)(TCLASS *p)) icmGeneric_reference;						\
	p->del       = TCLASS##_del;														\
	p->dump      = TCLASS##_dump;														\
	p->allocate  = NULL;																\
	p->check     = NULL;																\

/* TagType class allocation and initialisation. */
/* It's assumed that the parent class provides all methods. */
/* By default we initialise to the generic implementation, */
/* that assumes the presense of only the TCLASS##_serialise(), TCLASS##_dump() */
/* and (optional) TCLASS##_check() methods. */
/* returens NULL on error */
#define ICM_BASE_ALLOCINIT(TCLASS, TSIG)												\
	TCLASS *p = NULL;																	\
	if (icp->e.c != ICM_ERR_OK)															\
		return NULL;																	\
	if ((p = (TCLASS *)icp->al->calloc(icp->al, 1, sizeof(TCLASS))) == NULL) {			\
		icm_err(icp, ICM_ERR_MALLOC, "Allocating tag %s failed",#TCLASS);				\
		return NULL;																	\
	} else {																			\
		ICM_BASE_INIT(TCLASS, TSIG)														\
	}																					\

#define ICM_BASE_NS_ALLOCINIT(TCLASS)													\
	TCLASS *p = NULL;																	\
	if (icp->e.c != ICM_ERR_OK)															\
		return NULL;																	\
	if ((p = (TCLASS *)icp->al->calloc(icp->al, 1, sizeof(TCLASS))) == NULL) {			\
		icm_err(icp, ICM_ERR_MALLOC, "Allocating tag %s failed",#TCLASS);				\
		return NULL;																	\
	} else {																			\
		ICM_BASE_NS_INIT(TCLASS)														\
	}																					\

/* Base tag element data object */
struct _icmBase {
	ICM_BASE_MEMBERS(struct _icmBase)
}; typedef struct _icmBase icmBase;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* Tag type to hold an unknown tag type, */
/* so that it can be read and copied. */
struct _icmUnknown {
	ICM_BASE_MEMBERS(struct _icmUnknown)

	/* Private: */
	unsigned int     _count;	/* Count of data currently allocated */

	/* Public: */
	icTagTypeSignature uttype;	/* The unknown tag type actual signature */
	unsigned int	  count;	/* Allocated and used count of the array */
    unsigned char	  *data;  	/* tag data */

}; typedef struct _icmUnknown icmUnknown;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* UInt8 Array */
struct _icmUInt8Array {
	ICM_BASE_MEMBERS(struct _icmUInt8Array)

	/* Private: */
	unsigned int   _count;		/* Size currently allocated */

	/* Public: */
	unsigned int	count;		/* Allocated and used size of the array */
    unsigned int   *data;		/* Pointer to array of data */ 
}; typedef struct _icmUInt8Array icmUInt8Array;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* uInt16 Array */
struct _icmUInt16Array {
	ICM_BASE_MEMBERS(struct _icmUInt16Array)

	/* Private: */
	unsigned int   _count;		/* Size currently allocated */

	/* Public: */
	unsigned int	count;		/* Allocated and used count of the array */
    unsigned int	*data;		/* Pointer to array of data */ 
}; typedef struct _icmUInt16Array icmUInt16Array;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* uInt32 Array */
struct _icmUInt32Array {
	ICM_BASE_MEMBERS(struct _icmUInt32Array)

	/* Private: */
	unsigned int   _count;		/* Size currently allocated */

	/* Public: */
	unsigned int	count;		/* Allocated and used count of the array */
    unsigned int	*data;		/* Pointer to array of data */ 
}; typedef struct _icmUInt32Array icmUInt32Array;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* UInt64 Array */
struct _icmUInt64Array {
	ICM_BASE_MEMBERS(struct _icmUInt64Array)

	/* Private: */
	unsigned int   _count;		/* Size currently allocated */

	/* Public: */
	unsigned int	count;		/* Allocated and used count of the array */
    icmUInt64		*data;		/* Pointer to array of hight data */ 
}; typedef struct _icmUInt64Array icmUInt64Array;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* u16Fixed16 Array */
struct _icmU16Fixed16Array {
	ICM_BASE_MEMBERS(struct _icmU16Fixed16Array)

	/* Private: */
	unsigned int   _count;		/* Size currently allocated */

	/* Public: */
	unsigned int	count;		/* Allocated and used count of the array */
    double			*data;		/* Pointer to array of hight data */ 
}; typedef struct _icmU16Fixed16Array icmU16Fixed16Array;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* s15Fixed16 Array */
struct _icmS15Fixed16Array {
	ICM_BASE_MEMBERS(struct _icmS15Fixed16Array)

	/* Private: */
	unsigned int   _count;		/* Size currently allocated */

	/* Public: */
	unsigned int	count;		/* Allocated and used count of the array */
    double			*data;		/* Pointer to array of hight data */ 
}; typedef struct _icmS15Fixed16Array icmS15Fixed16Array;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* XYZ Array */
struct _icmXYZArray {
	ICM_BASE_MEMBERS(struct _icmXYZArray)

	/* Private: */
	unsigned int   _count;		/* Size currently allocated */

	/* Public: */
	unsigned int	count;		/* Allocated and used count of the array */
    icmXYZNumber	*data;		/* Pointer to array of data */ 
}; typedef struct _icmXYZArray icmXYZArray;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Chromaticity */

struct _icmChromaticity {
	ICM_BASE_MEMBERS(struct _icmChromaticity)

	/* Private: */
	unsigned int       _count;	/* Count currently allocated */

	/* Public: */
	icPhColEncoding enc;		/* Encoding of chromaticity coordinates */ 
	unsigned int	   count;	/* Count of device channels */
    icmxyCoordinate	  *data;  	/* Array of [count] Chromaticity coordinates */

								/* Setup the tag with a defined encoding */
								/* Return error code. */
	int (*setup)(struct _icmChromaticity *p);

}; typedef struct _icmChromaticity icmChromaticity;


/* - - - - - - - - - - - - - - - - - - - - -  */
/* Data */

struct _icmData {
	ICM_BASE_MEMBERS(struct _icmData)

	/* Private: */
	unsigned int    fcount;		/* Count in file (different if text lacks nul) */		\
	unsigned int   _count;		/* Size currently allocated */

	/* Public: */
    icAsciiOrBinary	flag;		/* Type of data */
	unsigned int	count;		/* Allocated and used size of the array (inc ascii null) */
    unsigned char	*data;  	/* data or string, NULL if size == 0 */
}; typedef struct _icmData icmData;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* Pseudo-ascii type that maps to icTextType or icSigTextDescriptionType, */
/* and has a common base class definition. */

#define COMMONTEXTDESCRIPTION_X

/* Check a tagtype to see if it is a commontext type */
# define ISCOMMONTEXT(ttype) 												\
   (((ttype) == icSigTextType || (ttype) == icSigTextDescriptionType))


#define icmSigCommonTextDescriptionType ((icTagTypeSignature)icmMakeTag('c','m','t','d'))

/* Base class */ 
#define ICM_CMTD_MEMBERS(TCLASS)		/* This tag type */										\
	ICM_BASE_MEMBERS(TCLASS)																	\
																							\
	/* Private: */																			\
	unsigned int     _count;		/* Count currently allocated */							\
	unsigned int     fcount;		/* Count in file */										\
	COMMONTEXTDESCRIPTION_X																	\
																							\
	/* Public: */																			\
	unsigned int 	  count;		/* Allocated and used size of desc, inc null */			\
	char              *desc;		/* ascii or utf-8 string (null terminated) */			\

struct _icmCommonTextDescription {
	ICM_CMTD_MEMBERS(struct _icmCommonTextDescription)
}; typedef struct _icmCommonTextDescription icmCommonTextDescription;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* text */
struct _icmText {
	ICM_CMTD_MEMBERS(struct _icmText)

}; typedef struct _icmText icmText;


/* - - - - - - - - - - - - - - - - - - - - -  */
/* DateTime */
struct _icmDateTime {
	ICM_BASE_MEMBERS(struct _icmDateTime)

	icmDateTimeNumber date;		/* Date/Time */

}; typedef struct _icmDateTime icmDateTime;

/* ============================================== */
/* icmProcessing Element declarations */

#include "icc_pe.h"

/* ============================================== */
/* lut */
 
/* Set method flags */
#define ICM_CREATE_FLAG_NONE 0x0000

#define ICM_CLUT_SET_EXACT 0x0000	/* Set clut node values exactly from callback */
#define ICM_CLUT_SET_APXLS 0x0001	/* Set clut node values to aproximate least squares fit */


/* - - - - - - - - - - - - - - - - - - - - -  */
/* Measurement Data */

struct _icmMeasurement {
	ICM_BASE_MEMBERS(struct _icmMeasurement)

	/* Public: */
    icStandardObserver           observer;       /* Standard observer */
    icmXYZNumber                 backing;        /* XYZ for backing */
    icMeasurementGeometry        geometry;       /* Meas. geometry */
    double                       flare;          /* Measurement flare */
    icIlluminant                 illuminant;     /* Standard illuminant */
}; typedef struct _icmMeasurement icmMeasurement;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* Named color */

/* Structure that holds each named color data */
typedef struct {
	/* Private: */
	unsigned int      _rcount;			/* Root count currently allocated */

	/* Public: */
	unsigned int 	  rcount;			/* Allocated and used size of root, inc null */
	char             *root;				/* Root name for color */
	double            pcsCoords[3];		/* icmNC2: PCS coords of color */
	double            deviceCoords[MAX_CHAN];	/* Dev coords of color */
} icmNamedColorVal;

struct _icmNamedColor {
	ICM_BASE_MEMBERS(struct _icmNamedColor)

	/* Private: */
	unsigned int      _count;			/* Count colors currently allocated */
	unsigned int      _pcount;			/* Prefix count currently allocated */
	unsigned int      _scount;			/* Suffix count currently allocated */

	/* Public: */
    unsigned int      vendorFlag;		/* Bottom 16 bits for ICC use */
    unsigned int      count;			/* Count of named colors */
    unsigned int      nDeviceCoords;	/* Num of device coordinates */
	unsigned int 	  pcount;			/* Allocated and used size of prefix, inc null */
    char              *prefix;			/* Prefix for each color name (null terminated) */
	unsigned int 	  scount;			/* Allocated and used size of suffix, inc null */
    char              *suffix;			/* Suffix for each color name (null terminated) */
    icmNamedColorVal  *data;			/* Array of [count] color values */
}; typedef struct _icmNamedColor icmNamedColor;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* Colorant Table */

/* Structure that holds colorant table data */
typedef struct {

	/* Private: */
	unsigned int      _ncount;			/* name count currently allocated */

	/* Public: */
	unsigned int 	  ncount;			/* Allocated and used size of name, inc null */
	char               *name;			/* Name for colorant (null terminated) */
	double             pcsCoords[3];	/* PCS coords of colorant */
} icmColorantTableVal;

struct _icmColorantTable {
	ICM_BASE_MEMBERS(struct _icmColorantTable)

	/* Private: */
	unsigned int         _count;		/* Count currently allocated */

	/* Public: */
    unsigned int         count;			/* Count of colorants */
    icmColorantTableVal *data;			/* Array of [count] colorants */
}; typedef struct _icmColorantTable icmColorantTable;


/* - - - - - - - - - - - - - - - - - - - - -  */
/* textDescription */

struct _icmTextDescription {
	ICM_CMTD_MEMBERS(struct _icmTextDescription)

	/* Private: */
	/* Defined in ICM_CMTD_MEMBERS: */
	/* unsigned int      fcount;	   Count in ICC file */
	/* unsigned int      _count;	   Count currently allocated */
	unsigned int   uc16count;		/* uc UTF-16 file character count */
	unsigned int   _uc8count;		/* uc Count currently allocated */
	unsigned int    scFcount;	   	/* ScriptCode Count in file */
	unsigned int   _scCount;	   	/* ScriptCode Count currently allocated */

	/* Public: */

	/* Defined in ICM_CMTD_MEMBERS: */
	/* unsigned int 	  count;	   Allocated and used size of desc, inc null */
	/* char              *desc;		   ascii string (null terminated) */

	/* Note that uc8desc may be NULL or count == 0 */
	/* corresponding to no bytes used in tag. */
	/* (All other text is assumed to be always nul terminated) */
	unsigned int      ucLangCode;	/* UniCode language code */
	unsigned int 	  uc8count;		/* Allocated and used size of ucDesc in chars, inc null */
	icmUTF8           *uc8desc;		/* The utf-8 description (nul terminated) */

	/* Note that scDesc may be NULL or count == 0. */
	/* See <http://unicode.org/Public/MAPPINGS/VENDORS/APPLE/ReadMe.txt> */
	unsigned short     scCode;		/* ScriptCode code */
	unsigned int 	  scCount;		/* Occupied size of scDesc in bytes, inc null */
	ORD8              *scDesc;		/* ScriptCode Description (null terminated, max 67 long) */
}; typedef struct _icmTextDescription icmTextDescription;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* icmProfileSequenceDesc */

struct _icmPSDescStruct {
	/* Public: */
    icmSig            deviceMfg;		/* Dev Manufacturer */
    unsigned int      deviceModel;		/* Dev Model */
    icmUInt64         attributes;		/* Dev attributes */
    icTechnologySignature technology;	/* Technology sig */
	icmCommonTextDescription *mfgDesc;		/* Manufacturer description text (sub-tagtype) */
											/* Created by ->allocate() */
	icmCommonTextDescription *modelDesc;		/* Model description text (sub-tagtype) */
												/* Created by ->allocate() */
}; typedef struct _icmPSDescStruct icmPSDescStruct;

/* Profile sequence description */
struct _icmProfileSequenceDesc {
	ICM_BASE_MEMBERS(struct _icmProfileSequenceDesc)

	/* Private: */
	unsigned int	 _count;			/* number currently allocated */

	/* Public: */
    unsigned int      count;			/* Number of descriptions */
	icmPSDescStruct     *data;			/* array of [count] descriptions */
}; typedef struct _icmProfileSequenceDesc icmProfileSequenceDesc;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* icmResponseCurveSet16 */

struct _icmRCS16Struct {
	/* Private */
	unsigned int     off;				/* Offset from begining of TagType to RCS16Struct*/
    unsigned int     __nMeasNchan;		/* Allocated number of device channels for _nMeas */
    unsigned int     _nchan;			/* Allocated number of device channels for nMeas */
	unsigned int     _pcsNchan;			/* Allocated number of device channels for pcsData */
	unsigned int     _respNchan;		/* Allocated number of device channels for response */

	unsigned int     *_nMeas;			/* array of [nchan] allocated number of measurements */
	
	/* Public: */
	icMeasUnitsSig    measUnit;			/* Measurement unit */

	unsigned int     *nMeas;			/* array of [nchan] number of measurement values */

	icmXYZNumber	 *pcsData;			/* array of [nchan] PCS XYZ max. colorant values */
	icmResponse16Number **response;		/* array of [nchan][nMeas] response numbers */

}; typedef struct _icmRCS16Struct icmRCS16Struct;

/* Profile sequence description */
struct _icmResponseCurveSet16 {
	ICM_BASE_MEMBERS(struct _icmResponseCurveSet16)

	/* Private: */
	unsigned int	 _typeCount;		/* number currently allocated */

	/* Public: */
    unsigned int      nchan;			/* Number of device channels */
    unsigned int      typeCount;		/* Number of measurement types */
	icmRCS16Struct    *typeData;		/* array of [typeCount] responses */
}; typedef struct _icmResponseCurveSet16 icmResponseCurveSet16;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Device Settings */

/* The Settings Structure holds an array of settings of a particular type */
struct _icmSettingStruct {

	/* Private: */
	unsigned int   _ssize;		/* Setting variable size in bytes */
	unsigned int   _count;		/* Count currently allocated */
	unsigned int   size;		/* Size in file bytes of each setting value */

	/* Public: */
	unsigned int   ssize;		/* Setting variable size in bytes (for unknown) */
	unsigned int   count; 		/* number of setting values */
	union {
		struct {
			icDevSetMsftIDSignature sig;		/* Setting identification */
			union {								/* Setting values - type depends on Sig */
				struct icmMsfResolution {
					unsigned int xres;			/* DPI */
					unsigned int yres;
				} *resolution;
				icDevSetMsftMedia   *media;
				icDevSetMsftDither  *halftone;
				unsigned char       *unknown;		/* If an unknown settingSig, count of size, */
													/* access i'th at unknown[ssize * i] */
			};
		} msft;
		/* Other platform signatures & setting types go here */

		/* If the platform is unknown or has no known settings: */
		struct {
			unsigned int   sig;			/* Setting identification */
			unsigned char  *unknown;	/* count of ssize, access i'th at unknown[ssize * i] */
		} unknown;
	};
}; typedef struct _icmSettingStruct icmSettingStruct;

/* A Setting Combination holds all arrays of different setting types */
struct _icmSettingComb {
	/* Private: */
	unsigned int   size;			/* Size in file bytes of this structure */
	unsigned int   _count;			/* count currently allocated */

	/* Public: */
	unsigned int       count; 		/* num of setting structures */
	icmSettingStruct  *data;
}; typedef struct _icmSettingComb icmSettingComb;

/* A Platform Entry holds all setting combinations for that platform */
struct _icmPlatformEntry {
	/* Private: */
	unsigned int        size;		/* Size in file bytes of this structure */
	unsigned int       _count;		/* count currently allocated */

	/* Public: */
	icPlatformSignature platform;
	unsigned int        count;		/* count of settings and allocated array size */
	icmSettingComb     *data; 
}; typedef struct _icmPlatformEntry icmPlatformEntry;

/* The Device Settings holds all platform settings */
struct _icmDeviceSettings {
	ICM_BASE_MEMBERS(struct _icmDeviceSettings)

	/* Private: */
	unsigned int   _count;			/* number currently allocated */

	/* Public: */
	unsigned int    count;			/* num of platforms and allocated array size */
	icmPlatformEntry *data;			/* Array of pointers to platform entry data */
}; typedef struct _icmDeviceSettings icmDeviceSettings;


/* - - - - - - - - - - - - - - - - - - - - -  */
/* signature (only ever used for technology ??) */
struct _icmSignature {
	ICM_BASE_MEMBERS(struct _icmSignature)

	/* Public: */
    icSig sig;		/* Signature */
}; typedef struct _icmSignature icmSignature;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* Per channel Screening Data */
typedef struct {
	/* Public: */
    double            frequency;		/* Frequency */
    double            angle;			/* Screen angle */
    icSpotShape       spotShape;		/* Spot Shape encodings below */
} icmScreeningData;

struct _icmScreening {
	ICM_BASE_MEMBERS(struct _icmScreening)

	/* Private: */
	unsigned int	  _channels;		/* number currently allocated */

	/* Public: */
    icScreening       screeningFlags;	/* Screening flags */
    unsigned int      channels;			/* Number of channels */
    icmScreeningData  *data;			/* Array of screening data */
}; typedef struct _icmScreening icmScreening;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* Under color removal, black generation */
struct _icmUcrBg {
	ICM_BASE_MEMBERS(struct _icmUcrBg)

	/* Private: */
    unsigned int      _UCRcount;		/* Currently allocated UCR count */
    unsigned int      _BGcount;			/* Currently allocated BG count */
	unsigned int	  fcount;			/* Count in file (different if text lacks nul) */
	unsigned int 	  _count;			/* Currently allocated string size */

	/* Public: */
    unsigned int      UCRcount;			/* Undercolor Removal Curve length */
    double           *UCRcurve;		    /* The array of UCR curve values, 0.0 - 1.0 */
										/* or 0.0 - 100 % if count = 1 */
    unsigned int      BGcount;			/* Black generation Curve length */
    double           *BGcurve;			/* The array of BG curve values, 0.0 - 1.0 */
										/* or 0.0 - 100 % if count = 1 */
	unsigned int 	  count;			/* Allocated and used size of desc, inc null */
	char              *string;			/* UcrBg description (null terminated) */
}; typedef struct _icmUcrBg icmUcrBg;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* viewingConditionsType */
struct _icmViewingConditions {
	ICM_BASE_MEMBERS(struct _icmViewingConditions)

	/* Public: */
    icmXYZNumber    illuminant;		/* In candelas per sq. meter */
    icmXYZNumber    surround;		/* In candelas per sq. meter */
    icIlluminant    stdIlluminant;	/* See icIlluminant defines */
}; typedef struct _icmViewingConditions icmViewingConditions;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* Postscript Color Rendering Dictionary names type */
struct _icmCrdInfo {
	ICM_BASE_MEMBERS(struct _icmCrdInfo)

	/* Private: */
    unsigned int     _ppcount;		/* Currently allocated count */
    unsigned int     fppcount;		/* Count in file */
	unsigned int     _crdcount[4];	/* Currently allocated counts */
	unsigned int     fcrdcount[4];	/* Count in file */

	/* Public: */
    unsigned int     ppcount;		/* Postscript product name count (including null) */
    char            *ppname;		/* Postscript product name (null terminated) */
	unsigned int     crdcount[4];	/* Rendering intent 0-3 CRD names counts (icluding null) */
	char            *crdname[4];	/* Rendering intent 0-3 CRD names (null terminated) */
}; typedef struct _icmCrdInfo icmCrdInfo;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* Apple ColorSync 2.5 video card gamma type (vcgt) */

struct _icmVideoCardGammaTable {
	/* Private: */
	unsigned int   _entryCount[3];	/* Currently allocated entryCount */

	/* Public: */
	unsigned int   channels;		/* No of gamma channels (1 or 3) */
	unsigned int   entryCount;		/* Number of entries per channel */
	unsigned int   entrySize;		/* Size in bytes of each entry */
	double          *data[3];		/* Entry for each channel */
}; typedef struct _icmVideoCardGammaTable icmVideoCardGammaTable;

struct _icmVideoCardGammaFormula {
	/* Red, Green, Blue */
	double           gamma[3];		/* must be > 0.0 */
	double           min[3];		/* must be > 0.0 and < 1.0 */
	double           max[3];		/* must be > 0.0 and < 1.0 */
}; typedef struct _icmVideoCardGammaFormula icmVideoCardGammaFormula;

struct _icmVideoCardGamma {
	ICM_BASE_MEMBERS(struct _icmVideoCardGamma)
	icVideoCardGammaFormat tagType;	/* eg. table or formula */
	union {
		icmVideoCardGammaTable   table;
		icmVideoCardGammaFormula formula;
	} u;

	double (*lookup)(struct _icmVideoCardGamma *p, int chan, double iv); /* Read a value */

}; typedef struct _icmVideoCardGamma icmVideoCardGamma;

/* ------------------------------------------------- */
/* The Profile header */
/* NOTE that the deviceClass should be set up before calling chromAdaptMatrix()! */ 
struct _icmHeader {

	ICM_BASE_MEMBERS(struct _icmHeader)

	unsigned int hsize;		/* Header size */
	unsigned int phsize;	/* Padded header size */
	unsigned int size;		/* Nominated size of the profile  */

	int doid;				/* Flag, nz = writing to compute ID */

	/* Values that must be set before writing */
    icProfileClassSignature deviceClass;	/* Type of profile */
    icColorSpaceSignature   colorSpace;		/* Color space of data (Cannonical input space) */
    icColorSpaceSignature   pcs;			/* PCS: XYZ or Lab (Cannonical output space) */
    icRenderingIntent       renderingIntent;/* Rendering intent (lower 16 bits) */

	/* Values that should be set before writing */
    icmSig                  manufacturer;	/* Dev manufacturer */
    icmSig		            model;			/* Dev model */
    icmUInt64               attributes;		/* Device attributes */
    icProfileFlags			flags;			/* Various bits */

	/* Values that may optionally be set before writing */
    /* icmUInt64            attributes;		   Device attributes.h (see above) */
    icmSig                  creator;		/* Profile creator */

	/* Values that are not normally set, since they have defaults */
    icmSig                  cmmId;			/* CMM for profile */
	icmVers                 vers;			/* Format version */
    icmDateTimeNumber       date;			/* Creation Date */
    icPlatformSignature     platform;		/* Primary Platform */
    icmXYZNumber            illuminant;		/* Profile illuminant (usually D50) */
    icRenderingIntent       extraRenderingIntent;	/* top 16 bits are extra non-ICC part of RI */

	/* Values that are created automatically */
    unsigned char           id[16];			/* MD5 fingerprint id, lsB to msB  <V4.0+> */

}; typedef struct _icmHeader icmHeader;

/* ---------------------------------------------------------- */
/* Objects for accessing lookup functions. */
/* Note that the "effective" PCS colorspace is the one specified by the */
/* PCS override, and visible in the overal profile conversion. */
/* The "native" PCS colorspace is the one that the underlying tags actually */
/* represent, and the PCS that the individual stages within each profile type handle. */
/* The conversion between the native and effective PCS is done in the to/from_abs() */
/* conversions. */

/* Public: Parameter to get_luobj function */
typedef enum {
    icmFwd           = 0,  /* Device to PCS, or Device 1 to Last Device */
    icmBwd           = 1,  /* PCS to Device, or Last Device to Device */
    icmGamut         = 2,  /* PCS Gamut check */
    icmPreview       = 3   /* PCS to PCS preview */
} icmLookupFunc;

/* Public: Parameter to get_luobj function */
typedef enum {
    icmLuOrdNorm     = 0,  /* Normal profile preference:  Lut, matrix, monochrome */
    icmLuOrdRev      = 1   /* Reverse profile preference: monochrome, matrix, Lut */
} icmLookupOrder;

typedef enum {
    icmSpaceLu4Type		= 10,	/* Color Space */
    icmNamedLu4Type		= 11	/* Named color */
} icmLu4Type;

/* Public: Lookup algorithm object type (legacy) */
typedef enum {
    icmMonoFwdType       = 0,	/* Monochrome, Forward */
    icmMonoBwdType       = 1,	/* Monochrome, Backward */
    icmMatrixFwdType     = 2,	/* Matrix, Forward */
    icmMatrixBwdType     = 3,	/* Matrix, Backward */
    icmLutType           = 4,	/* Multi-dimensional Lookup Table */
    icmNamedType         = 5	/* Named color data */
} icmLuAlgType;


typedef struct _icmLu4Space icmLu4Space;
typedef struct _icmLu4Base icmLu4Base;

typedef icmLu4Base icmLuBase;
typedef icmLu4Space icmLuMono;
typedef icmLu4Space icmLuMatrix;
typedef icmLu4Space icmLuLut;
typedef icmLu4Space icmLuSpace;		/* Convert to this eventually */

/* ========================================================== */
/* icmPe transform declarations */

#include "icc_xf.h"		

/* ========================================================== */

/* A tag record in the header */
typedef struct {
    icTagSignature      sig;			/* The tag signature */
	icTagTypeSignature  ttype;			/* The tag type actual signature */
    unsigned int        offset;			/* File offset relative to profile */
    unsigned int        size;			/* Size in bytes (not including padding) */
	unsigned int        pad;			/* Padding */
	icmBase            *objp;			/* In memory data structure, NULL if never read. */
} icmTagRec;


/* =================================================== */
/* Tag version managament support */

/* Return a string that shows the icc's version */
char *icmICCvers2str(struct _icc *p);

/* A version management version number */
typedef int icmTV; 	/* Version - major * 10000 + minor * 100 + bug fix */

/* Return a string that shows the given version */
/* This can only be called once before reusing returned static buffer */
char *icmTVers2str(icmTV tv);

/* Range of versions, inclusive */
typedef struct { 
	icmTV min, max;
} icmTVRange;

/* Return a string that shows the given version range */
/* This can only be called once before reusing returned static buffer */
char *icmTVersRange2str(const icmTVRange *tvr);

/* True if a given version is within the rang. */
#define ICMTV_IN_RANGE(vers, vrangep) ((vrangep)->min <= (vers) && (vers) <= (vrangep)->max)

/* Pre-defined version extremes */
#define ICMTV_MIN 0
#define ICMTV_MAX 999999

#define ICMTV_20 20000
#define ICMTV_21 20100		/* (N-Color) */
#define ICMTV_22 20200
#define ICMTV_23 20300		/* (Chromaticity Tag) */
#define ICMTV_24 20400		/* (Display etc. have intents) */
#define ICMTV_40 40000		/* (MultiLocalizedUnicode, LutAToB etc., ColorantOrder etc.) */
#define ICMTV_41 40100
#define ICMTV_42 40200
#define ICMTV_43 40300		/* (BToD0 etc.) */
#define ICMTV_44 40400		/* (Cicp, Metadata) */

#define ICMTV_DEFAULT ICMTV_22		/* To use default creation ICC version */

#define ICMTVRANGE_ALL { ICMTV_MIN, ICMTV_MAX }
#define ICMTVRANGE_NONE { ICMTV_MAX, ICMTV_MIN }

#define ICMTVRANGE_20_PLUS { ICMTV_20, ICMTV_MAX }
#define ICMTVRANGE_21_PLUS { ICMTV_21, ICMTV_MAX }
#define ICMTVRANGE_22_PLUS { ICMTV_22, ICMTV_MAX }
#define ICMTVRANGE_23_PLUS { ICMTV_23, ICMTV_MAX }
#define ICMTVRANGE_24_PLUS { ICMTV_24, ICMTV_MAX }
#define ICMTVRANGE_40_PLUS { ICMTV_40, ICMTV_MAX }
#define ICMTVRANGE_41_PLUS { ICMTV_41, ICMTV_MAX }
#define ICMTVRANGE_42_PLUS { ICMTV_42, ICMTV_MAX }
#define ICMTVRANGE_43_PLUS { ICMTV_43, ICMTV_MAX }
#define ICMTVRANGE_44_PLUS { ICMTV_44, ICMTV_MAX }

extern const icmTVRange icmtvrange_all;
extern const icmTVRange icmtvrange_none;

extern const icmTVRange icmtvrange_20_plus;
extern const icmTVRange icmtvrange_21_plus;
extern const icmTVRange icmtvrange_22_plus;
extern const icmTVRange icmtvrange_23_plus;
extern const icmTVRange icmtvrange_24_plus;
extern const icmTVRange icmtvrange_40_plus;
extern const icmTVRange icmtvrange_41_plus;
extern const icmTVRange icmtvrange_42_plus;
extern const icmTVRange icmtvrange_43_plus;
extern const icmTVRange icmtvrange_44_plus;


/* icmTV from a icmVers */
#define ICMVERS2TV(vers)  (((vers).majv * 100 + (vers).minv) * 100 + (vers).bfv)

/* Return true if icc version is within range */
int icmVersInRange(struct _icc *icp, const icmTVRange *tvr); 

/* Return true if the two version ranges have an overlap */
int icmVersOverlap(icmTVRange *tvr1, const icmTVRange *tvr2); 

/* Return a string that shows the icc version */
char *icmProfileVers2str(struct _icc *icp);

/* [Table] A record of tag type, valid version range and constructor. */
typedef	struct {
	icTagTypeSignature  ttype;			/* The tag type signature */
	icmTVRange        vrange;			/* File versions it is valid to use it in (inclusive) */
	icmBase *(*new_obj)(struct _icc *icp, icTagTypeSignature ttype);
} icmTagTypeVersConstrRec;


/* transform purpose - used to add semantics to lut tags */
typedef enum {
	icmTPNone    = 0,		/* Not Applicable */

	/* Lut purposes - determines in/out colorspace */
	icmTPLutFwd     = 1,	/* AtoBn:    Device to PCS */
	icmTPLutBwd     = 2,	/* BtoAn:    PCS to Device */
	icmTPLutGamut   = 3,	/* Gamut:    PCS to Gray */
	icmTPLutPreview = 4 	/* Preview:	 PCS to PCS */
} icmLutPurpose;

/* A record of tag type, valid version range and constructor. */
typedef	struct {
	icTagTypeSignature  ttype;			/* The tag type signature */
	icmTVRange          vrange;			/* File versions it is valid to use it in (inclusive) */
} icmTagTypeAndVersRec;

/* [Table] A record of tag types permitted for each tag sig */
typedef struct {
	icTagSignature       sig;
	icmTVRange           vrange;		/* Versions it is valid over (inclusive) */
	icmLutPurpose        purp;			/* Lut purpose enum */
	icmTagTypeAndVersRec ttypes[5];		/* ttypes and file versions ttype can be used with sig */
										/* Compatibility is intersection of TT vers & this vers. */
										/* Last has ttype icMaxEnumTagType */
} icmTagSigVersTypesRec;


/* A record of a tag sig and file versions it is valid to use it in */
typedef	struct {
	icTagSignature      sig;			/* The tag signature */
	icmTVRange          vrange;			/* File versions it is valid to use it in (inclusive) */
} icmTagSigVersRec;


/* Colorspace match flag */
typedef enum {
    icmCSMF_ANY = 1, 		/* Any colorspace */
    icmCSMF_XYZ = 2, 		/* icSigXYZData is a match */
    icmCSMF_Lab = 3, 		/* icSigLabData is a match */
    icmCSMF_PCS = 4, 		/* icSigXYZData or icSigLabData is a match */
    icmCSMF_DEV = 5, 		/* Device colorspace (not PCS or Luv/Yxy) */
    icmCSMF_NCOL = 6, 		/* Device N-color space */
    icmCSMF_NOT_NCOL = 7	/* Any but a device N-color space */
} icmCSMF;

/* Colorspace matching requirements - true if matches all. */
typedef struct {
	icmCSMF mf;				/* Match flags */ 
	int min, max;			/* Device min and max channel count, inclusive, */
							/* Ignored if either is 0 */
} icmCSMR;

#define ICMCSMR_ANYN 			{ icmCSMF_ANY,          0, 0 } 
#define ICMCSMR_ANY(NN) 		{ icmCSMF_ANY,          NN, NN } 
#define ICMCSMR_XYZ 			{ icmCSMF_XYZ,          0, 0 } 
#define ICMCSMR_LAB 			{ icmCSMF_LAB,          0, 0 } 
#define ICMCSMR_PCS 			{ icmCSMF_PCS,          0, 0 } 
#define ICMCSMR_NCOL 			{ icmCSMF_NCOL,         0, 0 } 
#define ICMCSMR_NOT_NCOL		{ icmCSMF_NOT_NCOL,     0, 0 } 
//#define ICMCSMR_DEVN			{ icmCSMF_DEV,          1, 15 } 	// Not used because profile
//#define ICMCSMR_DEV(NN)		{ icmCSMF_DEV,         NN, NN } 	// "device" space can be any

/* [Table] Class signature instance tag requirements */
typedef struct {
	icProfileClassSignature clsig;		/* Profile class signature (i.e. input/output/link etc.) */
	icmCSMR                 colsig;		/* Data Color space signature */
	icmCSMR                 pcssig;		/* PCS Color space signature */
	icmTVRange          	vrange;		/* File versions it is valid to use it in (inclusive) */
	icmTagSigVersRec        tags[16];	/* Required Tags, terminated by icMaxEnumTag */
										/* Compatibility is intersection of T vers & this vers. */
										/* Last has sig icMaxEnumTagType */
	icmTagSigVersRec        otags[16];	/* Optional Tags, terminated by icMaxEnumTag */
} icmRequiredTagType;

/* xform creation tag signature details */
typedef struct {
	icTagSignature		sig;		/* Signature to create */
	icTagTypeSignature	ttype;		/* Tag Type to create */
} icmXformSigs;

/* - - - - - - - - - - - - - - - - - - - */
/* Pseudo enumerations valid as parameter to get_luobj(): */

/* Special purpose Perceptual intent */
#define icmAbsolutePerceptual ((icRenderingIntent)97)

/* Special purpose Saturation intent */
#define icmAbsoluteSaturation ((icRenderingIntent)98)

/* To be specified where an intent is not appropriate */
#define icmDefaultIntent ((icRenderingIntent)99)


/* Pseudo PCS colospace used to indicate the native PCS */
#define icmSigDefaultData ((icColorSpaceSignature) 0x0)

/* The ICC object */
struct _icc {
  /* Public: */
	icmFile     *(*get_rfp)(struct _icc *p);			/* Return the current read fp (if any) */
	icmTV        (*get_version)(struct _icc *p); /* Return file version in icmTV format, 0 on err */
	int          (*set_version)(struct _icc *p, icmTV ver); /* For creation */

	void         (*set_cflag)(struct _icc *p, icmCompatFlags flags); /* Set | compatibility flags */
	void         (*unset_cflag)(struct _icc *p, icmCompatFlags flags); /* Unset &~ compat. flags */
	icmCompatFlags (*get_cflags)(struct _icc *p);					   /* Return compat. flags */
	void         (*set_vcrange)(struct _icc *p, const icmTVRange *tvr);
															/* Set icmCFlagAllowWrVersion */ 
															/* Use ICMTVRANGE_NONE to disable */
	void		 (*clear_err)(struct _icc *p);					       /* Clear any errors */
	unsigned int (*get_size)(struct _icc *p);				/* Return total size needed, 0 = err. */
															/* Will add auto tags (i.e. 'arts') */
	int          (*read)(struct _icc *p, icmFile *fp, unsigned int of);	/* Returns error code */
	int          (*write)(struct _icc *p, icmFile *fp, unsigned int of);/* Returns error code */
															/* Will add auto tags (i.e. 'arts') */
	void         (*dump)(struct _icc *p, icmFile *op, int verb);	/* Dump whole icc */
	void         (*del)(struct _icc *p);						/* Free whole icc */

	int          (*find_tag)(struct _icc *p, icTagSignature sig);
							/* Returns 0 if found, 1 if found but not readable, 2 of not found */
	icmBase *    (*read_tag)(struct _icc *p, icTagSignature sig); /* Returns pointer to object */
										/* Returns NULL if not found and doesn't set e.c, e.m) */
	icmBase *    (*read_tag_any)(struct _icc *p, icTagSignature sig);	/* (Convenience method) */
								/* Returns pointer to object, but may be icmSigUnknownType! */
	icmBase *    (*add_tag)(struct _icc *p, icTagSignature sig, icTagTypeSignature ttype);
															/* Returns pointer to object */
	int          (*rename_tag)(struct _icc *p, icTagSignature sig, icTagSignature sigNew);
															/* Rename and existing tag */
	icmBase *    (*link_tag)(struct _icc *p, icTagSignature sig, icTagSignature ex_sig);
															/* Returns pointer to object */
	int          (*unread_tag)(struct _icc *p, icTagSignature sig);
														/* Unread a tag (free on refcount == 0 */
	int          (*read_all_tags)(struct _icc *p); 		/* Read all the tags, non-zero on error. */

	int          (*delete_tag)(struct _icc *p, icTagSignature sig);	/* Delet a tag. Error if not */
															/* Returns 0 if deleted OK */
	int          (*delete_tag_quiet)(struct _icc *p, icTagSignature sig); /* Delet a tag quietly */
										/* No error if tag doesn't exist. Returns 0 if deleted OK */

	icmBase *    (*new_ttype)(struct _icc *p, icTagTypeSignature ttype, icTagTypeSignature pttype);
										/* Create tagtype for embedding in tagtype.  pttype is */
	icmPe *      (*new_pe)(struct _icc *p, icTagTypeSignature ttype, icTagTypeSignature pttype);
										/* Create icmPe for icmMultiProcessElements.  pttype is */
										/* parent tag-type. Returns pointer to object */
	int          (*copy_ttype)(struct _icc *p, icmBase *dst, icmBase *src); /* Copy & translate */ 
										/* tagtype from same/other icc. Returns error code. */

	int          (*check_id)(struct _icc *p, ORD8 *id);	/* Check profiles MD5 id */
												/* Returns 0 if ID is OK, 1 if not present etc. */
	int          (*write_check)(struct _icc *p);	/* Check profile is legal to write. */
													/* Return error code. */

	int          (*get_wb_points)(struct _icc *p,
	                 int *wpassumed, icmXYZNumber *wp, int *bpassumed, icmXYZNumber *bp,
	                 double toAbs[3][3], double fromAbs[3][3]);
								/* Return the white and black points from the ICC tags, */
								/* and the corresponding absolute<->relative conversion matrices. */
								/* Any argument may be NULL */
								/* This method undoes any V4 style 'chrm' adapation so that the */
								/* returned white & black represent the actual media color. */
								/* The wpassumed flag returns 1 if the white point tag */
								/* is missing because this type of profile doesn't have a wp tag. */
								/* The bpassumed flag returns 1 if the black point tag */
								/* is missing. Return nz on error and sets error codes. */
	double       (*get_tac)(struct _icc *p, double *chmax,
	                        void (*calfunc)(void *cntx, double *out, double *in), void *cntx);
 	                           /* Returns total ink limit and channel maximums. -1 on error. */
	                           /* calfunc is optional. */
	icmLutPurpose  (*get_tag_lut_purpose)(struct _icc *p, icTagSignature sig);
									/* Return the lut purpose enum for a tag signature. */
									/* Return icmTPNone if there is no sigtypetable, */
									/* or the signature is not recognised. */
	void         (*set_illum)(struct _icc *p, double ill_wp[3]);
								/* Clear any existing 'chad' matrix, and if Output type profile */
	                            /* and ARGYLL_CREATE_OUTPUT_PROFILE_WITH_CHAD set and */
								/* ill_wp != NULL, create a 'chad' matrix. */
	void         (*chromAdaptMatrix)(struct _icc *p, int flags, double imat[3][3],
	                                 double mat[3][3], icmXYZNumber d_wp, icmXYZNumber s_wp);
	                            /* Return an overall Chromatic Adaptation Matrix */
	                            /* for the given source and destination white points. */
	                            /* This will depened on the icc profiles current setup */
	                            /* for Abs->Rel conversion (wpchtmx[][] set to wrong Von */
	                            /* Kries or not, whether 'arts' tag has been read), and */
	                            /* whether an Output profile 'chad' tag has bean read */
	                            /* or will be created. (i.e. on writing assumes */
	                            /* icc->set_illum() called or not). */
	                            /* Set header->deviceClass before calling this! */
	                            /* ICM_CAM_NONE or ICM_CAM_MULMATRIX to mult by mat. */
	                            /* Return inverse of matrix if imat != NULL. */
	                            /* Use icmMulBy3x3(dst, mat, src). */
	                            /* NOTE that to transform primaries they */
	                            /* must be mat[XYZ][RGB] format! */

	/* Get a particular color conversion function */
	icmLuBase *  (*get_luobj) (struct _icc *p,
                               icmLookupFunc func,			/* Functionality */
	                           icRenderingIntent intent,	/* Intent */
	                           icColorSpaceSignature pcsor,	/* PCS override (0 = def) */
	                           icmLookupOrder order);		/* Search Order */
	                           /* Return appropriate lookup object */
	                           /* NULL on error, check errc+err for reason */

	/* Helper function to set multiple Monochrome tags simultaneously. */
	/* Note that these tables and matrix value all have to be */
	/* compatible in having the same configuration and resolutions. */
	/* Set errc and return error number in underlying icc */
	/* Note that clutfunc in[] value has "index under". */
	/* Returns ec */
	int (*create_mono_xforms) (
		struct _icc *icp,
		int     flags,					/* Setting flags */
		void   *cbctx,					/* Opaque callback context pointer value */

		int ntables,					/* Number of tables to be set, 1..n */
		icmXformSigs *sigs,				/* signatures and tag types for each table */
										/* ICCV2 Matrix/gamma/shaper uses icmSigShaperMatrix */
										/* Valid combinations are:
										  icmSigShaperMono + icmSigShaperMatrixType 
										  icSigAToB0Tag + icSigLut16Type
										  	(icSigBToA0Tag + icSigLutB16Type automatically added) */

		unsigned int	inputEnt,       /* Num of in-table entries */
		unsigned int	inv_inputEnt,   /* Num of inverse in-table entries */

		icColorSpaceSignature insig, 	/* Input color space */
		icColorSpaceSignature outsig, 	/* Output color space */

		void (*infunc)(void *cbctx, double *out, double *in, int tn)
							/* Input transfer function, inspace->inspace' (NULL = default) */
							/* Will be called ntables times each input grid value */
	);


	/* Helper function to set multiple Matrix tags simultaneously. */
	/* Note that these tables and matrix value all have to be */
	/* compatible in having the same configuration and resolutions. */
	/* Set errc and return error number in underlying icc */
	/* Note that clutfunc in[] value has "index under". */
	/* Returns ec */
	int (*create_matrix_xforms) (
		struct _icc *icp,
		int     flags,					/* Setting flags */
		void   *cbctx,					/* Opaque callback context pointer value */

		int ntables,					/* Number of tables to be set, 1..n */
		icmXformSigs *sigs,				/* signatures and tag types for each table */
										/* ICCV2 Matrix/gamma/shaper uses icmSigShaperMatrix */
										/* Valid combinations are:
										  icmSigShaperMatrix + icmSigShaperMatrixType */ 

		unsigned int	inputEnt,       /* Num of in-table entries */
		unsigned int	inv_inputEnt,   /* Num of inverse in-table entries */

		icColorSpaceSignature insig, 	/* Input color space */
		icColorSpaceSignature outsig, 	/* Output color space */

		void (*infunc)(void *cbctx, double *out, double *in, int tn),
							/* Input transfer function, inspace->inspace' (NULL = default) */
							/* Will be called ntables times each input grid value */

		double mx[3][3],	/* [outn][inn] Matrix values */
		double ct[3],		/* [outn] Constant values added after mx[][] multiply */
							/* Must be NULL or 0.0 if a table uses icmSigShaperMatrix */

		int isShTRC,		/* NZ if shared TRCs */
		double *gamma,		/* != NULL if gamma rather than shaper */
		int isLinear		/* NZ if pure linear, gamma = 1.0 */
	);

	/* Helper function to set multiple Lut tables simultaneously. */
	/* Note that these tables all have to be compatible in */
	/* having the same configuration and resolutions, and the */
	/* same per channel input and output curves. */
	/* Set errc and return error number in underlying icc */
	/* Note that clutfunc in[] value has "index under". */
	/* Returns ec */
	int (*create_lut_xforms) (
		struct _icc *icp,
		int     flags,					/* Setting flags */
		void   *cbctx,					/* Opaque callback context pointer value */

		int ntables,					/* Number of tables to be set, 1..n */
		icmXformSigs *sigs,				/* signatures and tag types for each table */

		unsigned int	bpv,			/* Bytes per value of AToB or BToA CLUT, 1 or 2 */
		unsigned int	inputEnt,       /* Num of in-table entries (must be 256 for Lut8 */
		unsigned int	clutPoints[MAX_CHAN],    /* Num of grid points (must be same for Lut8/16) */
		unsigned int	outputEnt,      /* Num of out-table entries (must be 256 for Lut8) */

		icColorSpaceSignature insig, 	/* Input color space */
		icColorSpaceSignature outsig, 	/* Output color space */

		double *inmin, double *inmax,	/* Maximum range of inspace values (Not used) */
										/* (NULL = default) */
		void (*infunc)(void *cbctx, double *out, double *in, int tn),
							/* Input transfer function, inspace->inspace' (NULL = default) */
							/* Will be called ntables times each input grid value */
		double *indmin, double *indmax,	/* Maximum range of inspace' values */
										/* (NULL = same as inmin/max or default) */
		void (*clutfunc)(void *cbntx, double *out, double *in, int tn),
							/* inspace' -> outspace[ntables]' transfer function */
							/* will be called once for each input' grid value, and ntable times */
							/* Note that in[] value has "index under". */
		double *clutmin, double *clutmax,	/* Maximum range of outspace' values */
											/* (NULL = default) */
		void (*outfunc)(void *cbntx, double *out, double *in, int tn),
								/* Output transfer function, outspace'->outspace (NULL = deflt) */
								/* Will be called ntables times on each output value */

		int *apxls_gmin, int *apxls_gmax/* If not NULL, the grid indexes not to be affected */
										/* by ICM_CLUT_SET_APXLS, defaulting to 0..>clutPoints-1 */
	);

	
	/* - - - - - - - - tweaks - - - - - - - */

	int              allowclutPoints256; /* Non standard - allow 256 res cLUT */

	int              useLinWpchtmx;		/* Force Wrong Von Kries for output class (default false) */
										/* Could be set by code, and is set set by */
										/* ARGYLL_CREATE_WRONG_VON_KRIES_OUTPUT_CLASS_REL_WP env. */
    icProfileClassSignature wpchtmx_class;	/* Class of profile wpchtmx was set for. */
										/* Set wpchtmx automatically on ->chromAdaptMatrix() */
										/* or ->get_size() or ->write() if the profile class */
										/* doesn't match wpchtmx_class. */
	double           wpchtmx[3][3];		/* Absolute to Media Relative Transformation Space (i.e. */
										/* Cone Space transformation) matrix. */
	double           iwpchtmx[3][3];	/* Inverse of wpchtmx[][] */
										/* (Default is Bradford matrix) */
	int				 useArts;			/* Save ArgyllCMS private 'arts' tag (default yes). */
										/* This creates 'arts' tag on writing. */
										/* This is cleared if 'arts' tag is not found on reading. */

	double           illwp[3];			/* Output type profile illuminant white point, */
										/* used to create 'chad' tag if */
										/* ARGYLL_CREATE_OUTPUT_PROFILE_WITH_CHAD is set. */
	int              illwpValid;		/* illwp[] has been set. */

	int              naturalChad;		/* nz if 'chad' tag is naturally present in the profile, */
										/* because it was read or added. wrD/OChad will be */
										/* ignored if this is the case. */

	int              chadmxValid;		/* nz if 'chad' tag has been read, or created */
										/* using ->set_illum() and chadmx is valid. */
	double			 chadmx[3][3];		/* 'chad' tag matrix if read or created. */
										/* (This is used to restore full absolute intent for */
	                                    /* Display and Output type profiles.) */

	int				 wrDChad;			/* Create 'chad' tag for Display profiles (default no). */
										/* Override with ARGYLL_CREATE_DISPLAY_PROFILE_WITH_CHAD. */
										/* On writing Display profiles this sets the media white */
										/* point tag to D50 and creates 'chad' tag from the white */
										/* point to D50. Ignored if naturalChad. */
		
	int				 wrOChad;			/* Create 'chad' tag for Output profiles (default no). */
										/* Override with ARGYLL_CREATE_DISPLAY_PROFILE_WITH_CHAD. */
										/* On writing an Output profile, this  Creates a 'chad' */
										/* tag frp, illwp[] to D50, and sets the white point */
										/* tag to be the real value transformed by the */
										/* 'chad' matrix. Ignored if naturalChad. */

	int              tempChad;			/* nz while temporary chad tag is in place and */
										/* white point has been modified, i.e. inside */
										/* ->get_size() or ->write() calls when wrD/OChad active. */
	icmXYZNumber	 tempWP;			/* Save real wp tag value here inside ->get_size() */
										/* or ->write() calls when wrD/OChad active. */

	icmXYZNumber	 tempBP;			/* Save real bp tag value here inside ->get_size() */
										/* or ->write() calls when wrD/OChad active. */
		
  /* - - - - - - - - - -*/


	void         (*warning)(struct _icc *p, int err, const char *format, va_list vp);
									/* Installable warning callback function, */
									/* which is NULL by default. */

	int              warnc;			/* Clip warning code */
									/* Set by icmLut_set_tables(), icmSetMultiLutTables() */

  /* Private: ? */
	icmErr         e;				/* Common error reporting */
	icmAlloc      *al;				/* Heap allocator reference */
	icmFile       *rfp;				/* Read File associated with object reference */
	icmFile       *wfp;				/* Write File associated with object reference */
	unsigned int of;				/* Offset of the profile within the file */

	unsigned int  align;			/* Alignment for tag table and tags */
    icmHeader      *header;			/* The header */
    unsigned int  _count;			/* Allocated tags in tagTable */
    unsigned int   count;			/* Number tags in the profile */
	icmTagRec     *data;    		/* The tagTable and tagData */
	unsigned int  pttsize;			/* Padded tag table size */

	unsigned int  mino;				/* Minimum offset of tags */
	unsigned int  maxs;				/* Maximum size available for tagtable & tags */

	icmCompatFlags cflags;          /* Compatibility flags */
	icmTVRange     vcrange;			/* icmCFlagAllowWrVersion range */
	icmSnOp        op;				/* tag->check() icmSnRead or icmSnWrite */
	int            rdff;			/* Currently reading tags from a file */

	icmTagTypeVersConstrRec *tagtypetable;	/* Table of Tag Types valid version range and */
											/* constructor. Last has ttype icMaxEnumTagType */

	icmTagSigVersTypesRec *tagsigtable;		/* Table of Tags valid version range and possible */
											/* Tag Types.  Last has sig icMaxEnumTag */

	icmRequiredTagType *classtagtable;		/* Table of Class Signature instance requirements */
											/* and optional. Last has sig icMaxEnumClass */

	icmTagSigVersRec *transtagtable;		/* Table of transform tags subject to classtagtable */
											/* requirements. */

}; typedef struct _icc icc;


/* ========================================================== */
/* Utility structures and declarations */

/* Type of encoding for argument to icm2str() */
typedef enum {
    icmScreenEncodings,
    icmDeviceAttributes,
	icmProfileHeaderFlags,
	icmAsciiOrBinaryData,
	icmVideoCardGammaFormat,
	icmTagSig,
	icmTagSig_alt,						/* Show alias name */
	icmTypeSig,
	icmColorSpaceSig,
	icmProfileClassSig,
	icmPlatformSig,
	icmDeviceManufacturerSig,
	icmDeviceModelSig,
	icmCMMSig,
	icmTechnologySig,
	icmMeasurementGeometry,
	icmRenderingIntent,
	icmSpotShape,
	icmStandardObserver,
	icmIlluminant,
	icmLanguageCode,
	icmRegionCode,
	icmDevSetMsftID,
	icmDevSetMsftMedia,
	icmDevSetMsftDither,
	icmMeasUnitsSig,
	icmPhColEncoding,
	icmTransformLookupFunc,
	icmTransformLookupOrder,
	icmProcessingElementOp,				/* attr.op */
	icmProcessingElementTag,
	icmTransformType,					/* Lu4 */
	icmTransformLookupAlgorithm,		/* Lu2 */
	icmTransformSourceTag				/* Lu4 */
} icmEnumType;

/* Return a string description of the given enumeration value */
extern ICCLIB_API const char *icm2str(icmEnumType etype, int enumval);

/* Return a (static) string containing CSInfo sig, nch, min, max */
char *icmCSInfo2str(icmCSInfo *p); 

/* Return a string that shows the XYZ number value */
/* Returned buffer is static */
char *icmXYZNumber2str(icmXYZNumber *p);

/* Return a string that shows the XYZ number value, */
/* and the Lab D50 number in paren. */
/* Returned string is static */
char *icmXYZNumber_and_Lab2str(icmXYZNumber *p);

/* Return a string that shows the given date and time */
/* Returned buffer is static */
char *icmDateTimeNumber2str(icmDateTimeNumber *p);

/* ============================================== */
/* MD5 checksum support */
 
/* A helper object that computes MD5 checksums */
struct _icmMD5 {
  /* Private: */
	int refcount;			/* Reference count for this object */
	icmAlloc *al;			/* Heap allocator reference */
	int fin;				/* Flag, nz if final has been called */
	ORD32 sum[4];			/* Current/final checksum */
	unsigned int tlen;		/* Total length added in bytes */
	ORD8 buf[64];			/* Partial buffer */

  /* Public: */
	void (*reset)(struct _icmMD5 *p);	/* Reset the checksum */
	void (*add)(struct _icmMD5 *p, ORD8 *buf, unsigned int len);	/* Add some bytes */
	void (*get)(struct _icmMD5 *p, ORD8 chsum[16]);		/* Finalise and get the checksum */
	struct _icmMD5 *(*reference)(struct _icmMD5 *p);	/* Take a reference */
	void (*del)(struct _icmMD5 *p);		/* We're done with the object */

}; typedef struct _icmMD5 icmMD5;

/* Create a new MD5 checksumming object, with a reset checksum value */
/* Return it or NULL if there is an error */
/* Note that this will fail if e has an error already set */
icmMD5 *new_icmMD5_a(icmErr *e, icmAlloc *al);

/* If SEPARATE_STD not defined: */
/* Note that this will fail if e has an error already set */
extern ICCLIB_API icmMD5 *new_icmMD5(icmErr *e);


/* Implementation of file access class to compute an MD5 checksum */
struct _icmFileMD5 {
	ICM_FILE_BASE

	/* Private: */
	icmErr e;			/* Error record */
	icmAlloc *al;		/* Heap allocator */
	icmMD5 *md5;		/* MD5 object */
	unsigned int of;	/* Current offset */

	/* Private: */
	size_t size;    	/* Size of the file (For read) */

}; typedef struct _icmFileMD5 icmFileMD5;

/* Create a dumy file access class with allocator */
icmFile *new_icmFileMD5_a(icmMD5 *md5, icmAlloc *al);

/* ========================================================== */
/* Public function declarations */
/* Create an empty object. Return null on error */
/* If e not NULL, return error code and message */
/* Note that this will fail if e has an error already set */
extern ICCLIB_API icc *new_icc_a(icmErr *e, icmAlloc *al);		/* With allocator class */

/* If SEPARATE_STD not defined: */
/* Note that this will fail if e has an error already set */
extern ICCLIB_API icc *new_icc(icmErr *e);				/* Default allocator */

/* ========================================================== */
/* Utility function declarations are in their own file */

#include "icc_util.h"

#ifdef __cplusplus
	}
#endif

#endif /* ICC_H */


