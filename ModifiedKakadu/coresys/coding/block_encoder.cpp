/*****************************************************************************/
// File: block_encoder.cpp [scope = CORESYS/CODING]
// Version: Kakadu, V6.1
// Author: David Taubman
// Last Revised: 28 November, 2008
/*****************************************************************************/
// Copyright 2001, David Taubman, The University of New South Wales (UNSW)
// The copyright owner is Unisearch Ltd, Australia (commercial arm of UNSW)
// Neither this copyright statement, nor the licensing details below
// may be removed from this file or dissociated from its contents.
/*****************************************************************************/
// Licensee: University of Arizona
// License number: 00307
// The licensee has been granted a UNIVERSITY LIBRARY license to the
// contents of this source file.  A brief summary of this license appears
// below.  This summary is not to be relied upon in preference to the full
// text of the license agreement, accepted at purchase of the license.
// 1. The License is for University libraries which already own a copy of
//    the book, "JPEG2000: Image compression fundamentals, standards and
//    practice," (Taubman and Marcellin) published by Kluwer Academic
//    Publishers.
// 2. The Licensee has the right to distribute copies of the Kakadu software
//    to currently enrolled students and employed staff members of the
//    University, subject to their agreement not to further distribute the
//    software or make it available to unlicensed parties.
// 3. Subject to Clause 2, the enrolled students and employed staff members
//    of the University have the right to install and use the Kakadu software
//    and to develop Applications for their own use, in their capacity as
//    students or staff members of the University.  This right continues
//    only for the duration of enrollment or employment of the students or
//    staff members, as appropriate.
// 4. The enrolled students and employed staff members of the University have the
//    right to Deploy Applications built using the Kakadu software, provided
//    that such Deployment does not result in any direct or indirect financial
//    return to the students and staff members, the Licensee or any other
//    Third Party which further supplies or otherwise uses such Applications.
// 5. The Licensee, its students and staff members have the right to distribute
//    Reusable Code (including source code and dynamically or statically linked
//    libraries) to a Third Party, provided the Third Party possesses a license
//    to use the Kakadu software, and provided such distribution does not
//    result in any direct or indirect financial return to the Licensee,
//    students or staff members.  This right continues only for the
//    duration of enrollment or employment of the students or staff members,
//    as appropriate.
/******************************************************************************
Description:
   Implements the embedded block coding algorithm, including distortion
estimation and R-D covex hull analysis, in addition to the coding
passes themselves.  The low level services offered by the MQ arithmetic coder
appear in "mq_encoder.cpp" and "mq_encoder.h".
******************************************************************************/
#include <stdio.h>
#include <iostream>
#include <math.h>
#include <string.h>
#include <assert.h>
#include "kdu_messaging.h"
#include "kdu_block_coding.h"
#include "block_coding_common.h"
#include "mq_encoder.h"


// FILE* pThresh = fopen("thresholds.txt","a");


using namespace std;

double integral_1(kdu_block *block, double var, double T, int level) {
	double dx[1000] = {};
	double ans = 0;
	double a = 0;
	double b = 0;
	double c = 0;
	double pd = 0;


	dx[0] = -10 * var;


	for (int i = 1; i < 1000; i++) {
		dx[i] = dx[i - 1] + var / 50;
	}


	double var_1 = 0;

	if (fabs(var) > 1) var_1 =fabs(log(var)+1);
	else var_1 = var;

	//if (block->comp_idx == 0)c = exp(T / var_1 + 0.4);
	//else c = exp(T / var_1 + 1);
	//c = exp(T/var_1+1)-exp(1)+1;
	c = exp(T /(var_1));
	//if (fabs(var) > exp(1))c = exp(T);
	//else c = exp(T / var+0.3);
	
	return c;

}

double vt_mask(kdu_block *block, double T, double var, int orient, int level) {
	double m;
	if (orient != 0) {
		m = integral_1(block, var, T, level);
	}
	else {
		//m = 1;
		m = integral_1(block, var, T, level);
	}
	return m;
}

double distj2k(int band, double sig2, double tb, double e) {
	double pin;
	double z = 0;
	//band =1;
	if (band == 0) {
		pin = tb / sqrt(3 * sig2);
	}
	else {
		pin = 1 - exp(-sqrt(2 / sig2)*tb);
	}

	if (band == 0) {
		if (fabs(e) <= tb / 2) {
			z = 1 / sqrt(12 * sig2) + (1 - pin) / tb;
		}
		if ((fabs(e) > tb / 2)&(fabs(e) <= tb)) {
			z = 1 / sqrt(12 * sig2);
		}
	}
	else {
		if (fabs(e) <= tb / 2) {
			z = exp(-sqrt(2 / sig2)*fabs(e)) / sqrt(2 * sig2) + (1 - pin) / tb;
		}
		if ((fabs(e) > tb / 2)&(fabs(e) <= tb)) {
			z = exp(-sqrt(2 / sig2)*fabs(e)) / sqrt(2 * sig2);

		}
	}
	return z;

}

double f2(double beta, double band, double sig2, double tb, double t0, double x) {
	double y;
	y = distj2k(band, sig2, tb, x)*exp(-pow((fabs(x / t0)), beta));
	//y = distj2k(band, sig2, tb, x)*pow(4,-pow((fabs(x / t0)), beta));
	return y;


}

//Romberg
double integral2(double beta, int band, double sig2, double tb, double t0) {
	double eps1 = 1e-15;
	double eps2 = 1e-11;
	//默认精度

	double h = tb / 2;
	double T1[20][20], T2[20][20];
	double n1 = 1;
	double n2 = 1;
	int k1 = 0;
	int k2 = 0;
	double err1 = 1;
	double err2 = 1;
	double S, x;
	double I1, I2;
	//自适应积分前半段，精度为1e-16

	T1[0][0] = h*(f2(beta, band, sig2, tb, t0, 0) + f2(beta, band, sig2, tb, t0, tb / 2)) / 2;



	while (err1 > eps1) {
		k1 = k1 + 1;
		S = 0;
		for (int ii = 0; ii < n1; ii++) {
			x = 0 + h*(2 * ii + 1) / 2;
			S = S + f2(beta, band, sig2, tb, t0, x);
		}
		T1[k1][0] = (T1[k1 - 1][0] + h*S) / 2;

		for (int m = 0; m < k1; m++) {
			T1[k1][m + 1] = (pow(4, m + 1)*T1[k1][m] - T1[k1 - 1][m]) / (pow(4, m + 1) - 1);
		}

		err1 = fabs(T1[k1][k1] - T1[k1][k1 - 1]);
		n1 = 2 * n1;
		h = h / 2;
	}
	I1 = 2 * T1[k1][k1];

	//自适应积分后半段，精度为1e-12；
	h = tb / 2;

	T2[0][0] = h*(f2(beta, band, sig2, tb, t0, tb / 2) + f2(beta, band, sig2, tb, t0, tb)) / 2;



	while (err2 > eps2) {
		k2 = k2 + 1;
		S = 0;
		for (int ii = 0; ii < n2; ii++) {
			x = tb / 2 + h*(2 * ii + 1) / 2;
			S = S + f2(beta, band, sig2, tb, t0, x);
		}
		T2[k2][0] = (T2[k2 - 1][0] + h*S) / 2;

		for (int m = 0; m < k2; m++) {
			T2[k2][m + 1] = (pow(4, m + 1)*T2[k2][m] - T2[k2 - 1][m]) / (pow(4, m + 1) - 1);
		}

		err2 = fabs(T2[k2][k2] - T2[k2][k2 - 1]);
		n2 = 2 * n2;
		h = h / 2;
	}
	I2 = 2 * T2[k2][k2];

	return I1 + I2;

}

//n=2 插值法积分
//double integral2(double beta, int band, double sig2, double tb, double t0) {
//	double dx1[101] = { 0 };
//	double dx2[1001] = { 0 };
//	double len, area;
//	double ans1 = 0, ans2 = 0;
//
//
//	dx1[0] = 0;
//	for (int i = 1; i < 101; i++) {
//		dx1[i] = dx1[i - 1] + 1e-5;
//	}
//	for (int i = 0; i < 101; i++) {
//		if ((i == 0) || (i == 100)) {
//			ans1 = ans1 + distj2k(band, sig2, tb, dx1[i])*exp(-pow((fabs(dx1[i] / t0)), beta));
//		}
//		else {
//			if (i % 2 != 0) {
//				ans1 = ans1 + 4 * distj2k(band, sig2, tb, dx1[i])*exp(-pow((fabs(dx1[i] / t0)), beta));
//			}
//			else {
//				ans1 = ans1 + 2 * distj2k(band, sig2, tb, dx1[i])*exp(-pow((fabs(dx1[i] / t0)), beta));
//			}
//		}
//	}
//
//
//
//	if (tb > 1e-3) {
//		dx2[0] = 1e-3;
//		len = (tb - 1e-3) / 1000;
//		for (int i = 1; i < 1001; i++) {
//			dx2[i] = dx2[i - 1] + len;
//		}
//		for (int i = 0; i < 1001; i++) {
//			if ((i == 0) || (i == 1000)) {
//				ans2 = ans2 + distj2k(band, sig2, tb, dx2[i])*exp(-pow((fabs(dx2[i] / t0)), beta));
//			}
//			else {
//				if (i % 2 != 0) {
//					ans2 = ans2 + 4 * distj2k(band, sig2, tb, dx2[i])*exp(-pow((fabs(dx2[i] / t0)), beta));
//				}
//				else {
//					ans2 = ans2 + 2 * distj2k(band, sig2, tb, dx2[i])*exp(-pow((fabs(dx2[i] / t0)), beta));
//				}
//			}
//		}
//	}
//
//	area = (ans1*(1e-5) / 3 + ans2*len / 3) * 2;
//	return area;
//}

double integral3(kdu_block *block,double tb,double t0) {

	int num_coeffs = block->size.get_y()*block->size.get_x();

	double ans = 0;
	double err=0;
	double sample_val=0;
	int q=0;

	for (int i = 0; i < num_coeffs; i++) {
		sample_val = (double)(block->sample_buffer[i] & 0x7fffffff) / (double)(1 << (31 - block->k_max));
		if (fabs(sample_val < (tb / 2))) err = sample_val;
		else {
			q = fabs(ceil(2*sample_val/tb));
			err = fabs(sample_val - (q + 0.5)*tb / 2);
		}
		ans = ans+exp(-fabs(pow(err / t0, 2)));
	}
	
	ans = ans / num_coeffs;

	return ans;




}//精确到每个小波系数的积分



//double t02tb_fov(double t0, double Q, double beta, int band, double sig2, double level, double pitch, double dist, double r, kdu_block *block) {
//	double tb = 128;
//	double pi = 3.1415926;
//	double jnd, n0, n;
//	double ptrans, pcan1, pcan2, pcanm;
//	double tbcan1, tbcan2, tbcanm;
//
//	int size_x = block->size.get_x();
//	int size_y = block->size.get_y();
//	int num_coeffs = block->size.get_y()*block->size.get_x();
//	int pos_x, pos_y;
//	int i, j;
//	double mask = 0;
//
//	double ans = 0;
//	double err = 0;
//	int q = 0;
//
//	double sample_val = 0;
//	double sample_var = 0;
//	double sample_mean = 0;
//	double sample_mean2 = 0;
//
//
//
//
//
//	if (isinf(t0))
//		tb = 128;
//	else {
//		jnd = 1 / Q;
//		n0 = 2 * round((2 * dist*tan(pi / 180)) / (2 * pitch));
//		n = round(n0*pow(2, -level) / r);
//		ptrans = -jnd / pow(n, 2);
//
//		//cout << ptrans << endl;
//		if((n<size_x)&&(n<size_y)){
//			for (pos_x = 0; pos_x < size_x - n; pos_x++) {
//				for (pos_y = 0; pos_y < size_y - n; pos_y++) {
//					for (i = pos_x; i < pos_x + n; i++) {
//						for (j = pos_y; j < pos_y + n; j++) {
//							sample_val = (double)(block->sample_buffer[i + j*size_x] & 0x7fffffff) / (double)(1 << (31 - block->k_max));
//							sample_mean2 += sample_val;
//							if (block->sample_buffer[i + j*size_x] < 0)
//								sample_val = -sample_val;
//							sample_var += pow(sample_val, 2.0);
//							sample_mean += sample_val;
//						}
//					}
//					sample_mean = 1.0 / (double)pow(n, 2)*sample_mean;
//					sample_mean2 = 1.0 / (double)pow(n, 2)*sample_mean2*256.0*(double)block->delta;
//					sample_var = 1.0 / (double)pow(n, 2)*sample_var - pow(sample_mean, 2.0);
//					sample_var *= pow((double)256 * block->delta, 2.0);
//
//					err++;
//					ans += sample_var;
//
//					double var_1 = 0;
//
//					if (fabs(sample_var) > exp(1)) var_1 = fabs(log(sample_var));
//					else var_1 = sample_var;
//					mask += exp(t0 / var_1 + 0.6);
//
//				}
//
//			}
//			ans = ans / err;
//			mask = mask / err;
//
//
//			ans = pow(ans, 2);
//
//			
//
//
//
//			tbcan1 = 0.001;
//			pcan1 = 0;
//			//cout << pcan1 << endl;
//			tbcan2 = 128;
//
//
//			pcan2 = log(integral2(beta, band, ans, tbcan2, mask*t0));
//			//pcan2 = log(integral3(block, tbcan2, t0));
//			//cout << pcan2 << endl;
//			if ((pcan2 > ptrans) || (pcan1 < ptrans)) {
//				if (pcan2 > ptrans) {
//					tb = tbcan2;
//				}
//				if (pcan1 < ptrans) {
//					tb = tbcan1;
//				}
//			}
//			else {
//				while (fabs(tbcan2 - tbcan1)>1e-5) {
//					tbcanm = (tbcan1 + tbcan2) / 2;
//					pcanm = log(integral2(beta, band, ans, tbcanm, mask*t0));
//					//pcanm = log(integral3(block, tbcanm, t0));
//					if (pcanm > ptrans) {
//						tbcan1 = tbcanm;
//					}
//					else {
//						tbcan2 = tbcanm;
//					}
//				}
//				tb = (tbcan1 + tbcan2) / 2;
//			}
//		}
//		else {
//			jnd = 1 / Q;
//			n0 = 2 * round((2 * dist*tan(pi / 180)) / (2 * pitch));
//			n = round(n0*pow(2, -level) / r);
//			ptrans = -jnd / pow(n, 2);
//
//			double var_1 = 0;
//
//			if (fabs(pow(sig2, 0.5)) > exp(1)) var_1 = fabs(log(pow(sig2, 0.5)));
//			else var_1 = pow(sig2, 0.5);
//			mask = exp(t0 / var_1 + 0.6);
//
//			//cout << ptrans << endl;
//
//			tbcan1 = 0.001;
//			pcan1 = 0;
//			//cout << pcan1 << endl;
//			tbcan2 = 128;
//			pcan2 = log(integral2(beta, band, sig2, tbcan2, mask*t0));
//			//pcan2 = log(integral3(block, tbcan2, t0));
//			//cout << pcan2 << endl;
//			if ((pcan2 > ptrans) || (pcan1 < ptrans)) {
//				if (pcan2 > ptrans) {
//					tb = tbcan2;
//				}
//				if (pcan1 < ptrans) {
//					tb = tbcan1;
//				}
//			}
//			else {
//				while (fabs(tbcan2 - tbcan1)>1e-5) {
//					tbcanm = (tbcan1 + tbcan2) / 2;
//					pcanm = log(integral2(beta, band, sig2, tbcanm, mask*t0));
//					//pcanm = log(integral3(block, tbcanm, t0));
//					if (pcanm > ptrans) {
//						tbcan1 = tbcanm;
//					}
//					else {
//						tbcan2 = tbcanm;
//					}
//				}
//				tb = (tbcan1 + tbcan2) / 2;
//			}
//
//
//
//		}
//		
//	}
//
//	
//
//		
//	return tb;
//
//}



