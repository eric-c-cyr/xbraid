/*BHEADER**********************************************************************
 * Copyright (c) 2013, Lawrence Livermore National Security, LLC. 
 * Produced at the Lawrence Livermore National Laboratory. Written by 
 * Jacob Schroder, Rob Falgout, Tzanio Kolev, Ulrike Yang, Veselin 
 * Dobrev, et al. LLNL-CODE-660355. All rights reserved.
 * 
 * This file is part of XBraid. Email xbraid-support@llnl.gov for support.
 * 
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License (as published by the Free Software
 * Foundation) version 2.1 dated February 1999.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the terms and conditions of the GNU General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 ***********************************************************************EHEADER*/


#include "c_array.h"

void bop6g(double t, double_array_2d *q06_ )
{

/* !********************************************************************** */
/* !** */
/* !** First derivative 6/3 SBP boundary operator, general case. */
/* !** */
/* !** Input: t - Free parameter in operator */
/* !**            t=0.7037 gives small error constant */
/* !** Output: q06 - Boundary operator coefficients. */
/* !** */
/* !********************************************************************** */
   double t1, t3, t23, t30, t46, t48;
#define q06(i,j) compute_index_2d(q06_, i, j)
   
          
   t1 = 43200.0/13649.0*t;
   t3 = 172800.0/13649.0*t;
   t23 = 86400.0/2711.0*t;
   t30 = 86400.0/5359.0*t;
   t46 = 43200.0/43801.0*t;
   t48 = 172800.0/43801.0*t;
   q06(1,1) = -21600.0/13649.0;
   q06(1,2) = -7624.0/40947.0+t1;
   q06(1,3) = 715489.0/81894.0-t3;
   q06(1,4) = -187917.0/13649.0+259200.0/13649.0*t;
   q06(1,5) = 735635.0/81894.0-t3;
   q06(1,6) = -89387.0/40947.0+t1;
   q06(1,7) = 0;
   q06(1,8) = 0;
   q06(1,9) = 0;
   q06(2,1) = 7624.0/180195.0-8640.0/12013.0*t;
   q06(2,2) = 0;
   q06(2,3) = -57139.0/12013.0+86400.0/12013.0*t;
   q06(2,4) = 745733.0/72078.0-172800.0/12013.0*t;
   q06(2,5) = -91715.0/12013.0+129600.0/12013.0*t;
   q06(2,6) = 240569.0/120130.0-34560.0/12013.0*t;
   q06(2,7) = 0;
   q06(2,8) = 0;
   q06(2,9) = 0;
   q06(3,1) = -715489.0/162660.0+17280.0/2711.0*t;
   q06(3,2) = 57139.0/5422.0-43200.0/2711.0*t;
   q06(3,3) = 0;
   q06(3,4) = -176839.0/8133.0+t23;
   q06(3,5) = 242111.0/10844.0-t23;
   q06(3,6) = -182261.0/27110.0+25920.0/2711.0*t;
   q06(3,7) = 0;
   q06(3,8) = 0;
   q06(3,9) = 0;
   q06(4,1) = 187917.0/53590.0-25920.0/5359.0*t;
   q06(4,2) = -745733.0/64308.0+t30;
   q06(4,3) = 176839.0/16077.0-t30;
   q06(4,4) = 0;
   q06(4,5) = -165041.0/32154.0+43200.0/5359.0*t;
   q06(4,6) = 710473.0/321540.0-17280.0/5359.0*t;
   q06(4,7) = 72.0/5359.0;
   q06(4,8) = 0;
   q06(4,9) = 0;
   q06(5,1) = -147127.0/47262.0+34560.0/7877.0*t;
   q06(5,2) = 91715.0/7877.0-129600.0/7877.0*t;
   q06(5,3) = -242111.0/15754.0+172800.0/7877.0*t;
   q06(5,4) = 165041.0/23631.0-86400.0/7877.0*t;
   q06(5,5) = 0;
   q06(5,6) = 8640.0/7877.0*t;
   q06(5,7) = -1296.0/7877.0;
   q06(5,8) = 144.0/7877.0;
   q06(5,9) = 0;
   q06(6,1) = 89387.0/131403.0-t46;
   q06(6,2) = -240569.0/87602.0+t48;
   q06(6,3) = 182261.0/43801.0-259200.0/43801.0*t;
   q06(6,4) = -710473.0/262806.0+t48;
   q06(6,5) = -t46;
   q06(6,6) = 0;
   q06(6,7) = 32400.0/43801.0;
   q06(6,8) = -6480.0/43801.0;
   q06(6,9) = 720.0/43801.0;
#undef q06
}

