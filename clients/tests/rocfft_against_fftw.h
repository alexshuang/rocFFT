
/*******************************************************************************
 * Copyright (C) 2016 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************/


#include <gtest/gtest.h>
#include<math.h>
#include <stdexcept>
#include <vector>

#include "rocfft.h"
#include "test_constants.h"
#include "rocfft_transform.h"
#include "fftw_transform.h"




/*****************************************************/
/*****************************************************/
// dimension is inferred from lengths.size()
// tightly packed is inferred from strides.empty()
template< class T, class fftw_T >
void complex_to_complex( data_pattern pattern, rocfft_transform_type transform_type ,
	std::vector<size_t> lengths, size_t batch,
	std::vector<size_t> input_strides, std::vector<size_t> output_strides,
	size_t input_distance, size_t output_distance,
	rocfft_array_type  in_array_type , rocfft_array_type  out_array_type ,
	rocfft_result_placement placeness,
	T scale = 1.0f )
{

	rocfft<T> test_fft( lengths.size(), &lengths[0],
		input_strides.empty() ? NULL : &input_strides[0],
		output_strides.empty() ? NULL : &output_strides[0],
		batch, input_distance, output_distance,
		in_array_type, out_array_type,
		placeness );

	fftw<T, fftw_T> reference( lengths.size(), &lengths[0], batch, c2c );

	if( pattern == sawtooth )
	{
		test_fft.set_input_to_sawtooth( 1.0f );
		reference.set_data_to_sawtooth( 1.0f );
	}
	else if( pattern == value )
	{
		test_fft.set_input_to_value( 2.0f, 2.5f );
		reference.set_all_data_to_value( 2.0f, 2.5f );
	}
	else if( pattern == impulse )
	{
		test_fft.set_input_to_impulse();
		reference.set_data_to_impulse();
	}
	else if( pattern == erratic )
	{
		test_fft.set_input_to_random();
		reference.set_data_to_random();
	}
	else
	{
		throw std::runtime_error( "invalid pattern type in complex_to_complex()" );
	}

	// if we're starting with unequal data, we're destined for failure
	EXPECT_EQ( true, test_fft.input_buffer() == reference.input_buffer() );

	if( transform_type  == rocfft_transform_type_complex_forward )
	{
		test_fft.set_forward_transform();
		test_fft.forward_scale( scale );

		reference.set_forward_transform();
		reference.forward_scale( scale );
	}
	else if( transform_type  == rocfft_transform_type_complex_inverse )
	{
		test_fft.set_backward_transform();
		test_fft.backward_scale( scale );

		reference.set_backward_transform();
		reference.backward_scale( scale );
	}
	else{
		throw std::runtime_error( "invalid transform_type  in complex_to_complex()" );
	}

	reference.transform();
	test_fft.transform();

	EXPECT_EQ( true, test_fft.result() == reference.result() );

}

/*****************************************************/
/*****************************************************/