double t02tb(double t0, double Q, double beta, int band, double sig2, double level, double pitch, double dist, double r,kdu_block *block) {
	double tb = 1024;
	double pi = 3.1415926;
	double jnd, n0, n;
	double ptrans, pcan1, pcan2, pcanm;
	double tbcan1, tbcan2, tbcanm;
	int mode = 1;
	//mode=0 fast for Big image ; mode=1 slow for Small image 

	if (isinf(t0))
		tb = 1024;
	else {
		jnd = 1 / Q;
		n0 = 2 * round((2 * dist*tan(pi / 180)) / (2 * pitch));
		n = round(n0*pow(2, -level) / r);
		ptrans = -jnd/ pow(n, 2);

		//cout << ptrans << endl;

		tbcan1 = 0.001;
		pcan1 = 0;
		//cout << pcan1 << endl;
		tbcan2 = 128;
		if (mode == 1) { 
			pcan2 = log(integral2(beta, band, sig2, tbcan2, t0)); 
		}
		else {
			pcan2 = log(integral3(block, tbcan2, t0));
		}
		
		//cout << pcan2 << endl;
		if ((pcan2 > ptrans) || (pcan1 < ptrans)) {
			if (pcan2 > ptrans) {
				tb = tbcan2;
			}
			if (pcan1 < ptrans) {
				tb = tbcan1;
			}
		}
		else {
			while (fabs(tbcan2 - tbcan1)>1e-5) {
				tbcanm = (tbcan1 + tbcan2) / 2;
				if (mode == 1) {
					pcanm = log(integral2(beta, band, sig2, tbcanm, t0));
				}
				else {
					pcanm = log(integral3(block, tbcanm, t0));
				}			
				if (pcanm > ptrans) {
					tbcan1 = tbcanm;
				}
				else {
					tbcan2 = tbcanm;
				}
			}
			tb = (tbcan1 + tbcan2) / 2;
		}
	}
	return tb;

}
double findT(double orient, int idx, int level, double r) {
	//when r=1
	double T_10[11][3] = { { 20.235,72.9174,49.9917 },{ 5.1708,38.9918,13.9723 },{ 2.3472,18.2878,7.4274 },{ 1.2316,7.8627,3.8368 },{ 0.68222,4.1152,2.0679 },{ 0.56547,2.5473,1.2809 },{ 0.38727,2.4183,0.84813 },{ 0.37089,1.7732,0.52714 },{ 0.29783,1.0925,0.4013 },{ 0.29969,0.51604,0.38628 },{ 0.35564,0.46997,0.32541 } };
	// r=0.75
	double T_7[11][3] = {{35.9166,156.8109,91.3263},{8.9598,66.9556,23.4314},{3.6854,27.3663,10.7422},{2.0145,13.1087,6.0959},{0.96784,6.016,2.9516},{0.79871,3.9716,1.9293},{0.46171,2.301,1.0542},{0.45407,2.1057,0.77371},{0.30831,1.3873,0.45552},{0.3278,0.82176,0.44623},{0.43689,0.61009,0.42041}};
	// r=0.5
	double T_5[11][3] = {{3.187575748021499e+16,2.258712675256643e+17,5.732595014481563e+16},{1.742334781358004e+16,1.238513920039133e+17,3.178804596302343e+16},{20.235,72.9174,49.9917},{5.1708,38.9918,13.9723},{2.3472,18.2878,7.4274},{1.2316,7.8627,3.8368},{0.68222,4.1152,2.0679},{0.56547,2.5473,1.2809},{0.38727,2.4183,0.84813},{0.37089,1.7732,0.52714},{0.57837,0.92133,0.6392}};

	double T_LL[6][3] = { {15.0024,85.5760,32.2044},{3.6813,25.7815,6.1920},{1.1289,7.4004,2.4926},{0.4274,2.4367,0.6028},{0.3564,1.1799,0.4100},{0.3556,0.4700,0.3254} };
	
	int i, j;
	double T_final;

	double Bigvalue = 1e+15;

	if (orient != 0) {
		if (orient == 3) {
			i = 2 * level - 2;
		}
		else {
			i = 2 * level - 1;
		}
	}
	else { i = 10; }
	j = idx;

	if (r >= 0.8) {
		T_final = T_10[i][j];
	}
	else if (r < 0.8 && r >= 0.65) {
		T_final = T_7[i][j];
	}
	else if (r < 0.65 && r>= 0.375){
		T_final = T_5[i][j];
	}
	else {
		int u = 1;
		while ( r < (1 / pow(2, u + 1) + 1 / pow(2, u + 2))) {
			u++;
		}

		if (i == 10) {
			if( 5 - u >= 0 ){ 
				T_final = T_LL[5 - u][j]; 
			}
			else {
				T_final = Bigvalue;
			}

		}
		else {
			if (i - 2 * u < 0) {
				T_final = Bigvalue;
			}
			else {
				T_final = T_10[i - 2 * u][j];
				
			}
		}
	}

	return T_final;
}

double VTcal(kdu_block *block, double var, int orient, int idx, int level,double Q,double r) {






	


	//T values

	
	double m, sig2, vt,T;

	T = findT(orient, idx, level, r);

	sig2 = var;

	//m = vt_mask(block, T, sig2, orient, level);
	m = (38 - log(sig2)) / pow(2, level);
	

	double mask;


	//double rho;
	if (r <= 1 && r>0.85) {
		mask = 8 - 2 * fabs(3 - level);
	}
	else if (r <= 0.85 && r >0.6) {
		mask = 10 - 2 * fabs(3 - level);
		//mask = 8 - 2.2 * fabs(3 - level);
	}
	else {
		//mask = 10 / pow(2, fabs(3 - level));
		mask = 8 - 3 * fabs(3 - level);
	}
	//mask= 10 / pow(2, fabs(3 - level));



	if (idx == 0) {
		mask = mask;
	}
	if (idx == 1) {
		mask = 0.8*mask;
	}
	if (idx == 2) {
		mask = 1.2*mask;
	}
	//调整分量几乎不影响SSIM的结果，但会使HDRVDP的效果更强


	//mask = mask / (1 - log10(10*Q));

	//if (orient == 0)mask = mask*(2 - 0.2*level);
	

	int i;
	if (orient != 0) {
		if (orient == 3) {
			i = 2 * level - 2;
		}
		else {
			i = 2 * level - 1;
		}
	}
	else { i = 10; }

			

	
	
	//double m1;
	if (orient != 0) {
			vt = m*t02tb(mask*T, Q, 2.0, 1, sig2, level, 0.1845, 600, r, block);	
	}
	else {
			vt = m*t02tb(mask*T, Q, 2.0, 1, sig2, 5, 0.1845, 600, r, block);	
		
	}
	//printf("vt=%lf\n",vt);
	return vt;
}
//findVT modify by jyh


static kdu_byte *significance_luts[4] =
  {lh_sig_lut, hl_sig_lut, lh_sig_lut, hh_sig_lut};

#define DISTORTION_LSBS 5
#define SIGNIFICANCE_DISTORTIONS (1<<DISTORTION_LSBS)
#define REFINEMENT_DISTORTIONS (1<<(DISTORTION_LSBS+1))

static kdu_int32 significance_distortion_lut[SIGNIFICANCE_DISTORTIONS];
static kdu_int32 significance_distortion_lut_lossless[SIGNIFICANCE_DISTORTIONS];
static kdu_int32 refinement_distortion_lut[REFINEMENT_DISTORTIONS];
static kdu_int32 refinement_distortion_lut_lossless[REFINEMENT_DISTORTIONS];

#define EXTRA_ENCODE_CWORDS 3 // Number of extra context-words between stripes.
#define MAX_POSSIBLE_PASSES (31*3-2)

/* ========================================================================= */
/*                   Local Class and Structure Definitions                   */
/* ========================================================================= */

/*****************************************************************************/
/*                             kd_block_encoder                              */
/*****************************************************************************/

class kd_block_encoder : public kdu_block_encoder_base {
  /* Although we can supply a constructor and a virtual destructor in the
     future, we have no need for these for the moment. */
  protected:
    void encode(kdu_block *block, bool reversible, double msb_wmse,
                kdu_uint16 estimated_slope_threshold);
  };

/* ========================================================================= */
/*            Initialization of Distortion Estimation Tables                 */
/* ========================================================================= */

static void initialize_significance_distortion_luts();
static void initialize_refinement_distortion_luts();
static class encoder_local_init {
    public: encoder_local_init()
              { initialize_significance_distortion_luts();
                initialize_refinement_distortion_luts(); }
  } _do_it;

/*****************************************************************************/
/* STATIC          initialize_significance_distortion_luts                   */
/*****************************************************************************/

#define KD_DISTORTION_LUT_SCALE ((double)(1<<16))

static void
  initialize_significance_distortion_luts()
{
  for (kdu_int32 n=0; n < SIGNIFICANCE_DISTORTIONS; n++)
    {
      kdu_int32 idx = n | (1<<DISTORTION_LSBS);
      double v_tilde = ((double) idx) / ((double)(1<<DISTORTION_LSBS));
      assert((v_tilde >= 1.0) && (v_tilde < 2.0));
      double sqe_before = v_tilde*v_tilde;
      double sqe_after = (v_tilde-1.5)*(v_tilde-1.5);
      significance_distortion_lut[n] = (int)
        floor(0.5 + KD_DISTORTION_LUT_SCALE*(sqe_before-sqe_after));
      significance_distortion_lut_lossless[n] = (int)
        floor(0.5 + KD_DISTORTION_LUT_SCALE*sqe_before);
    }
}

/*****************************************************************************/
/* STATIC            initialize_refinement_distortion_luts                   */
/*****************************************************************************/

static void
  initialize_refinement_distortion_luts()
{
  for (kdu_int32 n=0; n < REFINEMENT_DISTORTIONS; n++)
    {
      double v_tilde = ((double) n) / ((double)(1<<DISTORTION_LSBS));
      assert(v_tilde < 2.0);
      double sqe_before = (v_tilde-1.0)*(v_tilde-1.0);
      v_tilde = (n >> DISTORTION_LSBS)?(v_tilde-1.0):v_tilde;
      assert((v_tilde >= 0.0) && (v_tilde < 1.0));
      double sqe_after = (v_tilde-0.5)*(v_tilde-0.5);
      refinement_distortion_lut[n] = (int)
        floor(0.5 + KD_DISTORTION_LUT_SCALE*(sqe_before-sqe_after));
      refinement_distortion_lut_lossless[n] = (int)
        floor(0.5 + KD_DISTORTION_LUT_SCALE*sqe_before);
    }
}


/* ========================================================================= */
/*             Binding of MQ and Raw Symbol Coding Services                  */
/* ========================================================================= */

static inline
  void reset_states(mqe_state states[])
  /* `states' is assumed to be the 18-element context state table.
     See Table 12.1 in the book by Taubman and Marcellin */
{
  for (int n=0; n < 18; n++)
    states[n].init(0,0);
    states[KAPPA_SIG_BASE].init(4,0);
    states[KAPPA_RUN_BASE].init(3,0);
}


#define USE_FAST_MACROS // Comment this out if you want functions instead.

#ifdef USE_FAST_MACROS
#  define _mq_check_out_(coder)                                     \
     register kdu_int32 A; register kdu_int32 C; register kdu_int32 t; \
     kdu_int32 temp; kdu_byte *store;                                 \
     coder.check_out(A,C,t,temp,store)
#  define _mq_check_in_(coder)                                      \
     coder.check_in(A,C,t,temp,store)
#  define _mq_enc_(coder,symbol,state)                              \
     _mq_encode_(symbol,state,A,C,t,temp,store)
#  define _mq_enc_run_(coder,run)                                   \
     _mq_encode_run_(run,A,C,t,temp,store)

#  define _raw_check_out_(coder)                                    \
     register kdu_int32 t; register kdu_int32 temp; kdu_byte *store;   \
     coder.check_out(t,temp,store)
#  define _raw_check_in_(coder)                                     \
     coder.check_in(t,temp,store)
#  define _raw_enc_(coder,symbol)                                   \
     _raw_encode_(symbol,t,temp,store)
