/**
 * @File  mris_graidient.c
 * @brief program for computing the gradient of a surface-based vector field.
 *
 * Compute the gradient of a surface based vector field by first computing the individual
 * partial derivatives using a least squared fit to the local neighborhood using a Taylor
 * expansion f1 = f0 + dx * delX1 + dy *delY1 ..., then use the Froebenius norm to collapse this
 * to a scalar field
 */
/*
 * Original Author: Bruce Fischl
 * CVS Revision Info:
 *    $Author: fischl $
 *    $Date: 2013/03/27 01:53:50 $
 *    $Revision: 1.23 $
 *
 * Copyright © 2011 The General Hospital Corporation (Boston, MA) "MGH"
 *
 * Terms and conditions for use, reproduction, distribution and contribution
 * are found in the 'FreeSurfer Software License Agreement' contained
 * in the file 'LICENSE' found in the FreeSurfer distribution, and here:
 *
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferSoftwareLicense
 *
 * Reporting: freesurfer@nmr.mgh.harvard.edu
 *
 */



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "macros.h"
#include "error.h"
#include "diag.h"
#include "proto.h"
#include "mrisurf.h"
#include "mri.h"
#include "macros.h"
#include "version.h"

static char vcid[] = "$Id: mris_gradient.c,v 1.23 2013/03/27 01:53:50 fischl Exp $";

char *Progname ;
int main(int argc, char *argv[]) ;

#define FROBENIUS_NORM  0

MRI *MRIScomputeGradient(MRI_SURFACE *mris, MRI *mri, int which_norm, MRI *mri_grad) ;

static int  get_option(int argc, char *argv[]) ;
static void usage_exit(void) ;
static void print_usage(void) ;
static void print_help(void) ;
static void print_version(void) ;

static int nbhd_size = 3 ;

int
main(int argc, char *argv[]) {
  char          *out_fname ;
  int           nargs ;
  MRI_SURFACE   *mris ;
  MRI           *mri, *mri_grad ;

  nargs = handle_version_option (argc, argv, "$Id: mris_gradient.c,v 1.23 2013/03/27 01:53:50 fischl Exp $", "$Name:  $");
  if (nargs && argc - nargs == 1)
    exit (0);
  argc -= nargs;

  Progname = argv[0] ;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;

  for ( ; argc > 1 && ISOPTION(*argv[1]) ; argc--, argv++) {
    nargs = get_option(argc, argv) ;
    argc -= nargs ;
    argv += nargs ;
  }

  if (argc < 4)
    usage_exit() ;

  mris = MRISread(argv[1]) ; 
  if (!mris)
    ErrorExit(ERROR_NOFILE, "%s: could not read surface file %s", Progname, argv[1]) ;
  mri = MRIread(argv[2]) ;
  if (!mri)
    ErrorExit(ERROR_NOFILE, "%s: could not read intensity volume %s", Progname, argv[2]) ;
  out_fname = argv[3] ;

  MRISresetNeighborhoodSize(mris, nbhd_size) ;

  mri_grad = MRIScomputeGradient(mris, mri, FROBENIUS_NORM, NULL) ;
  printf("writing output gradient to %s\n", out_fname) ;
  MRIwrite(mri_grad, out_fname) ;
  MRIfree(&mri) ;

  exit(0) ;
  return(0) ;  /* for ansi */
}

/*----------------------------------------------------------------------
  Parameters:

  Description:
  ----------------------------------------------------------------------*/
static int
get_option(int argc, char *argv[]) {
  int  nargs = 0 ;
  char *option ;

  option = argv[1] + 1 ;            /* past '-' */
  if (!stricmp(option, "-help"))
    print_help() ;
  else if (!stricmp(option, "-version"))
    print_version() ;
  else switch (toupper(*option)) {
    case 'N':
      nbhd_size = atoi(argv[2]) ;
      fprintf(stderr, "using neighborhood size=%d\n", nbhd_size) ;
      nargs = 1 ;
      break ;
    case 'V':
      Gdiag_no = atoi(argv[2]) ;
      nargs = 1 ;
      break ;
    case '?':
    case 'U':
      print_usage() ;
      exit(1) ;
      break ;
    default:
      fprintf(stderr, "unknown option %s\n", argv[1]) ;
      exit(1) ;
      break ;
    }

  return(nargs) ;
}

static void
usage_exit(void) {
  print_usage() ;
  exit(1) ;
}

static void
print_usage(void) {
  fprintf(stderr,
          "usage: %s [options] <input surface> <input vector field> <output gradient file>\n",
          Progname) ;
}

