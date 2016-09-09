
#ifndef PLAN_H
#define PLAN_H

#ifndef nullptr
#define nullptr NULL
#endif

#define MIN(A,B) (((A) < (B)) ? (A) : (B))

struct rocfft_plan_description_t
{

	rocfft_array_type inArrayType, outArrayType;

	size_t inStrides[3];
	size_t outStrides[3];

	size_t inDist;
	size_t outDist;

	size_t inOffset[2];
	size_t outOffset[2];

	double scale;

	rocfft_plan_description_t()
	{
		inArrayType  = rocfft_array_type_complex_interleaved;
		outArrayType = rocfft_array_type_complex_interleaved;

		inStrides[0] = 0;
		inStrides[1] = 0;
		inStrides[2] = 0;

		outStrides[0] = 0;
		outStrides[1] = 0;
		outStrides[2] = 0;

		inDist = 0;
		outDist = 0;

		inOffset[0]  = 0;
		inOffset[1]  = 0;
		outOffset[0] = 0;
		outOffset[1] = 0;

		scale = 1.0;
	}
};

struct rocfft_plan_t
{
	size_t rank;
	size_t lengths[3];
	size_t batch;

	rocfft_result_placement	placement;
	rocfft_transform_type	transformType;
	rocfft_precision	precision;

	rocfft_plan_description_t desc;

	rocfft_plan_t() :
		placement(rocfft_placement_inplace),
		rank(1),
		batch(1),
		transformType(rocfft_transform_type_complex_forward),
		precision(rocfft_precision_single)
	{
		lengths[0] = 1;
		lengths[1] = 1;
		lengths[2] = 1;
	}	
};



#endif // PLAN_H