#else // Do not use fast macros
#  define _mq_check_out_(coder)
#  define _mq_check_in_(coder)
#  define _mq_enc_(coder,symbol,state) coder.mq_encode(symbol,state)
#  define _mq_enc_run_(coder,run) coder.mq_encode_run(run)

#  define _raw_check_out_(coder)
#  define _raw_check_in_(coder)
#  define _raw_enc_(coder,symbol) coder.raw_encode(symbol)
#endif // USE_FAST_MACROS

 /* The coding pass functions defined below all return a 32-bit integer,
    which represents the normalized reduction in MSE associated with the
    coded symbols.  Specifically, the MSE whose reduction is returned is
    equal to 2^16 * sum_i (x_i/2^p - x_i_hat/2^p)^2 where x_i denotes the
    integer sample values in the `samples' array and x_i_hat denotes the
    quantized representation available from the current coding pass and all
    previous coding passes, assuming a mid-point reconstruction rule.
       The mid-point reconstruction rule satisfies x_i_hat = (q_i+1/2)*Delta
    where q_i denotes the quantization indices and Delta is the quantization
    step size.  This rule is modified only if the `lossless_pass' argument is
    true, which is permitted only when symbols coded in the coding pass
    result in a lossless representation of the corresponding subband samples.
    Of course, this can only happen in the last bit-plane when the reversible
    compression path is being used.  In this case, the function uses the fact
    that all coded symbols have 0 distortion.
       It should be noted that the MSE reduction can be negative, meaning
    that the coding of symbols actually increases distortion. */


/* ========================================================================= */
/*                           Coding pass functions                           */
/* ========================================================================= */

/*****************************************************************************/
/* STATIC                   encode_sig_prop_pass_raw                         */
/*****************************************************************************/

static kdu_int32
  encode_sig_prop_pass_raw(mq_encoder &coder, int p, bool causal,
                           kdu_int32 *samples, kdu_int32 *contexts,
                           int width, int num_stripes, int context_row_gap,
                           bool lossless_pass)
{
  /* Ideally, register storage is available for 9 32-bit integers. Two
     are declared inside the "_raw_check_out_" macro.  The order of priority
     for these registers corresponds roughly to the order in which their
     declarations appear below.  Unfortunately, none of these register
     requests are likely to be honored by the register-starved X86 family
     of processors, but the register declarations may prove useful to
     compilers for other architectures or for hand optimizations of
     assembly code. */
  register kdu_int32 *cp = contexts;
  register int c;
  register kdu_int32 cword;
  _raw_check_out_(coder); // Declares t and temp as registers.
  register kdu_int32 sym;
  register kdu_int32 val;
  register kdu_int32 *sp = samples;
  register kdu_int32 shift = 31-p; assert(shift > 0);
  int r, width_by2=width+width, width_by3=width_by2+width;
  kdu_int32 distortion_change = 0;
  kdu_int32 *distortion_lut = significance_distortion_lut;
  if (lossless_pass)
    distortion_lut = significance_distortion_lut_lossless;

  assert((context_row_gap - width) == EXTRA_ENCODE_CWORDS);
  for (r=num_stripes; r > 0; r--, cp += EXTRA_ENCODE_CWORDS, sp += width_by3)
    for (c=width; c > 0; c--, sp++, cp++)
      {
        if (*cp == 0)
          continue;
        cword = *cp;
        if ((cword & (NBRHD_MASK<<0)) && !(cword & (SIG_PROP_MEMBER_MASK<<0)))
          { // Process first row of stripe column (row 0)
            val = sp[0]<<shift; // Move bit p to sign bit.
            sym = (kdu_int32)(((kdu_uint32) val)>>31); // Move bit into LSB
            _raw_enc_(coder,sym);
            if (val >= 0) // New magnitude bit was 0, so still insignificant
              { cword |= (PI_BIT<<0); goto row_1; }
            // Compute distortion change
            val =  (val>>(31-DISTORTION_LSBS)) & (SIGNIFICANCE_DISTORTIONS-1);
            distortion_change += distortion_lut[val];
            // Encode sign bit
            sym = sp[0];
            sym = (kdu_int32)(((kdu_uint32) sym)>>31); // Move sign into LSB
            _raw_enc_(coder,sym);
            // Broadcast neighbourhood context changes
            if (!causal)
              {
                cp[-context_row_gap-1] |=(SIGMA_BR_BIT<<9);
                cp[-context_row_gap  ] |=(SIGMA_BC_BIT<<9)|(sym<<NEXT_CHI_POS);
                cp[-context_row_gap+1] |=(SIGMA_BL_BIT<<9);
              }
            cp[-1] |= (SIGMA_CR_BIT<<0);
            cp[1]  |= (SIGMA_CL_BIT<<0);
            cword |= (SIGMA_CC_BIT<<0) | (PI_BIT<<0) | (sym<<CHI_POS);
          }
row_1:
        if ((cword & (NBRHD_MASK<<3)) && !(cword & (SIG_PROP_MEMBER_MASK<<3)))
          { // Process second row of stripe column (row 1)
            val = sp[width]<<shift; // Move bit p to sign bit.
            sym = (kdu_int32)(((kdu_uint32) val)>>31); // Move bit into LSB
            _raw_enc_(coder,sym);
            if (val >= 0) // New magnitude bit was 0, so still insignificant
              { cword |= (PI_BIT<<3); goto row_2; }
            // Compute distortion change
            val =  (val>>(31-DISTORTION_LSBS)) & (SIGNIFICANCE_DISTORTIONS-1);
            distortion_change += distortion_lut[val];
            // Encode sign bit
            sym = sp[width];
            sym = (kdu_int32)(((kdu_uint32) sym)>>31); // Move sign into LSB
            _raw_enc_(coder,sym);
            // Broadcast neighbourhood context changes
            cp[-1] |= (SIGMA_CR_BIT<<3);
            cp[1]  |= (SIGMA_CL_BIT<<3);
            cword |= (SIGMA_CC_BIT<<3) | (PI_BIT<<3) | (sym<<(CHI_POS+3));
          }
row_2:
        if ((cword & (NBRHD_MASK<<6)) && !(cword & (SIG_PROP_MEMBER_MASK<<6)))
          { // Process third row of stripe column (row 2)
            val = sp[width_by2]<<shift; // Move bit p to sign bit.
            sym = (kdu_int32)(((kdu_uint32) val)>>31); // Move bit into LSB
            _raw_enc_(coder,sym);
            if (val >= 0) // New magnitude bit was 0, so still insignificant
              { cword |= (PI_BIT<<6); goto row_3; }
            // Compute distortion change
            val =  (val>>(31-DISTORTION_LSBS)) & (SIGNIFICANCE_DISTORTIONS-1);
            distortion_change += distortion_lut[val];
            // Encode sign bit
            sym = sp[width_by2];
            sym = (kdu_int32)(((kdu_uint32) sym)>>31); // Move sign into LSB
            _raw_enc_(coder,sym);
            // Broadcast neighbourhood context changes
            cp[-1] |= (SIGMA_CR_BIT<<6);
            cp[1]  |= (SIGMA_CL_BIT<<6);
            cword |= (SIGMA_CC_BIT<<6) | (PI_BIT<<6) | (sym << (CHI_POS+6));
          }
row_3:
        if ((cword & (NBRHD_MASK<<9)) && !(cword & (SIG_PROP_MEMBER_MASK<<9)))
          { // Process fourth row of stripe column (row 3)
            val = sp[width_by3]<<shift; // Move bit p to sign bit.
            sym = (kdu_int32)(((kdu_uint32) val)>>31); // Move bit into LSB
            _raw_enc_(coder,sym);
            if (val >= 0) // New magnitude bit was 0, so still insignificant
              { cword |= (PI_BIT<<9); goto done; }
            // Compute distortion change
            val =  (val>>(31-DISTORTION_LSBS)) & (SIGNIFICANCE_DISTORTIONS-1);
            distortion_change += distortion_lut[val];
            // Encode sign bit
            sym = sp[width_by3];
            sym = (kdu_int32)(((kdu_uint32) sym)>>31); // Move sign into LSB
            _raw_enc_(coder,sym);
            // Broadcast neighbourhood context changes
            cp[context_row_gap-1] |= SIGMA_TR_BIT;
            cp[context_row_gap  ] |= SIGMA_TC_BIT | (sym<<PREV_CHI_POS);
            cp[context_row_gap+1] |= SIGMA_TL_BIT;
            cp[-1] |= (SIGMA_CR_BIT<<9);
            cp[1]  |= (SIGMA_CL_BIT<<9);
            cword |= (SIGMA_CC_BIT<<9) | (PI_BIT<<9) | (sym<<(CHI_POS+9));
          }
done:
        *cp = cword;
      }
  _raw_check_in_(coder);
  return distortion_change;
}

/*****************************************************************************/
/* STATIC                    encode_sig_prop_pass                            */
/*****************************************************************************/

