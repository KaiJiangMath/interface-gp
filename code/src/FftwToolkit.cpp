/*! \file	FftwToolkit.cpp
 *
 *  \brief	FFTW tool;
 *
 */


#include "Head.h"
#include "DataOperators.h"
#include "Mytimer.h"
#include "functs.h"


/**
 * \brief	Obtain Fourier k;
 */
void fn_obt_kIndex	(	tmat<int>		kspace,
						tvec<int>		NCpt )
{
	int dim = NCpt.len;
	int cplxDofs = kspace.row;

	int *k = (int *)malloc(sizeof(int)*dim);
	for (int i = 0; i < dim; i++) k[i] = 0;	

	for (int i = 0; i < cplxDofs; i++)
	{
		for (int j = 0; j < dim; j++)
		{
			if ( k[j] > NCpt.val[j]/2 ) 
				kspace.val[i][j] = k[j] - NCpt.val[j];
			else if ( k[j] == NCpt.val[j]/2 )
				kspace.val[i][j] = 0;
			else
				kspace.val[i][j] = k[j];
		}
		k[dim-1] ++;
		for (int jj = dim-1; jj > 0; jj--)
		{
			if (k[jj] > NCpt.val[jj]-1)
			{
				k[jj] = 0;
				k[jj-1] ++;
			}
		}
	}
	free(k);
}


/**
 * \brief	Obtain global indexes of special positions (N/2);
 */
tvec<int> fn_obt_half_gIndex	(	tvec<int>	NCpt,
									int		cplxDofs )
{
	/* Initialization; */
	tvec<int> gIndex = fn_tvec_init<int> ( cplxDofs );
	fn_tvec_setZero<int> ( gIndex );

	/* temporary variable; */
	int dim = NCpt.len;
	int *k = (int *) malloc( sizeof(int) * dim );
	for ( int i = 0; i < dim; i++ ) k[i] = 0;	

	int ind = 0;
	for (int i = 0; i < cplxDofs; i++)
	{
		for (int j = 0; j < dim; j++)
		{
			if ( k[j] == NCpt.val[j]/2 )
			{
				gIndex.val[ind] = i;
				ind ++ ;
				break;
			}
		}
		k[dim-1] ++;
		for (int jj = dim-1; jj > 0; jj--)
		{
			if (k[jj] > NCpt.val[jj]-1)
			{
				k[jj] = 0;
				k[jj-1] ++;
			}
		}
	}
	free(k);
	gIndex.len = ind;
	return gIndex;
}


/**
 * \brief	Transform Fourier k into the global index;
 */
int fn_kIndex_to_gIndex	( int *k, tvec<int> Ndof )
{
	int index;
	int dim = Ndof.len;
	int *ktmp = (int *)malloc(sizeof(int)*dim);
	for (int i = 0; i < dim; i++)
	{
		if (k[i]<0) ktmp[i] = k[i]+Ndof.val[i];
		else ktmp[i] = k[i];
	}
	
	/**
	 * ktmp[dim-1] + ktmp[dim-2]*Ndof.val[dim-1] + ... 
	 *		+ ktmp[0]*Ndof.val[1]*...*Ndof.val[dim-1];
	 */
	index = ktmp[dim-1];
	for (int i = dim-2; i >= 0; i--)
	{
		int tmp = 1;
		for (int j = i+1; j < dim; j++)
		{
			tmp *= Ndof.val[j]; 
		}
		index += tmp*ktmp[i];
	}
	free(ktmp);
	return index;
}


/**
 * \brief	Calculation of convolution of pow('src', order);
 */
void fn_convolution		(	tvec<fftw_complex>	src,
							tvec<fftw_complex>	orig, 
							stu_fftw_var		*sfftv,
								int				order )
{
	int cplxDofs = src.len;
	double atmp, btmp, aatmp, bbtmp;

	memcpy(sfftv->fftw_Ctmp.val, src.val, sizeof(fftw_complex) * cplxDofs);
	fftw_execute(sfftv->Planc2cBack);   // cplx to real

	for (int i = 0; i < cplxDofs; i++)
	{
		// (a+ib)(c+id) = (ac-bd) + i(ad+bc);
		atmp = sfftv->fftw_Rtmp.val[i][0];
		btmp = sfftv->fftw_Rtmp.val[i][1];
		for (int j = 0; j < order-1; j++)
		{
			aatmp = atmp*sfftv->fftw_Rtmp.val[i][0] - 
					btmp*sfftv->fftw_Rtmp.val[i][1];
			bbtmp = atmp*sfftv->fftw_Rtmp.val[i][1] + 
					btmp*sfftv->fftw_Rtmp.val[i][0];
			atmp = aatmp;
			btmp = bbtmp;
		}
		sfftv->fftw_Rtmp.val[i][0] = atmp;
		sfftv->fftw_Rtmp.val[i][1] = btmp;
	}

	fftw_execute(sfftv->Planc2cFord);  // real to cplx
	fn_tvec_constMultiply_complex ( sfftv->fftw_Ctmp, 1.0/cplxDofs );
	memcpy(orig.val, sfftv->fftw_Ctmp.val, sizeof(fftw_complex)*cplxDofs);
}


/**
 * \brief	Calculation of convolution of 'src1' and 'src2';
 */
void fn_convolution_general		(	tvec<fftw_complex>	src1,
									tvec<fftw_complex>	src2,
									tvec<fftw_complex>	orig, 
									stu_fftw_var		*sfftv )
{
	int cplxDofs = orig.len;

	/* temporary variables; */
	tvec<fftw_complex> src1_Rtmp = fn_tvec_init<fftw_complex> ( cplxDofs );
	tvec<fftw_complex> src2_Rtmp = fn_tvec_init<fftw_complex> ( cplxDofs );
	tvec<fftw_complex> orig_Rtmp = fn_tvec_init<fftw_complex> ( cplxDofs );

	/* FFT for 'src1'; */
	memcpy ( sfftv->fftw_Ctmp.val, src1.val, sizeof(fftw_complex) * cplxDofs );
	fftw_execute(sfftv->Planc2cBack);   // cplx to real
	memcpy ( src1_Rtmp.val, sfftv->fftw_Rtmp.val, sizeof(fftw_complex) * cplxDofs );

	/* FFT for 'src2'; */
	memcpy ( sfftv->fftw_Ctmp.val, src2.val, sizeof(fftw_complex) * cplxDofs );
	fftw_execute(sfftv->Planc2cBack);   // cplx to real
	memcpy ( src2_Rtmp.val, sfftv->fftw_Rtmp.val, sizeof(fftw_complex) * cplxDofs );

	/* Calculate convolution; */
	for (int i = 0; i < cplxDofs; i++)
	{
		fn_complex_multiply	( src1_Rtmp.val[i], src2_Rtmp.val[i], orig_Rtmp.val[i] );
	}

	/* back to Fourier space; */
	memcpy ( sfftv->fftw_Rtmp.val, orig_Rtmp.val, sizeof(fftw_complex) * cplxDofs );
	fftw_execute ( sfftv->Planc2cFord );  // real to cplx
	fn_tvec_constMultiply_complex ( sfftv->fftw_Ctmp, 1.0/cplxDofs );
	memcpy ( orig.val, sfftv->fftw_Ctmp.val, sizeof(fftw_complex) * cplxDofs );

	/* release memory; */
	fn_tvec_free<fftw_complex> ( src1_Rtmp );
	fn_tvec_free<fftw_complex> ( src2_Rtmp );
	fn_tvec_free<fftw_complex> ( orig_Rtmp );
}