static void
print_help(void) {
  print_usage() ;
  fprintf(stderr,
          "\nThis program computes the gradient of an intensity profile of the cortical ribbon\n"
          "and writes the resulting measurement into a .mgz file\n"
          "<output file>.\n") ;
  fprintf(stderr, "\nvalid options are:\n\n") ;
  exit(1) ;
}

static void
print_version(void) {
  fprintf(stderr, "%s\n", vcid) ;
  exit(1) ;
}

MRI *
MRIScomputeGradient(MRI_SURFACE *mris, MRI *mri, int which_norm, MRI *mri_grad) 
{
  int      vno, nbad = 0 ;

  if (mri_grad == NULL)
    mri_grad = MRIallocSequence(mris->nvertices, 1, 1, MRI_FLOAT, mri->nframes+1) ;


  MRIScomputeSecondFundamentalForm(mris) ;

  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    MATRIX   *m_df, *m_p, *m_x, *m_xtx, *m_inv, *m_xt, *m_tmp ;
    int      vno2, n, frame ;
    VERTEX   *v, *vn ;
    double   norm = 0.0, val1, val0, x1, y1, dx, dy ;

    v = &mris->vertices[vno] ;
    if (vno == Gdiag_no)
      DiagBreak() ;
    if (v->ripflag)
      continue ;
      
    m_x = MatrixAlloc(v->vtotal, 2, MATRIX_REAL) ;   // matrix of delta coordinates
    m_p = MatrixAlloc(2, 1, MATRIX_REAL) ;            // [dx,dy]
    m_df = MatrixAlloc(v->vtotal, 1, MATRIX_REAL) ;   // vector of changes in function val
    m_tmp = m_xt = m_xtx = m_inv = m_p = NULL ; 
    for (n = 0 ; n < v->vtotal ; n++)
    {
      vno2 = v->v[n] ; vn = &mris->vertices[vno2] ;
      if (vno2 == Gdiag_no)
	DiagBreak() ;
      x1 = v->e1x * (vn->x-v->x) + v->e1y * (vn->y-v->y) + v->e1z * (vn->z-v->z);
      y1 = v->e2x * (vn->x-v->x) + v->e2y * (vn->y-v->y) + v->e2z * (vn->z-v->z);
      *MATRIX_RELT(m_x, n+1, 1) = x1 ;
      *MATRIX_RELT(m_x, n+1, 2) = y1 ;
    }
    m_xt = MatrixTranspose(m_x, NULL) ;
    m_xtx = MatrixMultiply(m_xt, m_x, NULL) ;
    if (MatrixConditionNumber(m_xtx) > 1000)
    {
      nbad++ ;
      DiagBreak() ;
    }
    m_inv = MatrixSVDInverse(m_xtx, NULL) ;
    if (m_inv == NULL)
    {
      MatrixFree(&m_x) ; MatrixFree(&m_xt) ; MatrixFree(&m_xtx) ;
      continue ;
    }
    m_tmp = MatrixMultiply(m_inv, m_xt, m_tmp) ;

    // everything except m_df is independent of data - just depends on geometry
    for (frame = 0 ; frame < mri->nframes ; frame++)
    {
      val0 = MRIgetVoxVal(mri, vno, 0, 0, frame) ;
      for (n = 0 ; n < v->vtotal ; n++)
      {
	vno2 = v->v[n] ; 
	val1 = MRIgetVoxVal(mri, vno2, 0, 0, frame) ;
	*MATRIX_RELT(m_df, n+1, 1) = val1 - val0 ;
      }

      m_p = MatrixMultiply(m_tmp, m_df, m_p) ;
      dx = *MATRIX_RELT(m_p, 1, 1) ;
      dy = *MATRIX_RELT(m_p, 1, 2) ;
      if (!devFinite(dx))
	dx = 0 ;
      if (!devFinite(dy))
	dy = 0 ;
      norm += dx*dx + dy*dy ;
      MRIsetVoxVal(mri_grad, vno, 0, 0, frame+1, dx*dx + dy*dy) ;
    }

    MRIsetVoxVal(mri_grad, vno, 0, 0, 0, sqrt(norm/mri->nframes)) ;
    MatrixFree(&m_x) ; MatrixFree(&m_p) ; MatrixFree(&m_df) ; MatrixFree(&m_xt) ; MatrixFree(&m_xtx) ; MatrixFree(&m_tmp) ;
    MatrixFree(&m_inv) ;
  }

  return(mri_grad) ;
}