static kdu_int32
encode_sig_prop_pass(mq_encoder &coder, mqe_state states[],
					 int p, bool causal, int orientation,
					 kdu_int32 *samples, kdu_int32* dec_samples, kdu_int32 *contexts,
					 int width, int num_stripes, int context_row_gap,
					 bool lossless_pass)
{
	/* Ideally, register storage is available for 12 32-bit integers. Three
	are declared inside the "_mq_check_out_" macro.  The order of priority
	for these registers corresponds roughly to the order in which their
	declarations appear below.  Unfortunately, none of these register
	requests are likely to be honored by the register-starved X86 family
	of processors, but the register declarations may prove useful to
	compilers for other architectures or for hand optimizations of
	assembly code. */
	register kdu_int32 *cp = contexts;
	register int c;
	register kdu_int32 cword;
	_mq_check_out_(coder); // Declares A, C, and t as registers.
	register kdu_int32 sym;
	register kdu_int32 val;
	register kdu_int32 *sp = samples;
	//ohhan
	register kdu_int32 *dec_sp = dec_samples;
	int kx, ky;
	kx = ky = 0;
	register kdu_int32 sign_sample = 0;
	register kdu_int32 shift = 31-p; assert(shift > 0);
	register  kdu_byte *sig_lut = significance_luts[orientation];
	register mqe_state *state_ref;
	int r, width_by2=width+width, width_by3=width_by2+width;
	kdu_int32 distortion_change = 0;
	kdu_int32 *distortion_lut = significance_distortion_lut;
	if (lossless_pass)
		distortion_lut = significance_distortion_lut_lossless;

	assert((context_row_gap - width) == EXTRA_ENCODE_CWORDS);
	for (r=num_stripes; r > 0; r--, cp += EXTRA_ENCODE_CWORDS, sp += width_by3, dec_sp += width_by3, kx += 4, ky = 0)
		for (c=width; c > 0; c--, sp++, dec_sp++, cp++, ky++)
		{
			if (*cp == 0)
			{ // Invoke speedup trick to skip over runs of all-0 neighbourhoods
				for (cp+=3; *cp == 0; cp+=3, c-=3, sp+=3, dec_sp+=3, ky+=3);
				cp-=3;
				continue;
			}

			//ohhan-Debug
			if (kx == 56 && ky == 21)
				kx +=0;

			if (&(dec_sp[0]) != &(dec_samples[kx*64+ky]))
				kx += 0;

			cword = *cp;
			if ((cword & (NBRHD_MASK<<0)) && !(cword & (SIG_PROP_MEMBER_MASK<<0)))
			{ // Process first row of stripe column (row 0)
				state_ref = states+KAPPA_SIG_BASE+sig_lut[cword & NBRHD_MASK];
				val = sp[0]<<shift; // Move bit p to sign bit.
				sym = val & KDU_INT32_MIN;
				_mq_enc_(coder,sym,*state_ref);
				if (val >= 0) // New magnitude bit was 0, so still insignificant
				{ cword |= (PI_BIT<<0); goto row_1; }
				// Compute distortion change
				val =  (val>>(31-DISTORTION_LSBS)) & (SIGNIFICANCE_DISTORTIONS-1);
				distortion_change += distortion_lut[val];

				//ohhan
				dec_sp[0] |= 1<<p;   //true coefficients
				dec_sp[0] |= 1<<(p-1); //mid-point reconstruction
				if (sign_sample = (sp[0]&(1<<31))) 
					dec_sp[0] |= sign_sample;  //sign bit


				// Encode sign bit
				sym = cword & ((CHI_BIT>>3) | (SIGMA_CC_BIT>>3) |
					(CHI_BIT<<3) | (SIGMA_CC_BIT<<3));
				sym >>= 1; // Shift down so that top sigma bit has address 0
				sym |= (cp[-1] & ((CHI_BIT<<0) | (SIGMA_CC_BIT<<0))) >> (1+1);
				sym |= (cp[ 1] & ((CHI_BIT<<0) | (SIGMA_CC_BIT<<0))) >> (1-1);
				sym |= (sym >> (CHI_POS-1-SIGMA_CC_POS)); // Interleave chi & sigma
				val = sign_lut[sym & 0x000000FF];
				state_ref = states + KAPPA_SIGN_BASE + (val>>1);
				sym = val << 31; // Get sign flipping to `sym'
				val = sp[0] & KDU_INT32_MIN; // Get the sign bit
				sym ^= val; // Moves flipped sign bit into `sym'
				_mq_enc_(coder,sym,*state_ref);
				// Broadcast neighbourhood context changes; sign bit is in `val'
				cp[-1] |= (SIGMA_CR_BIT<<0);
				cp[1]  |= (SIGMA_CL_BIT<<0);
				if (val < 0)
				{
					cword |= (SIGMA_CC_BIT<<0) | (PI_BIT<<0) | (CHI_BIT<<0);
					if (!causal)
					{
						cp[-context_row_gap-1] |= (SIGMA_BR_BIT<<9);
						cp[-context_row_gap  ] |= (SIGMA_BC_BIT<<9) | NEXT_CHI_BIT;
						cp[-context_row_gap+1] |= (SIGMA_BL_BIT<<9);
					}
				}
				else
				{
					cword |= (SIGMA_CC_BIT<<0) | (PI_BIT<<0);
					if (!causal)
					{
						cp[-context_row_gap-1] |= (SIGMA_BR_BIT<<9);
						cp[-context_row_gap  ] |= (SIGMA_BC_BIT<<9);
						cp[-context_row_gap+1] |= (SIGMA_BL_BIT<<9);
					}
				}
			}
row_1:
			if ((cword & (NBRHD_MASK<<3)) && !(cword & (SIG_PROP_MEMBER_MASK<<3)))
			{ // Process second row of stripe column (row 1)
				state_ref = states+KAPPA_SIG_BASE+sig_lut[(cword>>3) & NBRHD_MASK];
				val = sp[width]<<shift; // Move bit p to sign bit.
				sym = val & KDU_INT32_MIN;
				_mq_enc_(coder,sym,*state_ref);
				if (val >= 0) // New magnitude bit was 0, so still insignificant
				{ cword |= (PI_BIT<<3); goto row_2; }
				// Compute distortion change
				val =  (val>>(31-DISTORTION_LSBS)) & (SIGNIFICANCE_DISTORTIONS-1);
				distortion_change += distortion_lut[val];

				//ohhan
				dec_sp[width] |= 1<<p;   //true coefficients
				dec_sp[width] |= 1<<(p-1); //mid-point reconstruction
				if (sign_sample = (sp[width]&(1<<31))) 
					dec_sp[width] |= sign_sample;  //sign bit


				// Encode sign bit
				sym = cword & ((CHI_BIT<<0) | (SIGMA_CC_BIT<<0) |
					(CHI_BIT<<6) | (SIGMA_CC_BIT<<6));
				sym >>= 4; // Shift down so that top sigma bit has address 0
				sym |= (cp[-1] & ((CHI_BIT<<3) | (SIGMA_CC_BIT<<3))) >> (4+1);
				sym |= (cp[ 1] & ((CHI_BIT<<3) | (SIGMA_CC_BIT<<3))) >> (4-1);
				sym |= (sym >> (CHI_POS-1-SIGMA_CC_POS)); // Interleave chi & sigma
				val = sign_lut[sym & 0x000000FF];
				state_ref = states + KAPPA_SIGN_BASE + (val>>1);
				sym = val << 31; // Get sign flipping to `sym'
				val = sp[width] & KDU_INT32_MIN; // Get the sign bit
				sym ^= val; // Moves flipped sign bit into `sym'
				_mq_enc_(coder,sym,*state_ref);
				// Broadcast neighbourhood context changes; sign bit is in `val'
				cp[-1] |= (SIGMA_CR_BIT<<3);
				cp[1]  |= (SIGMA_CL_BIT<<3);
				cword |= (SIGMA_CC_BIT<<3) | (PI_BIT<<3);
				val = (kdu_int32)(((kdu_uint32) val)>>(31-(CHI_POS+3))); // SRL
				cword |= val;
			}
row_2:
			if ((cword & (NBRHD_MASK<<6)) && !(cword & (SIG_PROP_MEMBER_MASK<<6)))
			{ // Process third row of stripe column (row 2)
				state_ref = states+KAPPA_SIG_BASE+sig_lut[(cword>>6) & NBRHD_MASK];
				val = sp[width_by2]<<shift; // Move bit p to sign bit.
				sym = val & KDU_INT32_MIN;
				_mq_enc_(coder,sym,*state_ref);
				if (val >= 0) // New magnitude bit was 0, so still insignificant
				{ cword |= (PI_BIT<<6); goto row_3; }
				// Compute distortion change
				val =  (val>>(31-DISTORTION_LSBS)) & (SIGNIFICANCE_DISTORTIONS-1);
				distortion_change += distortion_lut[val];

				//ohhan
				dec_sp[width_by2] |= 1<<p;   //true coefficients
				dec_sp[width_by2] |= 1<<(p-1); //mid-point reconstruction
				if (sign_sample = (sp[width_by2]&(1<<31))) 
					dec_sp[width_by2] |= sign_sample;  //sign bit


				// Encode sign bit
				sym = cword & ((CHI_BIT<<3) | (SIGMA_CC_BIT<<3) |
					(CHI_BIT<<9) | (SIGMA_CC_BIT<<9));
				sym >>= 7; // Shift down so that top sigma bit has address 0
				sym |= (cp[-1] & ((CHI_BIT<<6) | (SIGMA_CC_BIT<<6))) >> (7+1);
				sym |= (cp[ 1] & ((CHI_BIT<<6) | (SIGMA_CC_BIT<<6))) >> (7-1);
				sym |= (sym >> (CHI_POS-1-SIGMA_CC_POS)); // Interleave chi & sigma
				val = sign_lut[sym & 0x000000FF];
				state_ref = states + KAPPA_SIGN_BASE + (val>>1);
				sym = val << 31; // Get sign flipping to `sym'
				val = sp[width_by2] & KDU_INT32_MIN; // Get the sign bit
				sym ^= val; // Moves flipped sign bit into `sym'
				_mq_enc_(coder,sym,*state_ref);
				// Broadcast neighbourhood context changes; sign bit is in `val'
				cp[-1] |= (SIGMA_CR_BIT<<6);
				cp[1]  |= (SIGMA_CL_BIT<<6);
				cword |= (SIGMA_CC_BIT<<6) | (PI_BIT<<6);
				val = (kdu_int32)(((kdu_uint32) val)>>(31-(CHI_POS+6))); // SRL
				cword |= val;
			}
row_3:
			if ((cword & (NBRHD_MASK<<9)) && !(cword & (SIG_PROP_MEMBER_MASK<<9)))
			{ // Process fourth row of stripe column (row 3)
				state_ref = states+KAPPA_SIG_BASE+sig_lut[(cword>>9) & NBRHD_MASK];
				val = sp[width_by3]<<shift; // Move bit p to sign bit.
				sym = val & KDU_INT32_MIN;
				_mq_enc_(coder,sym,*state_ref);
				if (val >= 0) // New magnitude bit was 0, so still insignificant
				{ cword |= (PI_BIT<<9); goto done; }
				// Compute distortion change
				val =  (val>>(31-DISTORTION_LSBS)) & (SIGNIFICANCE_DISTORTIONS-1);
				distortion_change += distortion_lut[val];

				//ohhan
				dec_sp[width_by3] |= 1<<p;   //true coefficients
				dec_sp[width_by3] |= 1<<(p-1); //mid-point reconstruction
				if (sign_sample = (sp[width_by3]&(1<<31))) 
					dec_sp[width_by3] |= sign_sample;  //sign bit


				// Encode sign bit
				sym = cword & ((CHI_BIT<<6) | (SIGMA_CC_BIT<<6) |
					0       | (SIGMA_CC_BIT<<12));
				sym >>= 10; // Shift down so that top sigma bit has address 0
				if (cword < 0) // Use the fact that NEXT_CHI_BIT = 31
					sym |= CHI_BIT<<(12-10);
				sym |= (cp[-1] & ((CHI_BIT<<9) | (SIGMA_CC_BIT<<9))) >> (10+1);
				sym |= (cp[ 1] & ((CHI_BIT<<9) | (SIGMA_CC_BIT<<9))) >> (10-1);
				sym |= (sym >> (CHI_POS-1-SIGMA_CC_POS)); // Interleave chi & sigma
				val = sign_lut[sym & 0x000000FF];
				state_ref = states + KAPPA_SIGN_BASE + (val>>1);
				sym = val << 31; // Get sign flipping to `sym'
				val = sp[width_by3] & KDU_INT32_MIN; // Get the sign bit
				sym ^= val; // Moves flipped sign bit into `sym'
				_mq_enc_(coder,sym,*state_ref);
				// Broadcast neighbourhood context changes; sign bit is in `val'
				cp[context_row_gap-1] |= SIGMA_TR_BIT;
				cp[context_row_gap+1] |= SIGMA_TL_BIT;
				cp[-1] |= (SIGMA_CR_BIT<<9);
				cp[1]  |= (SIGMA_CL_BIT<<9);
				if (val < 0)
				{
					cp[context_row_gap  ] |= SIGMA_TC_BIT | PREV_CHI_BIT;
					cword |= (SIGMA_CC_BIT<<9) | (PI_BIT<<9) | (CHI_BIT<<9);
				}
				else
				{
					cp[context_row_gap  ] |= SIGMA_TC_BIT;
					cword |= (SIGMA_CC_BIT<<9) | (PI_BIT<<9);
				}
			}
done:
			*cp = cword;
		}
		_mq_check_in_(coder);
		return distortion_change;
}

/*****************************************************************************/
/* STATIC                    encode_mag_ref_pass_raw                         */
/*****************************************************************************/

static kdu_int32
  encode_mag_ref_pass_raw(mq_encoder &coder, int p, bool causal,
                          kdu_int32 *samples, kdu_int32 *contexts,
                          int width, int num_stripes, int context_row_gap,
                          bool lossless_pass)
{
  /* Ideally, register storage is available for 9 32-bit integers.
     Two 32-bit integers are declared inside the "_raw_check_out_" macro.
     The order of priority for these registers corresponds roughly to the
     order in which their declarations appear below.  Unfortunately, none
     of these register requests are likely to be honored by the
     register-starved X86 family of processors, but the register
     declarations may prove useful to compilers for other architectures or
     for hand optimizations of assembly code. */
  register kdu_int32 *cp = contexts;
  register int c;
  register kdu_int32 cword;
  _raw_check_out_(coder); // Declares t and temp as registers.
  register kdu_int32 *sp = samples;
  register kdu_int32 sym;
  register kdu_int32 val;
  register kdu_int32 shift = 31-p; // Shift to get new mag bit to sign position
  int r, width_by2=width+width, width_by3=width_by2+width;
  kdu_int32 distortion_change = 0;
  kdu_int32 *distortion_lut = refinement_distortion_lut;
  if (lossless_pass)
    distortion_lut = refinement_distortion_lut_lossless;

  assert((context_row_gap - width) == EXTRA_ENCODE_CWORDS);
  for (r=num_stripes; r > 0; r--, cp += EXTRA_ENCODE_CWORDS, sp += width_by3)
    for (c=width; c > 0; c--, sp++, cp++)
      {
        if ((*cp & ((MU_BIT<<0)|(MU_BIT<<3)|(MU_BIT<<6)|(MU_BIT<<9))) == 0)
          { // Invoke speedup trick to skip over runs of all-0 neighbourhoods
            for (cp+=2; *cp == 0; cp+=2, c-=2, sp+=2);
            cp-=2;
            continue;
          }
        cword = *cp;
        if (cword & (MU_BIT<<0))
          { // Process first row of stripe column
            val = sp[0];
            val <<= shift; // Get new magnitude bit to sign position.
            sym = (kdu_int32)(((kdu_uint32) val)>>31);
            // Compute distortion change
            val =  (val >> (31-DISTORTION_LSBS)) & (REFINEMENT_DISTORTIONS-1);
            distortion_change += distortion_lut[val];
            // Encode magnitude bit
            _raw_enc_(coder,sym);
          }
        if (cword & (MU_BIT<<3))
          { // Process second row of stripe column
            val = sp[width];
            val <<= shift; // Get new magnitude bit to sign position.
            sym = (kdu_int32)(((kdu_uint32) val)>>31);
            // Compute distortion change
            val =  (val >> (31-DISTORTION_LSBS)) & (REFINEMENT_DISTORTIONS-1);
            distortion_change += distortion_lut[val];
            // Encode magnitude bit
            _raw_enc_(coder,sym);
          }
        if (cword & (MU_BIT<<6))
          { // Process third row of stripe column
            val = sp[width_by2];
            val <<= shift; // Get new magnitude bit to sign position.
            sym = (kdu_int32)(((kdu_uint32) val)>>31);
            // Compute distortion change
            val =  (val >> (31-DISTORTION_LSBS)) & (REFINEMENT_DISTORTIONS-1);
            distortion_change += distortion_lut[val];
            // Encode magnitude bit
            _raw_enc_(coder,sym);
          }
        if (cword & (MU_BIT<<9))
          { // Process fourth row of stripe column
            val = sp[width_by3];
            val <<= shift; // Get new magnitude bit to sign position.
            sym = (kdu_int32)(((kdu_uint32) val)>>31);
            // Compute distortion change
            val =  (val >> (31-DISTORTION_LSBS)) & (REFINEMENT_DISTORTIONS-1);
            distortion_change += distortion_lut[val];
            // Encode magnitude bit
            _raw_enc_(coder,sym);
          }
      }
  _raw_check_in_(coder);
  return distortion_change;
}

/*****************************************************************************/
/* STATIC                     encode_mag_ref_pass                            */
/*****************************************************************************/