/*c-----------------------------------------------------------------------*/
void diffusion_coeff_4( double_array_1d *iop2_, double_array_2d *bop2_, double *gh2, double bder[5] )
{
/* iop2(5) */
/* bop2(4,6) */
/* bder[0...4] */
#define iop2(i) compute_index_1d(iop2_, i)
#define bop2(i,j) compute_index_2d(bop2_, i, j)

/* SBP operator of order 2/4. 5pt stencil. */

/* *** Norm  */
/*       h(1) = 17d0/48 */
/*       h(2) = 59d0/48 */
/*       h(3) = 43d0/48 */
/*       h(4) = 49d0/48 */

/* interior operator */
   iop2(1) = -1.0/12;
   iop2(2) =  4.0/3;
   iop2(3) = -5.0/2;
   iop2(4) =  4.0/3;
   iop2(5) = -1.0/12;

/* Second derivative boundary modified stencil, 2nd order truncation error */

/* ghost point coefficient */
   *gh2 = 12.0/17.0;

/* interior stencil */
   bop2(1,1) = -14.0/17.0;
   bop2(1,2) = -13.0/17.0;
   bop2(1,3) = 20.0/17.0;
   bop2(1,4) = -5.0/17.0;
   bop2(1,5) = 0;
   bop2(1,6) = 0;

   bop2(2,1) = 1.0;
   bop2(2,2) = -2.0;
   bop2(2,3) = 1.0;
   bop2(2,4) = 0;
   bop2(2,5) = 0;
   bop2(2,6) = 0;

   bop2(3,1) = -4.0/43.0;
   bop2(3,2) = 59.0/43.0;
   bop2(3,3) = -110.0/43.0;
   bop2(3,4) = 59.0/43.0;
   bop2(3,5) = -4.0/43.0;
   bop2(3,6) = 0;

   bop2(4,1) = -1.0/49.0;
   bop2(4,2) = 0;
   bop2(4,3) = 59.0/49.0;
   bop2(4,4) = -118.0/49.0;
   bop2(4,5) = 64.0/49.0;
   bop2(4,6) = -4.0/49.0;

/* boundary derivative, 4th order, uses ghost point */
   bder[0] = -1.0/4;
   bder[1] = -5.0/6;
   bder[2] =  3.0/2;
   bder[3] = -1.0/2;
   bder[4] =  1.0/12;
#undef iop2
#undef bop2
}

/*c-----------------------------------------------------------------------*/
/*      subroutine WAVEPROPBOP_6( iop, iop2, bop, bop2, gh2, h, s ) */
void diffusion_coeff_6( double_array_1d *iop2_, double_array_2d *bop2_, double *gh2, double bder[7] )
{
/* iop2(7) */
/* bop2(6,9) */
/* bder[0...6] */
#define iop2(i) compute_index_1d(iop2_, i)
#define bop2(i,j) compute_index_2d(bop2_, i, j)

/* *********************************************************************** */
/* ******** SBP operator of order 3/6. 7pt stencil */
/* *********************************************************************** */

/*      real*8 bop2(6,9), gh2, s(0:6), iop2(7) */

/* *** Norm  */
/*       h(1) = 13649.D0/43200.D0 */
/*       h(2) = 12013.D0/8640.D0 */
/*       h(3) = 2711.D0/4320.D0 */
/*       h(4) = 5359.D0/4320.D0 */
/*       h(5) = 7877.D0/8640.D0 */
/*       h(6) = 43801.D0/43200.D0 */


/* *** Second derivative interior, 6th order */
   iop2(1) = 1.0/90;
   iop2(2) =-3.0/20;
   iop2(3) = 3.0/2;
   iop2(4) = -49.0/18;
   iop2(5) = iop2(3);
   iop2(6) = iop2(2);
   iop2(7) = iop2(1);
   

/* *** Second derivative bop, 3rd order */
/* ghost point coeff */
   *gh2 = 7200.0/13649;

/* near bndry points */
   bop2(1,1) = 10490.0/40947.0;
   bop2(1,2) = -178907.0/54596.0;
   bop2(1,3) = 163609.0/40947.0;
   bop2(1,4) = -147397.0/81894.0;
   bop2(1,5) = 3747.0/13649.0;
   bop2(1,6) = 3755.0/163788.0;
   bop2(1,7) = 0;
   bop2(1,8) = 0;
   bop2(1,9) = 0;
   bop2(2,1) = 6173.0/5860.0;
   bop2(2,2) = -2066.0/879.0;
   bop2(2,3) = 3283.0/1758.0;
   bop2(2,4) = -303.0/293.0;
   bop2(2,5) = 2111.0/3516.0;
   bop2(2,6) = -601.0/4395.0;
   bop2(2,7) = 0;
   bop2(2,8) = 0;
   bop2(2,9) = 0;
   bop2(3,1) = -52391.0/81330.0;
   bop2(3,2) = 134603.0/32532.0;
   bop2(3,3) = -21982.0/2711.0;
   bop2(3,4) = 112915.0/16266.0;
   bop2(3,5) = -46969.0/16266.0;
   bop2(3,6) = 30409.0/54220.0;
   bop2(3,7) = 0;
   bop2(3,8) = 0;
   bop2(3,9) = 0;
   bop2(4,1) = 68603.0/321540.0;
   bop2(4,2) = -12423.0/10718.0;
   bop2(4,3) = 112915.0/32154.0;
   bop2(4,4) = -75934.0/16077.0;
   bop2(4,5) = 53369.0/21436.0;
   bop2(4,6) = -54899.0/160770.0;
   bop2(4,7) = 48.0/5359.0;
   bop2(4,8) = 0;
   bop2(4,9) = 0;
   bop2(5,1) = -7053.0/39385.0;
   bop2(5,2) = 86551.0/94524.0;
   bop2(5,3) = -46969.0/23631.0;
   bop2(5,4) = 53369.0/15754.0;
   bop2(5,5) = -87904.0/23631.0;
   bop2(5,6) = 820271.0/472620.0;
   bop2(5,7) = -1296.0/7877.0;
   bop2(5,8) = 96.0/7877.0;
   bop2(5,9) = 0;
   bop2(6,1) = 21035.0/525612.0;
   bop2(6,2) = -24641.0/131403.0;
   bop2(6,3) = 30409.0/87602.0;
   bop2(6,4) = -54899.0/131403.0;
   bop2(6,5) = 820271.0/525612.0;
   bop2(6,6) = -117600.0/43801.0;
   bop2(6,7) = 64800.0/43801.0;
   bop2(6,8) = -6480.0/43801.0;
   bop2(6,9) = 480.0/43801.0;
 
   bder[0] = -1.0/6;
   bder[1] = -77.0/60;
   bder[2] = 5.0/2;
   bder[3] = -5.0/3;
   bder[4] = 5.0/6;
   bder[5] = -1.0/4;
   bder[6] = 1.0/30;

}