static kdu_int32
encode_mag_ref_pass(mq_encoder &coder, mqe_state states[],
					int p, bool causal, kdu_int32 *samples, kdu_int32 *dec_samples,
					kdu_int32 *contexts, int width, int num_stripes,
					int context_row_gap, bool lossless_pass)
{
	/* Ideally, register storage is available for 12 32-bit integers.
	Three 32-bit integers are declared inside the "_mq_check_out_" macro.
	The order of priority for these registers corresponds roughly to the
	order in which their declarations appear below.  Unfortunately, none
	of these register requests are likely to be honored by the
	register-starved X86 family of processors, but the register
	declarations may prove useful to compilers for other architectures or
	for hand optimizations of assembly code. */
	register kdu_int32 *cp = contexts;
	register int c;

	register kdu_int32 cword;
	_mq_check_out_(coder); // Declares A, C and t as registers.
	register kdu_int32 *sp = samples;
	register kdu_int32 *dec_sp = dec_samples;
	//ohhan
	register kdu_int32 curr_bit_mask = ~(1<<p);
	register mqe_state *state_ref;
	register kdu_int32 sym;
	register kdu_int32 val;
	register kdu_int32 shift = 31-p; // Shift to get new mag bit to sign position
	register kdu_int32 refined_mask = (((kdu_int32)(-1))<<(p+2)) & KDU_INT32_MAX;
	int r, width_by2=width+width, width_by3=width_by2+width;
	kdu_int32 distortion_change = 0;
	kdu_int32 *distortion_lut = refinement_distortion_lut;
	int kx, ky;
	kx = ky = 0;
	if (lossless_pass)
		distortion_lut = refinement_distortion_lut_lossless;

	states += KAPPA_MAG_BASE;
	assert((context_row_gap - width) == EXTRA_ENCODE_CWORDS);
	for (r=num_stripes; r > 0; r--, cp += EXTRA_ENCODE_CWORDS, sp += width_by3, dec_sp += width_by3, kx += 4, ky = 0)
		for (c=width; c > 0; c--, sp++, dec_sp++, cp++, ky++)
		{
			if ((*cp & ((MU_BIT<<0)|(MU_BIT<<3)|(MU_BIT<<6)|(MU_BIT<<9))) == 0)
			{ // Invoke speedup trick to skip over runs of all-0 neighbourhoods
				for (cp+=2; *cp == 0; cp+=2, c-=2, sp+=2, dec_sp += 2, ky += 2);
				cp-=2;
				continue;
			}

			//ohhan-Debug
			if (kx == 4 && ky == 4)
				kx +=0;

			if (&(dec_sp[0]) != &(dec_samples[kx*64+ky]))
				kx += 0;


			cword = *cp;
			if (cword & (MU_BIT<<0))
			{ // Process first row of stripe column
				val = sp[0];
				// Get coding context
				state_ref = states;
				if (!(val & refined_mask))
				{ // This is the first magnitude refinement step
					if (cword & (NBRHD_MASK<<0))
						state_ref++;
				}
				else
					state_ref += 2;
				val <<= shift; // Get new magnitude bit to sign position.
				sym = val & KDU_INT32_MIN;




				// Compute distortion change
				val =  (val >> (31-DISTORTION_LSBS)) & (REFINEMENT_DISTORTIONS-1);


				dec_sp[0] |= 1<<(p-1);
				if (val < (1<<5))
				{
					dec_sp[0] &= curr_bit_mask;
				}



				distortion_change += distortion_lut[val];
				// Encode magnitude bit
				_mq_enc_(coder,sym,*state_ref);
			}
			if (cword & (MU_BIT<<3))
			{ // Process second row of stripe column
				val = sp[width];
				// Get coding context
				state_ref = states;
				if (!(val & refined_mask))
				{ // This is the first magnitude refinement step
					if (cword & (NBRHD_MASK<<3))
						state_ref++;
				}
				else
					state_ref += 2;
				val <<= shift; // Get new magnitude bit to sign position.
				sym = val & KDU_INT32_MIN;
				// Compute distortion change
				val =  (val >> (31-DISTORTION_LSBS)) & (REFINEMENT_DISTORTIONS-1);

				dec_sp[width] |= 1<<(p-1);
				if (val < (1<<5))
				{
					dec_sp[width] &= curr_bit_mask;
				}

				distortion_change += distortion_lut[val];
				// Encode magnitude bit
				_mq_enc_(coder,sym,*state_ref);
			}
			if (cword & (MU_BIT<<6))
			{ // Process third row of stripe column
				val = sp[width_by2];
				// Get coding context
				state_ref = states;
				if (!(val & refined_mask))
				{ // This is the first magnitude refinement step
					if (cword & (NBRHD_MASK<<6))
						state_ref++;
				}
				else
					state_ref += 2;
				val <<= shift; // Get new magnitude bit to sign position.
				sym = val & KDU_INT32_MIN;
				// Compute distortion change
				val =  (val >> (31-DISTORTION_LSBS)) & (REFINEMENT_DISTORTIONS-1);

				dec_sp[width_by2] |= 1<<(p-1);
				if (val < (1<<5))
				{
					dec_sp[width_by2] &= curr_bit_mask;
				}

				distortion_change += distortion_lut[val];
				// Encode magnitude bit
				_mq_enc_(coder,sym,*state_ref);
			}
			if (cword & (MU_BIT<<9))
			{ // Process fourth row of stripe column
				val = sp[width_by3];
				// Get coding context
				state_ref = states;
				if (!(val & refined_mask))
				{ // This is the first magnitude refinement step
					if (cword & (NBRHD_MASK<<9))
						state_ref++;
				}
				else
					state_ref += 2;
				val <<= shift; // Get new magnitude bit to sign position.
				sym = val & KDU_INT32_MIN;
				// Compute distortion change
				val =  (val >> (31-DISTORTION_LSBS)) & (REFINEMENT_DISTORTIONS-1);

				dec_sp[width_by3] |= 1<<(p-1);
				if (val < (1<<5))
				{
					dec_sp[width_by3] &= curr_bit_mask;
				}

				distortion_change += distortion_lut[val];
				// Encode magnitude bit
				_mq_enc_(coder,sym,*state_ref);
			}
		}
		_mq_check_in_(coder);
		return distortion_change;
}

/*****************************************************************************/
/* STATIC                     encode_cleanup_pass                            */
/*****************************************************************************/

static kdu_int32
encode_cleanup_pass(mq_encoder &coder, mqe_state states[],
					int p, bool causal, int orientation,
					kdu_int32 *samples, kdu_int32 *dec_samples, kdu_int32 *contexts,
					int width, int num_stripes, int context_row_gap,
					bool lossless_pass)
{
	/* Ideally, register storage is available for 12 32-bit integers. Three
	are declared inside the "_mq_check_out_" macro.  The order of priority
	for these registers corresponds roughly to the order in which their
	declarations appear below.  Unfortunately, none of these register
	requests are likely to be honored by the register-starved X86 family
	of processors, but the register declarations may prove useful to
	compilers for other architectures or for hand optimizations of
	assembly code. */
	register kdu_int32 *cp = contexts;
	register int c;
	register kdu_int32 cword;
	_mq_check_out_(coder); // Declares A, C, and t as registers.
	register kdu_int32 sym;
	register kdu_int32 val;
	register kdu_int32 *sp = samples;
	register kdu_int32 sign_sample = 0;
	//ohhan
	register kdu_int32 *dec_sp = dec_samples;
	register kdu_int32 kx, ky;
	register kdu_int32 shift = 31-p; assert(shift > 0);
	register  kdu_byte *sig_lut = significance_luts[orientation];
	register mqe_state *state_ref;
	int r, width_by2=width+width, width_by3=width_by2+width;
	kdu_int32 distortion_change = 0;
	kdu_int32 *distortion_lut = significance_distortion_lut;
	if (lossless_pass)
		distortion_lut = significance_distortion_lut_lossless;

	kx = 0;
	ky = 0;

	assert((context_row_gap - width) == EXTRA_ENCODE_CWORDS);
	for (r=num_stripes; r > 0; r--, cp += EXTRA_ENCODE_CWORDS, sp += width_by3, dec_sp += width_by3, kx += 4, ky = 0)
		for (c=width; c > 0; c--, sp++, dec_sp++, cp++, ky++)
		{
			if (*cp == 0)
			{ // Enter the run mode
				sym = 0; val = -1;
				if ((sp[0] << shift) < 0)
				{ val = 0; sym = KDU_INT32_MIN; }
				else if ((sp[width] << shift) < 0)
				{ val = 1; sym = KDU_INT32_MIN; }
				else if ((sp[width_by2] << shift) < 0)
				{ val = 2; sym = KDU_INT32_MIN; }
				else if ((sp[width_by3] << shift) < 0)
				{ val = 3; sym = KDU_INT32_MIN; }
				state_ref = states + KAPPA_RUN_BASE;
				_mq_enc_(coder,sym,*state_ref);
				if (val < 0)
					continue;
				_mq_enc_run_(coder,val);
				cword = *cp;
				switch (val) {
			  case 0: val = sp[0]<<shift; goto row_0_significant;
			  case 1: val = sp[width]<<shift; goto row_1_significant;
			  case 2: val = sp[width_by2]<<shift; goto row_2_significant;
			  case 3: val = sp[width_by3]<<shift; goto row_3_significant;
				}
			}

			//ohhan-Debug
			if (kx == 4 && ky == 4)
				kx +=0;

			if (&(dec_sp[0]) != &(dec_samples[kx*64+ky]))
				kx += 0;

			cword = *cp;
			if (!(cword & (CLEANUP_MEMBER_MASK<<0)))
			{ // Process first row of stripe column (row 0)
				state_ref = states+KAPPA_SIG_BASE+sig_lut[cword & NBRHD_MASK];
				val = sp[0]<<shift;
				sym = val & KDU_INT32_MIN;
				_mq_enc_(coder,sym,*state_ref);
				if (val >= 0) // New magnitude bit was 0, so still insignificant
					goto row_1;
row_0_significant:
				// Compute distortion change
				val =  (val>>(31-DISTORTION_LSBS)) & (SIGNIFICANCE_DISTORTIONS-1);
				distortion_change += distortion_lut[val];
				//ohhan
				dec_sp[0] |= 1<<p;   //true coefficients
				dec_sp[0] |= 1<<(p-1); //mid-point reconstruction
				if (sign_sample = (sp[0]&(1<<31))) 
					dec_sp[0] |= sign_sample;  //sign bit 

				// Encode sign bit
				sym = cword & ((CHI_BIT>>3) | (SIGMA_CC_BIT>>3) |
					(CHI_BIT<<3) | (SIGMA_CC_BIT<<3));
				sym >>= 1; // Shift down so that top sigma bit has address 0
				sym |= (cp[-1] & ((CHI_BIT<<0) | (SIGMA_CC_BIT<<0))) >> (1+1);
				sym |= (cp[ 1] & ((CHI_BIT<<0) | (SIGMA_CC_BIT<<0))) >> (1-1);
				sym |= (sym >> (CHI_POS-1-SIGMA_CC_POS)); // Interleave chi & sigma
				val = sign_lut[sym & 0x000000FF];
				state_ref = states + KAPPA_SIGN_BASE + (val>>1);
				sym = val << 31; // Get sign flipping to `sym'
				val = sp[0] & KDU_INT32_MIN; // Get the sign bit
				sym ^= val; // Moves flipped sign bit into `sym'
				_mq_enc_(coder,sym,*state_ref);
				// Broadcast neighbourhood context changes; sign bit is in `val'
				cp[-1] |= (SIGMA_CR_BIT<<0);
				cp[1]  |= (SIGMA_CL_BIT<<0);
				if (val < 0)
				{
					cword |= (SIGMA_CC_BIT<<0) | (CHI_BIT<<0);
					if (!causal)
					{
						cp[-context_row_gap-1] |= (SIGMA_BR_BIT<<9);
						cp[-context_row_gap  ] |= (SIGMA_BC_BIT<<9) | NEXT_CHI_BIT;
						cp[-context_row_gap+1] |= (SIGMA_BL_BIT<<9);
					}
				}
				else
				{
					cword |= (SIGMA_CC_BIT<<0);
					if (!causal)
					{
						cp[-context_row_gap-1] |= (SIGMA_BR_BIT<<9);
						cp[-context_row_gap  ] |= (SIGMA_BC_BIT<<9);
						cp[-context_row_gap+1] |= (SIGMA_BL_BIT<<9);
					}
				}
			}
row_1:
			if (!(cword & (CLEANUP_MEMBER_MASK<<3)))
			{ // Process second row of stripe column (row 1)
				state_ref = states+KAPPA_SIG_BASE+sig_lut[(cword>>3) & NBRHD_MASK];
				val = sp[width]<<shift;
				sym = val & KDU_INT32_MIN;
				_mq_enc_(coder,sym,*state_ref);
				if (val >= 0) // New magnitude bit was 0, so still insignificant
					goto row_2;
row_1_significant:
				// Compute distortion change
				val =  (val>>(31-DISTORTION_LSBS)) & (SIGNIFICANCE_DISTORTIONS-1);
				distortion_change += distortion_lut[val];

				//ohhan
				dec_sp[width] |= 1<<p;   //true coefficients
				dec_sp[width] |= 1<<(p-1); //mid-point reconstruction
				if (sign_sample = (sp[width]&(1<<31))) 
					dec_sp[width] |= sign_sample;  //sign bit 


				// Encode sign bit
				sym = cword & ((CHI_BIT<<0) | (SIGMA_CC_BIT<<0) |
					(CHI_BIT<<6) | (SIGMA_CC_BIT<<6));
				sym >>= 4; // Shift down so that top sigma bit has address 0
				sym |= (cp[-1] & ((CHI_BIT<<3) | (SIGMA_CC_BIT<<3))) >> (4+1);
				sym |= (cp[ 1] & ((CHI_BIT<<3) | (SIGMA_CC_BIT<<3))) >> (4-1);
				sym |= (sym >> (CHI_POS-1-SIGMA_CC_POS)); // Interleave chi & sigma
				val = sign_lut[sym & 0x000000FF];
				state_ref = states + KAPPA_SIGN_BASE + (val>>1);
				sym = val << 31; // Get sign flipping to `sym'
				val = sp[width] & KDU_INT32_MIN; // Get the sign bit
				sym ^= val; // Moves flipped sign bit into `sym'
				_mq_enc_(coder,sym,*state_ref);
				// Broadcast neighbourhood context changes; sign bit is in `val'
				cp[-1] |= (SIGMA_CR_BIT<<3);
				cp[1]  |= (SIGMA_CL_BIT<<3);
				cword |= (SIGMA_CC_BIT<<3);
				val = (kdu_int32)(((kdu_uint32) val)>>(31-(CHI_POS+3))); // SRL
				cword |= val;
			}
row_2:
			if (!(cword & (CLEANUP_MEMBER_MASK<<6)))
			{ // Process third row of stripe column (row 2)
				state_ref = states+KAPPA_SIG_BASE+sig_lut[(cword>>6) & NBRHD_MASK];
				val = sp[width_by2]<<shift;
				sym = val & KDU_INT32_MIN;
				_mq_enc_(coder,sym,*state_ref);
				if (val >= 0) // New magnitude bit was 0, so still insignificant
					goto row_3;
row_2_significant:
				// Compute distortion change
				val =  (val>>(31-DISTORTION_LSBS)) & (SIGNIFICANCE_DISTORTIONS-1);
				distortion_change += distortion_lut[val];

				//ohhan
				dec_sp[width_by2] |= 1<<p;   //true coefficients
				dec_sp[width_by2] |= 1<<(p-1); //mid-point reconstruction
				if (sign_sample = (sp[width_by2]&(1<<31))) 
					dec_sp[width_by2] |= sign_sample;  //sign bit 


				// Encode sign bit
				sym = cword & ((CHI_BIT<<3) | (SIGMA_CC_BIT<<3) |
					(CHI_BIT<<9) | (SIGMA_CC_BIT<<9));
				sym >>= 7; // Shift down so that top sigma bit has address 0
				sym |= (cp[-1] & ((CHI_BIT<<6) | (SIGMA_CC_BIT<<6))) >> (7+1);
				sym |= (cp[ 1] & ((CHI_BIT<<6) | (SIGMA_CC_BIT<<6))) >> (7-1);
				sym |= (sym >> (CHI_POS-1-SIGMA_CC_POS)); // Interleave chi & sigma
				val = sign_lut[sym & 0x000000FF];
				state_ref = states + KAPPA_SIGN_BASE + (val>>1);
				sym = val << 31; // Get sign flipping to `sym'
				val = sp[width_by2] & KDU_INT32_MIN; // Get the sign bit
				sym ^= val; // Moves flipped sign bit into `sym'
				_mq_enc_(coder,sym,*state_ref);
				// Broadcast neighbourhood context changes; sign bit is in `val'
				cp[-1] |= (SIGMA_CR_BIT<<6);
				cp[1]  |= (SIGMA_CL_BIT<<6);
				cword |= (SIGMA_CC_BIT<<6);
				val = (kdu_int32)(((kdu_uint32) val)>>(31-(CHI_POS+6))); // SRL
				cword |= val;
			}
row_3:
			if (!(cword & (CLEANUP_MEMBER_MASK<<9)))
			{ // Process fourth row of stripe column (row 3)
				state_ref = states+KAPPA_SIG_BASE+sig_lut[(cword>>9) & NBRHD_MASK];
				val = sp[width_by3]<<shift;
				sym = val & KDU_INT32_MIN;
				_mq_enc_(coder,sym,*state_ref);
				if (val >= 0) // New magnitude bit was 0, so still insignificant
					goto done;
row_3_significant:
				// Compute distortion change
				val =  (val>>(31-DISTORTION_LSBS)) & (SIGNIFICANCE_DISTORTIONS-1);
				distortion_change += distortion_lut[val];

				//ohhan
				dec_sp[width_by3] |= 1<<p;   //true coefficients
				dec_sp[width_by3] |= 1<<(p-1); //mid-point reconstruction
				if (sign_sample = (sp[width_by3]&(1<<31))) 
					dec_sp[width_by3] |= sign_sample;  //sign bit 


				// Decode sign bit
				sym = cword & ((CHI_BIT<<6) | (SIGMA_CC_BIT<<6) |
					0       | (SIGMA_CC_BIT<<12));
				sym >>= 10; // Shift down so that top sigma bit has address 0
				if (cword < 0) // Use the fact that NEXT_CHI_BIT = 31
					sym |= CHI_BIT<<(12-10);
				sym |= (cp[-1] & ((CHI_BIT<<9) | (SIGMA_CC_BIT<<9))) >> (10+1);
				sym |= (cp[ 1] & ((CHI_BIT<<9) | (SIGMA_CC_BIT<<9))) >> (10-1);
				sym |= (sym >> (CHI_POS-1-SIGMA_CC_POS)); // Interleave chi & sigma
				val = sign_lut[sym & 0x000000FF];
				state_ref = states + KAPPA_SIGN_BASE + (val>>1);
				sym = val << 31; // Get sign flipping to `sym'
				val = sp[width_by3] & KDU_INT32_MIN; // Get the sign bit
				sym ^= val; // Moves flipped sign bit into `sym'
				_mq_enc_(coder,sym,*state_ref);
				// Broadcast neighbourhood context changes; sign bit is in `val'
				cp[context_row_gap-1] |= SIGMA_TR_BIT;
				cp[context_row_gap+1] |= SIGMA_TL_BIT;
				cp[-1] |= (SIGMA_CR_BIT<<9);
				cp[1]  |= (SIGMA_CL_BIT<<9);
				if (val < 0)
				{
					cp[context_row_gap  ] |= SIGMA_TC_BIT | PREV_CHI_BIT;
					cword |= (SIGMA_CC_BIT<<9) | (CHI_BIT<<9);
				}
				else
				{
					cp[context_row_gap  ] |= SIGMA_TC_BIT;
					cword |= (SIGMA_CC_BIT<<9);
				}
			}
done:
			cword |= (cword << (MU_POS - SIGMA_CC_POS)) &
				((MU_BIT<<0)|(MU_BIT<<3)|(MU_BIT<<6)|(MU_BIT<<9));
			cword &= ~((PI_BIT<<0)|(PI_BIT<<3)|(PI_BIT<<6)|(PI_BIT<<9));
			*cp = cword;
		}
		_mq_check_in_(coder);
		return distortion_change;
}

/* ========================================================================= */
/*               Distortion-Length Slope Calculating Functions               */
/* ========================================================================= */

/*****************************************************************************/
/* INLINE                         slope_to_log                               */
/*****************************************************************************/

static inline kdu_uint16
  slope_to_log(double lambda)
{
  double max_slope = pow(2.0,32.0); // Maximum slope we can reprsent
  double log_scale = 256.0 / log(2.0); // Convert log values to 8.8 fixed point
  double log_slope = lambda / max_slope;
  log_slope = (log_slope > 1.0)?1.0:log_slope;
  log_slope = (log(log_slope) * log_scale) + (double)(1<<16);
  if (log_slope > (double) 0xFFFF)
    return 0xFFFF;
  else if (log_slope < 2.0)
    return 2;
  else
    return (kdu_uint16) log_slope;
}

/*****************************************************************************/
/* INLINE                       slope_from_log                               */
/*****************************************************************************/

static inline double
  slope_from_log(kdu_uint16 slope)
{
  double max_slope = pow(2.0,32.0);
  double log_scale = log(2.0) / 256.0;
  double log_slope = ((double) slope) - ((double)(1<<16));
  log_slope *= log_scale;
  return exp(log_slope) * max_slope;
}

/*****************************************************************************/
/* STATIC                      find_convex_hull                              */
/*****************************************************************************/

static void
  find_convex_hull(int pass_lengths[], double pass_wmse_changes[],
                   kdu_uint16 pass_slopes[], int num_passes)
{
  int z;

  for (z=0; z < num_passes; z++)
    { /* Find the value, lambda(z), defined in equation (8.4) of the book by
         Taubman and Marcellin.  Every coding pass which contributes to the
         convex hull set has this value for its distortion-length slope;
         however, not all coding passes which have a positive lambda(z) value
         actually contribute to the convex hull.  The points which do not
         contribute are weeded out next. */
      double lambda_z = -1.0; // Marks initial condition.
      double delta_L = 0.0, delta_D = 0.0;
      int t, min_t=(z>9)?(z-9):0;
      for (t=z; t >= min_t; t--)
        {
          delta_L += (double) pass_lengths[t];
          delta_D += pass_wmse_changes[t];
          if (delta_D <= 0.0)
            { // This pass cannot contribute to convex hull
              lambda_z = -1.0; break;
            }
          else if ((delta_L > 0.0) &&
                   ((lambda_z < 0.0) || ((delta_L*lambda_z) > delta_D)))
            lambda_z = delta_D / delta_L;
        }
      if (lambda_z <= 0.0)
        pass_slopes[z] = 0;
      else
        pass_slopes[z] = slope_to_log(lambda_z);
    }

  /* Now weed out the non-contributing passes. */
  for (z=0; z < num_passes; z++)
    {
      for (int t=z+1; t < num_passes; t++)
        if (pass_slopes[t] >= pass_slopes[z])
          { pass_slopes[z] = 0; break; }
    }
}


/* ========================================================================= */
/*                           kdu_block_encoder                               */
/* ========================================================================= */

/*****************************************************************************/
/*                  kdu_block_encoder::kdu_block_encoder                     */
/*****************************************************************************/

kdu_block_encoder::kdu_block_encoder()
{
  state = new kd_block_encoder;
}


/* ========================================================================= */
/*                            kd_block_encoder                               */
/* ========================================================================= */

/*****************************************************************************/
/*                         kd_block_encoder::encode                          */
/*****************************************************************************/

void
  kd_block_encoder::encode(kdu_block *block, bool reversible, double msb_wmse,
                           kdu_uint16 estimated_threshold)
{
  double estimated_slope_threshold = -1.0;
  if ((estimated_threshold > 1) && (msb_wmse > 0.0) &&
      (block->orientation != LL_BAND))
    estimated_slope_threshold = slope_from_log(estimated_threshold);
     /* Note: the exclusion of the LL band above is important, since LL
        subband blocks do not generally have zero mean samples (far from
        it) so that an approximately uniform block can yield distortion
        which varies in an unpredictable manner with the number of coded
        bit-planes.  These unpredictable variations can lead to a large
        number of consecutive coding passes not lying on the convex hull
        of the R-D curve -- a condition which would cause the coder
        to quit prematurely if an estimated slope threshold were given. */

  /* Allocate space on the stack for a number of largish quantities.  These
     could be placed elsewhere. */
  double pass_wmse_changes[MAX_POSSIBLE_PASSES];
  mqe_state states[18];
  mq_encoder pass_encoders[MAX_POSSIBLE_PASSES];
  // Get dimensions.
  int num_cols = block->size.x;
  int num_rows = block->size.y;
  int num_stripes = (num_rows+3)>>2;
  int context_row_gap = num_cols + EXTRA_ENCODE_CWORDS;
  int num_context_words = (num_stripes+2)*context_row_gap+1;
  
  // Prepare enough storage.
  assert(block->max_samples >= ((num_stripes<<2)*num_cols));
  if (block->max_contexts < num_context_words)
    block->set_max_contexts((num_context_words > 1600)?num_context_words:1600);
  kdu_int32 *samples = block->sample_buffer;
  kdu_int32 *context_words = block->context_buffer + context_row_gap + 1;

	//ohhan
  kdu_int32 dec_samples[4096];
  memset((kdu_int32*)dec_samples,0,sizeof(kdu_int32)*4096);
  bool percep_coding = true;
  double embbed_delta;
  bool below_threshold[100]; 

  kdu_uint16* pass_dist = new kdu_uint16[block->num_passes];
  memset((kdu_uint16*)pass_dist,0,sizeof(kdu_uint16)*block->num_passes);
  double dist_sum = 0;
  double minko_dist = 0;
  double max_dist = -1000;
  double max_coeff=-1000;
  double minmaxdist=0.0;
  int minmaxdistidx[100];
  double firstlow=0.0;
  int firstlowidx[100];
  double dist = 0;
  double coeffsingle=0;
  double beta = 10;
  
  //Calculate variance
  double sample_val = 0;
  double sample_var = 0;
  double sample_mean = 0;
  double sample_mean2 = 0;
  double jnd_threshold[100];
  float normalization[6][4] ={{1.0, 1.0, 1.0, 1.0}, {0.756664, 1.0, 1.0, 1.321590}, {1.145082, 1.513328, 1.513328, 2.0}, {0.866442, 1.145082, 1.145082, 1.513329}, {0.655606, 0.866442, 0.866442, 1.145082}, {0.992147, 1.311212, 1.311212, 1.732885}};
  int num_coeffs = block->size.get_y()*block->size.get_x();

  for (int i = 0; i < num_coeffs; i++)
  {
	  sample_val = (double)(block->sample_buffer[i]&0x7fffffff)/(double)(1<<(31-block->k_max));
	  sample_mean2 += sample_val;
	  if (block->sample_buffer[i] < 0)
	  sample_val = -sample_val;
	  sample_var += pow(sample_val,2.0);
	  sample_mean += sample_val;
  }
  sample_mean = 1.0/(double)num_coeffs*sample_mean;
  sample_mean2 = 1.0/(double)num_coeffs*sample_mean2*256.0*(double)block->delta; 
  sample_var = 1.0/(double)num_coeffs*sample_var - pow(sample_mean,2.0);
  sample_var *= pow((double)256*block->delta,2.0); 
  //jnd_threshold = block->a_b * sample_var + block->b_b;


  // az_liuf 

  //if (block->orient == 0)
  //{
	 // if(block->comp_idx==0)
	 // {
		//  jnd_threshold = 0.314442;
	 // }
	 // else if(block->comp_idx==1)
	 // {
		//  jnd_threshold = 0.526328;
	 // }
	 // else if(block->comp_idx==2)
	 // {
		//  jnd_threshold = 0.396245;
	 // }
  //}

  

  //modify jyh

  int num_vts = block->vtnums;
  int Q_mode = block->Qmode;
  double *Q = block->Qquality;
  double *p_weight = block->p_weight;
  //if (block->mapon == true) {
	 // printf("num_vts=%d\n",num_vts);
  //}
  //if (block->extralayer = true)printf("using extralayer\n");
  double vt[100];
  int i_vt = 0;



  if (block->mapon == false) {

	  if (Q_mode == 4) {
		  for (; i_vt < num_vts; i_vt++) {
			  int vt_band, vtidx;
			  if (block->orient != 0) {
				  if (block->orient == 3) {
					  vt_band = 2 * block->dwt_level - 2;
				  }
				  else {
					  vt_band = 2 * block->dwt_level - 1;
				  }
			  }
			  else { vt_band = 10; }
			  vtidx = 33 * i_vt + 3 * vt_band + block->comp_idx;
			  vt[i_vt] = Q[vtidx];
		  }
	  }
	  else if (Q_mode == 1) {
		  for (; i_vt < num_vts; i_vt++) {
			  vt[i_vt] = VTcal(block, sample_var, block->orient, block->comp_idx, block->dwt_level, Q[i_vt], Q[num_vts]);
		  }
	  }
	  else {
		  for (; i_vt < num_vts; i_vt++) {
			  vt[i_vt] = VTcal(block, sample_var, block->orient, block->comp_idx, block->dwt_level, Q[num_vts], Q[i_vt]);
		  }
	  }
  }
  else {
	  if (block->extralayer == true) {
		  if (Q_mode == 4) {
			  int vt_band, vtidx;
			  if (block->orient != 0) {
				  if (block->orient == 3) {
					  vt_band = 2 * block->dwt_level - 2;
				  }
				  else {
					  vt_band = 2 * block->dwt_level - 1;
				  }
			  }
			  else { vt_band = 10; }

			  for (; i_vt < num_vts; i_vt++) {
				  if (i_vt != num_vts - 1) {
					  vtidx = 33 * i_vt + 3 * vt_band + block->comp_idx;
					  vt[i_vt] = p_weight[i_vt]*Q[vtidx];
				  }
				  else {
					  vtidx = 33 * (i_vt - 1) + 3 * vt_band + block->comp_idx;
					  vt[i_vt] = Q[vtidx];
				  }
			  }
		  }
		  else if (Q_mode == 1) {
			  for (; i_vt < num_vts; i_vt++) {
				  if (i_vt != num_vts - 1) {
					  vt[i_vt] = p_weight[i_vt] * VTcal(block, sample_var, block->orient, block->comp_idx, block->dwt_level, Q[i_vt], Q[num_vts - 1]);
				  }
				  else {
					  vt[i_vt] = VTcal(block, sample_var, block->orient, block->comp_idx, block->dwt_level, Q[i_vt - 1], Q[num_vts - 1]);
				  }
			  }
		  }
		  else {
			  for (; i_vt < num_vts; i_vt++) {
				  if (i_vt != num_vts - 1) {
					  vt[i_vt] = p_weight[i_vt] * VTcal(block, sample_var, block->orient, block->comp_idx, block->dwt_level, Q[num_vts - 1], Q[i_vt]);
				  }
				  else {
					  vt[i_vt] = VTcal(block, sample_var, block->orient, block->comp_idx, block->dwt_level, Q[num_vts - 1], Q[i_vt - 1]);
				  }
			  }
		  }
	  }
	  else {
		  if (Q_mode == 4) {
			  for (; i_vt < num_vts; i_vt++) {
				  int vt_band, vtidx;
				  if (block->orient != 0) {
					  if (block->orient == 3) {
						  vt_band = 2 * block->dwt_level - 2;
					  }
					  else {
						  vt_band = 2 * block->dwt_level - 1;
					  }
				  }
				  else { vt_band = 10; }
				  vtidx = 33 * i_vt + 3 * vt_band + block->comp_idx;
				  vt[i_vt] = p_weight[i_vt]*Q[vtidx];
			  }
		  }
		  else if (Q_mode == 1) {
			  for (; i_vt < num_vts; i_vt++) {
				 vt[i_vt] = p_weight[i_vt]*VTcal(block, sample_var, block->orient, block->comp_idx, block->dwt_level, Q[i_vt], Q[num_vts]);
				 //printf("Q[%d]=%lf,Q[%d]=%lf\n", i_vt, Q[i_vt], num_vts, Q[num_vts]);
				 //printf("vt[%d]=%lf numvts=%d\n", i_vt, vt[i_vt], num_vts);
			  }
		  }
		  else {
			  for (; i_vt < num_vts; i_vt++) {
				  vt[i_vt] = p_weight[i_vt]*VTcal(block, sample_var, block->orient, block->comp_idx, block->dwt_level, Q[num_vts], Q[i_vt]);
			  }
		  }
	  }
	  
  }


  if (num_vts != 1) {
	  double temp_vt;
	  int pos_vt;
	  for (int ord = 1; ord < num_vts; ord++) {
		  temp_vt = vt[ord];
		  pos_vt = ord - 1;
		  while ((pos_vt >= 0) && (temp_vt > vt[pos_vt])) {
			  vt[pos_vt + 1] = vt[pos_vt];
			  pos_vt--;
		  }
		  vt[pos_vt + 1] = temp_vt;
	  }
  }


  //for (int i = 0; i < num_vts; i++) {
	 // printf("vt[%d]=%lf  ", i, vt[i]);
	 // if (i == num_vts - 1) {
		//  printf("p_w=%lf\n", p_weight[0]);
	 // }
  //}


  if (block->orient == 0) {
	  block->a_b = 1.0;
	  block->b_b = 1.0;
  }
  else {
	  block->a_b = 0;
	  block->b_b = vt[0];
  }

  for (i_vt = 0; i_vt < num_vts; i_vt++) {
	  jnd_threshold[i_vt] = vt[i_vt];
	  below_threshold[i_vt] = 0;
	  firstlowidx[i_vt] = 0;
	  minmaxdistidx[i_vt] = 0;
	  //printf("jnd_threshold[%i]=%f\n",i_vt, jnd_threshold[i_vt]);
  }

  
  //masking 
  bool masking_mode;
  double nm[4096];  
  double sm[4096];
  memset((double*)nm,1.0,sizeof(double)*4096);
  memset((double*)sm,1.0,sizeof(double)*4096);
  double masking[4096];
  memset((double*)masking,1.0,sizeof(double)*4096);
  double masking_factor = 1.0f;
  double sm_exp, nm_exp, sm_weight, nm_weight, masking_beta;
  int winsize = 32;
  int winsizeb = (int)((double)winsize/pow(2.0,(double)block->dwt_level));
  double nm_val;
  double adjusted_threshold[100];
  

  // Determine the actual number of passes which we can accomodate.

  int full_reversible_passes = block->num_passes;
        // For reversibility, need all these passes recorded in code-stream.
  if (block->num_passes > ((31-block->missing_msbs)*3-2))
    block->num_passes = (31-block->missing_msbs)*3-2;
  if (block->num_passes < 0)
    block->num_passes = 0;

  // Make sure we have sufficient resources to process this number of passes

  if (block->max_passes < block->num_passes)
    block->set_max_passes(block->num_passes+10,false); // Allocate a few extra

  // Timing loop starts here.
  int cpu_counter = block->start_timing();
  do {
      // Initialize contexts, marking OOB (Out Of Bounds) locations
      memset(context_words-1,0,(size_t)((num_stripes*context_row_gap+1)<<2));
      if (num_rows & 3)
        {
          kdu_int32 oob_marker;
          if ((num_rows & 3) == 1) // Last 3 rows of last stripe unoccupied
            oob_marker = (OOB_MARKER<<3) | (OOB_MARKER<<6) | (OOB_MARKER<<9);
          else if ((num_rows & 3) == 2) // Last 2 rows of last stripe unused
            oob_marker = (OOB_MARKER << 6) | (OOB_MARKER << 9);
          else
            oob_marker = (OOB_MARKER << 9);
          kdu_int32 *cp = context_words + (num_stripes-1)*context_row_gap;
          for (int k=num_cols; k > 0; k--)
            *(cp++) = oob_marker;
        }
      if (context_row_gap > num_cols)
        { // Initialize the extra context words between lines to OOB
          kdu_int32 oob_marker =
            OOB_MARKER | (OOB_MARKER<<3) | (OOB_MARKER<<6) | (OOB_MARKER<<9);
          assert(context_row_gap >= (num_cols+3));
          kdu_int32 *cp = context_words + num_cols;
          for (int k=num_stripes; k > 0; k--, cp+=context_row_gap)
            cp[0] = cp[1] = cp[2] = oob_marker; // Need 3 OOB words after line
        }

      double pass_wmse_scale = msb_wmse / KD_DISTORTION_LUT_SCALE;
      pass_wmse_scale = pass_wmse_scale / ((double)(1<<16));
      for (int i=block->missing_msbs; i > 0; i--)
        pass_wmse_scale *= 0.25;
      int p_max = 30 - block->missing_msbs; // Index of most significant plane
      int p = p_max; // Bit-plane counter
      int z = 0; // Coding pass index
      int k=2; // Coding pass category; start with cleanup pass
      int segment_passes = 0; // Num coding passes in current codeword segment.
      kdu_byte *seg_buf = block->byte_buffer; // Start of current segment
      int segment_bytes = 0; // Bytes consumed so far by this codeword segment
      int first_unsized_z = 0; // First pass whose length is not yet known
      int available_bytes = block->max_bytes; // For current & future segments
      bool bypass = false;
      bool causal = (block->modes & Cmodes_CAUSAL) != 0;
      bool optimize = !(block->modes & Cmodes_ERTERM);


	  //masking
	  masking_mode = false;
	  if (block->comp_idx != 0)
		  masking_mode = false;
	  sm_weight = 0.8;
	  sm_exp = 0.35;
	  nm_weight = 0.35;
	  nm_exp = 0.4;
	  masking_beta = 0.8;
	  

	  for (int i=0; i < num_coeffs; i++)
	  {
		  if (masking_mode == true)
			  sm[i] = max(1.0,sm_weight*pow((double)(samples[i]&0x7fffffff)/(double)(1<<(31-block->k_max))*block->delta*256.0/(sample_mean2+0.00001),sm_exp));
	  }

	  // liuf, find the maximum abs of the coeffcients in the codeblock

	  max_coeff=-1000;

	  for (int mj=0; mj<num_coeffs;mj++)
	  {
		  coeffsingle=fabs((double)(samples[mj]&0x7fffffff)/(double)(1<<(31-block->k_max)));
//		  if((block->orient==0)&&(block->dwt_level==5))
//			fprintf(pThresh,"\n%f",coeffsingle*block->delta*256.0);
		  if(max_coeff<coeffsingle)
			  max_coeff=coeffsingle;
	  }
	  max_coeff = max_coeff*block->delta*256.0;
	  

	  //ohhan
	  //fprintf(pThresh,"\n%d %d %d ",block->comp_idx,block->dwt_level,block->orient);

      for (; z < block->num_passes; z++, k++)
        {

          if (k == 3)
            { // Move on to next bit-plane.
              k=0; p--;
              pass_wmse_scale *= 0.25;
            }

          // See if we need to augment byte buffer resources to safely continue
          if ((available_bytes-segment_bytes) < 4096)
            { // We could build a much more thorough test for sufficiency.
              assert(available_bytes >= segment_bytes); // else already overrun
              kdu_byte *old_handle = block->byte_buffer;
              block->set_max_bytes(block->max_bytes+8192,true);
              available_bytes += 8192;
              kdu_byte *new_handle = block->byte_buffer;
              for (int i=0; i < z; i++)
                pass_encoders[i].augment_buffer(old_handle,new_handle);
              seg_buf = new_handle + (seg_buf-old_handle);
            }

          // Either start a new codeword segment, or continue an earlier one.
          if (segment_passes == 0)
            { // Need to start a new codeword segment.
              segment_passes = block->num_passes;
              if (block->modes & Cmodes_BYPASS)
                {
                  if (z < 10)
                    segment_passes = 10-z;
                  else if (k == 2) // Cleanup pass.
                    { segment_passes = 1; bypass = false; }
                  else
                    {
                      segment_passes = 2;
                      bypass = true;
                    }
                }
              if (block->modes & Cmodes_RESTART)
                segment_passes = 1;
              if ((z+segment_passes) > block->num_passes)
                segment_passes = block->num_passes - z;
              pass_encoders[z].start(seg_buf,!bypass);
            }
          else
            pass_encoders[z].continues(pass_encoders+z-1);

		  // Encoding steps for the pass
		  kdu_int32 distortion_change;
		  bool lossless_pass = reversible && ((31-p) == block->K_max_prime);

		  if ((z == 0) || (block->modes & Cmodes_RESET))
			  reset_states(states);
		  if ((k == 0) && !bypass)
			  distortion_change =
			  encode_sig_prop_pass(pass_encoders[z],
			  states,p,causal,block->orientation,
			  samples, dec_samples, context_words,
			  num_cols,num_stripes,context_row_gap,
			  lossless_pass);
		  else if (k == 0)
			  distortion_change =
			  encode_sig_prop_pass_raw(pass_encoders[z],
			  p,causal,samples,context_words,
			  num_cols,num_stripes,context_row_gap,
			  lossless_pass);
		  else if ((k == 1) && !bypass)
			  distortion_change =
			  encode_mag_ref_pass(pass_encoders[z],
			  states,p,causal,samples,dec_samples,context_words,
			  num_cols,num_stripes,context_row_gap,
			  lossless_pass);
		  else if (k == 1)
			  distortion_change =
			  encode_mag_ref_pass_raw(pass_encoders[z],
			  p,causal,samples,context_words,
			  num_cols,num_stripes,context_row_gap,
			  lossless_pass);
		  else
			  distortion_change =
			  encode_cleanup_pass(pass_encoders[z],
			  states,p,causal,block->orientation,
			  samples, dec_samples, context_words,
			  num_cols,num_stripes,context_row_gap,
			  lossless_pass);
		  pass_wmse_changes[z] = pass_wmse_scale * distortion_change;

		  
		  if (percep_coding == true)
		  {
			  

			  //texture masking
			  if (masking_mode == true && block->dwt_level <= 3)
			  {
				  for (int i = 0; i < block->size.get_y(); i += winsizeb)
				  {
					  for (int j = 0; j < block->size.get_x(); j += winsizeb)
					  {
						  sample_var = 0;
						  sample_mean = 0;
						  //calculate local variance
						  for (int m = i; m < i+winsizeb; m++)  //m,y,i - (row)
						  {
							  for (int n = j; n < j+winsizeb; n++) //n,x,j - (col)
							  {
								  sample_val = (double)(dec_samples[m*block->size.get_x()+n]&0x7fffffff)/(double)(1<<(31-block->k_max));
								  if (dec_samples[m*block->size.get_x()+n] < 0)
									  sample_val = -sample_val;
								  sample_var += pow(sample_val,2.0);
								  sample_mean += sample_val;
							  }
						  }
						  sample_mean = 1.0/(double)(winsizeb*winsizeb)*sample_mean;
						  sample_var = 1.0/(double)(winsizeb*winsizeb)*sample_var - pow(sample_mean,2.0);
						  sample_var *= pow((double)256.0*block->delta,2.0);
						  nm_val = max(1.0,nm_weight*pow(sample_var,nm_exp));

						  //allocate texture masking
						  for (int m = i; m < i+winsizeb; m++)
						  {
							  for (int n = j; n < j+winsizeb; n++)
							  {
								  nm[m*block->size.get_x()+n] = nm_val;
							  }
						  }

					  }
				  }

				  sample_mean = 0;
				  //minkowski distance
				  for (int i = 0; i < num_coeffs; i++)
				  {
					  masking[i] = sm[i]*nm[i];
					  sample_mean += pow(masking[i],masking_beta); 
				  }
				  masking_factor = pow(1.0/(double)num_coeffs*sample_mean,1.0/masking_beta);

			  }
			  double qthresh[100];


			  for (i_vt = 0; i_vt < num_vts; i_vt++) {
				  if (masking_mode == true)
				  {
					  if (block->orient != 0)
						  adjusted_threshold[i_vt] = min(masking_factor*jnd_threshold[i_vt], 6.0*block->b_b);
					  else
						  adjusted_threshold[i_vt] = masking_factor*jnd_threshold[i_vt];
				  }
				  else
					  adjusted_threshold[i_vt] = jnd_threshold[i_vt];

				  qthresh[i_vt] = adjusted_threshold[i_vt] / (block->delta*256.0);
				  //printf("adjusted_threshold[%i]=%f, qthresh[%i]=%f\n",i_vt, adjusted_threshold[i_vt],i_vt, qthresh[i_vt]);
			  }


			  max_dist = -1000;
			  dist_sum = 0;


			  // liuf, find distortion
			  for (i_vt = 0; i_vt < num_vts; i_vt++) {
				  for (int i = 0; i < num_coeffs; i++)
				  {
					  //Calculate error
					  /*if((double)(samples[i]&0x7fffffff)/(double)(1<<(31-block->k_max))!=0.0)
					  {
					  dist1 = abs((double)(samples[i]&0x7fffffff-dec_samples[i]&0x7fffffff)/(double)(1<<(31-block->k_max)));
					  dist2 = abs((double)(samples[i]&0x7fffffff-dec_samples[i]&0x7fffffff)/(double)(1<<(31-block->k_max))+1);
					  dist=max(dist1,dist2);
					  }
					  else
					  {
					  dist1 = abs((double)(samples[i]&0x7fffffff-dec_samples[i]&0x7fffffff)/(double)(1<<(31-block->k_max))-1);
					  dist2 = abs((double)(samples[i]&0x7fffffff-dec_samples[i]&0x7fffffff)/(double)(1<<(31-block->k_max))+1);
					  dist=max(dist1,dist2);
					  }*/
					  dist = fabs((double)(samples[i] & 0x7fffffff - dec_samples[i] & 0x7fffffff) / (double)(1 << (31 - block->k_max)));
					  /*if((double)(dec_samples[i]&0x7fffffff)/(double)(1<<(31-block->k_max))==0.0)
					  {
					  if((double)(samples[i]&0x7fffffff)/(double)(1<<(31-block->k_max))>=1.0)
					  {
					  dist=dist+0.5;
					  }
					  }
					  else*/
					  if (((double)(dec_samples[i] & 0x7fffffff) / (double)(1 << (31 - block->k_max)) >= qthresh[i_vt]) && ((double)(samples[i] & 0x7fffffff) / (double)(1 << (31 - block->k_max)) >= qthresh[i_vt]))
					  {
						  dist = dist*2.0;
					  }

					  //dist_sum += pow(dist*block->delta*256.0/jnd_threshold,beta);
					  //pass_dist[z] += dist;

					  //dist = dist / p_weight[i];
					  if (max_dist < dist)
						  max_dist = dist;
					  //if(block->orient == 0)
					  //fprintf(pThresh,"\n%f %f",dist,max_dist);


				  }

				  // liuf, find whether to include
				  //minko_dist = pow(dist_sum,1.0/beta);
				  max_dist = max_dist*block->delta*256.0;

				  if ((max_dist<minmaxdist) || (z == 0))
				  {
					  // minmaxdist=max_dist;
					  minmaxdistidx[i_vt] = z;
				  }
				  if ((below_threshold[i_vt] == false) && (max_dist<adjusted_threshold[i_vt]))
				  {
					  // firstlow=max_dist;
					  firstlowidx[i_vt] = z;
					  below_threshold[i_vt] = true;
				  }
			  }


			  /*//embbed_delta = block->delta*256.0*(1<<(int)((block->num_passes-z+1)/3.0));
			  pass_dist[z] = (kdu_uint16)(max_dist+1000.0);
			  if (below_threshold) 
				  pass_dist[z] = 1;
			  //if (below_threshold == false && embbed_delta <= jnd_threshold)
			  if (below_threshold == false && max_dist <= adjusted_threshold)
			  {
				  //printf("\njnd threshold=%.4f, max distortion=%.4f, \nmasking_factor=%.4f adjusted threshold=%.4f(difference=%.4f)\n",jnd_threshold,max_dist,masking_factor,adjusted_threshold,abs(adjusted_threshold-max_dist));
				  //fprintf(pThresh,"%.4f %.4f %.4f",adjusted_threshold,max_dist,adjusted_threshold-max_dist);
				  pass_dist[z] = 2;
				  below_threshold = true;
			  }*/

		  }

		  if (block->orient == 1 && block->dwt_level == 4)
			  block->orient += 0;


          if ((block->modes & Cmodes_SEGMARK) && (k==2))
            {
              kdu_int32 segmark = 0x0A;
              pass_encoders[z].mq_encode_run(segmark>>2);
              pass_encoders[z].mq_encode_run(segmark & 3);
            }

          // Update codeword segment status.
          segment_passes--;
          segment_bytes = pass_encoders[z].get_bytes_used();
          if (segment_passes == 0)
            {
              kdu_byte *new_buf = pass_encoders[z].terminate(optimize);
              available_bytes -= (int)(new_buf - seg_buf);
              seg_buf = new_buf;
              segment_bytes = 0;
            }

          { // Incrementally determine and process truncation lengths
            int t;
            bool final;
            for  (t=first_unsized_z; t <= z; t++)
              {
                block->pass_lengths[t] =
                  pass_encoders[t].get_incremental_length(final);
                if (final)
                  { assert(first_unsized_z == t); first_unsized_z++; }
              }
          }

          if (percep_coding == false && estimated_slope_threshold > 0.0)
            { // See if we can finish up early to save time and memory.
              /* The heuristic is as follows:
                   1) We examine potential truncation points (coding passes),
                      t <= z, finding the distortion length slopes, S_t,
                      which would arise at those points if they were to lie
                      on the convex hull.  This depends only upon distortion
                      and length information for coding passes <= t.
                   2) Let I=[z0,z] be the smallest interval which contains
                      three potential convex hull points, t1 < t2 < t3.
                      By "potential convex hull points", we simply mean that
                      S_t1 >= S_t2 >= S_t3 > 0.  If no such interval exists,
                      set z0=0 and I=[0,z].
                   3) We terminate coding here if all the following conditions
                      hold:
                      a) Interval I contains at least 3 points.
                      b) Interval I contains at least 2 potential convex hull
                         points.
                      c) For each t in I=[z0,z], S_t < alpha(z-t) * S_min,
                         where S_min is the value of `expected_slope_threshold'
                         and alpha(q) = 3^floor(q/3).  This means that
                         alpha(z-t) = 1 for t in {z,z-1,z-2};
                         alpha(z-t) = 3 for t in {z-3,z-4,z-5}; etc.
                         The basic idea is to raise the slope threshold by
                         3 each time we move to another bit-plane.  The base
                         of 3 represents a degree of conservatism, since at
                         high bit-rates we expect the slope to change by a
                         factor of 4 for each extra bit-plane.
              */
              int num_hull_points=0;
              int u, z0;
              double last_deltaD, last_deltaL;
              double deltaD, deltaL, best_deltaD, best_deltaL;
              for (z0=z; (z0 >= 0) && (num_hull_points < 3); z0--)
                {
                  for (deltaL=0.0, deltaD=0.0, u=z0;
                       (u >= 0) && (u > (z0-7)); u--)
                    {
                      deltaL += block->pass_lengths[u];
                      deltaD += pass_wmse_changes[u];
                      if ((u==z0) ||
                          ((best_deltaD*deltaL) > (deltaD*best_deltaL)))
                        { best_deltaD=deltaD;  best_deltaL=deltaL; }
                    }
                  double ref = estimated_slope_threshold*best_deltaL;
                  for (u = z-z0; u > 2; u-=3)
                    ref *= 3.0;
                  if (best_deltaD > ref)
                    { // Failed condition 3.c
                      num_hull_points=0; // Prevent truncation
                      break;
                    }
                  if ((best_deltaD > 0.0) && (best_deltaL > 0.0))
                    { // Might be a potential convex hull point
                      if ((num_hull_points > 0) &&
                          ((best_deltaD*last_deltaL) <=
                           (last_deltaD*best_deltaL)))
                        continue; // Cannot be a convex hull point
                      last_deltaD = best_deltaD;
                      last_deltaL = best_deltaL;
                      num_hull_points++;
                    }
                }
              if ((num_hull_points >= 2) && ((z-z0) >= 3))
                {
                  block->num_passes = z+1; // No point in coding more passes.
                  if (segment_passes > 0)
                    { // Need to terminate the segment
                      pass_encoders[z].terminate(optimize);
                      for (u=first_unsized_z; u <= z; u++)
                        {
                          bool final;
                          block->pass_lengths[u] =
                            pass_encoders[u].get_incremental_length(final);
                          assert(final);
                        }
                      first_unsized_z = u;
                      segment_passes = 0;
                    }
                }
            }

          if (segment_passes == 0)
            { // Finish cleaning up the completed codeword segment.
              assert(first_unsized_z == (z+1));
              pass_encoders[z].finish();
            }
        }

      assert(segment_passes == 0);
      assert(first_unsized_z == block->num_passes);

      if (msb_wmse > 0.0)
        { /* Form the convex hull set.  We could have done most of the work
             incrementally as the lengths became available -- see above.
             However, there is little value to this in the present
             implementation.  A hardware implementation may prefer the
             incremental calculation approach for all quantities, so that
             slopes and lengths can be dispatched in a covnenient interleaved
             fashion to memory as they become available. */
          find_convex_hull(block->pass_lengths,pass_wmse_changes,
                           block->pass_slopes,block->num_passes);


		  //liuf - pass_slopes[i]
          /*
		  for (i_vt = 0; i_vt < num_vts; i_vt++) {
			  printf("minmaxdistidx[%i]=%i,   ", i_vt, minmaxdistidx[i_vt]);
			  printf("firstlowidx[%i]=%i\n", i_vt, firstlowidx[i_vt]);
		  }
		  */

		  if (percep_coding == true) {
			  kdu_uint16 min_value = 65535;
			  if ((max_coeff < jnd_threshold[num_vts-1]) || (jnd_threshold[num_vts-1]>100)) {
				  for (int i = 0; i < block->num_passes; i++)
				  {
					  block->pass_slopes[i] = 0;
					 // printf("block->pass_slope[%i]=%i\n", i, block->pass_slopes[i]);
				  }
			  }
			  else
			  {
				  for (int i = 0; i < block->num_passes; i++) {
					  for (i_vt = 0; i_vt < num_vts; i_vt++) {
						  if (below_threshold[i_vt] == true)
						  {
							  if (i <= firstlowidx[i_vt]) {
								  block->pass_slopes[i] = num_vts-i_vt+1;
								  break;
							  }
							  else
								  block->pass_slopes[i] = 0;
						  }
						  else
						  {
							  if (i <= minmaxdistidx[0]) {
								  block->pass_slopes[i] = num_vts - i_vt + 1;
								  break;
							  }
							  else
								  block->pass_slopes[i] = 0;
						  }
					  }
					//  printf("block->pass_slope[%i]=%i\n", i, block->pass_slopes[i]);
				  }
			  }
		  }

		  //if (percep_coding == true)
		  //{
			 // for (i_vt = 0; i_vt < num_vts; i_vt++) {
				//  kdu_uint16 min_value = 65535;
				//  if ((max_coeff<jnd_threshold[i_vt]) || (jnd_threshold[i_vt]>100))
				//  {
				//	  for (int i = 0; i < block->num_passes; i++)
				//	  {
				//		  block->pass_slopes[i] = 0;
				//		  printf("block->pass_slope[%i]=%i\n", i, block->pass_slopes[i]);
				//	  }
				//  }
				//  else
				//  {
				//	  for (int i = 0; i < block->num_passes; i++)
				//	  {
				//		  /*if (pass_dist[i] < min_value )
				//		  {
				//		  min_value = pass_dist[i];
				//		  block->pass_slopes[i] = pass_dist[i];
				//		  }
				//		  else
				//		  block->pass_slopes[i] = 0;*/
				//		  if (below_threshold[i_vt] == true)
				//		  {
				//			  if (i <= firstlowidx[i_vt])
				//				  block->pass_slopes[i] = 2;
				//			  else
				//				  block->pass_slopes[i] = 0;
				//		  }
				//		  else
				//		  {
				//			  if (i <= minmaxdistidx[0])
				//				  block->pass_slopes[i] = 2;
				//			  else
				//				  block->pass_slopes[i] = 0;
				//		  }
				//		  printf("block->pass_slope[%i]=%i\n", i, block->pass_slopes[i]);
				//	  }
				//  }
			 // }
		  //}



          if (reversible && (block->num_passes > 0) &&
              (block->num_passes == full_reversible_passes))
            { /* Must force last coding pass onto convex hull.  Otherwise, a
                 decompressor using something other than the mid-point rounding
                 rule can fail to achieve lossless decompression. */
              z = full_reversible_passes-1;
              if (block->pass_slopes[z] == 0)
                block->pass_slopes[z] = 1; /* Works because
                      `find_convex_hull_slopes' never generates slopes of 1. */
            }
        }
    } while ((--cpu_counter) > 0);
  block->finish_timing();
}
