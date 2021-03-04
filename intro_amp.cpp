// Data Structures and Algorithms II : Intro to AMP and benchmarking exercise
// Ruth Falconer  <r.falconer@abertay.ac.uk>
// Adapted from C++ AMP book http://ampbook.codeplex.com/license.

//https://docs.microsoft.com/en-us/cpp/parallel/amp/cpp-amp-cpp-accelerated-massive-parallelism?view=msvc-160

//https://docs.microsoft.com/en-us/cpp/parallel/amp/reference/reference-cpp-amp?view=msvc-160

/*************************To do in lab ************************************/
//Change size of the vector/array
//Compare Debug versus Release modes
//Add more work to the loop until the GPU is faster than CPU
//Compare double versus ints via templating  

#include <chrono>
#include <iostream>
#include <iomanip>
#include <amp.h>
#include <time.h>
#include <string>
#include <array>
#include <assert.h>
#include <fstream>

/* 
 * From the size alterations below we can see that for the GPU to perform better than
 * the GPU the size must be of >= 2^18. (DEBUG MODE)
 * I still need to find at what SIZE the GPU outperforms the CPU (RELEASE MODE) but I can't
 * as I keep getting an error if I try and use a SIZE 1<<25 or greater.
 */

//#define SIZE 1<<25		// same as 2^25 = 33,554,432			GIVES ME THE ERROR OF DOOM!
#define SIZE 1<<24	// same as 2^24 = 16,777,216			GPU faster
//#define SIZE 1<<20	// same as 2^20 = 1,048,576				GPU faster
//#define SIZE 1<<17	// same as 2^17 = 131,072				ROUGHLY THE SAME
//#define SIZE 1<<15	// same as 2^15 = 32,768				CPU faster
//#define SIZE 1<<10	// same as 2^10 = 1024					CPU faster


////fill the arrays - try ints versus floats & double -- change via templating
std::vector<double> v1(SIZE, 1.0);
std::vector<double> v2(SIZE, 2.0);
std::vector<double> v3(SIZE, 0.0);

static const std::array<double, SIZE> arr1 = { 1 };
static const std::array<double, SIZE> arr2 = { 2 };
static std::array<double, SIZE>  arr3 = { 0 };

// Need to access the concurrency libraries 
using namespace concurrency;

// Import things we need from the standard library
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::cout;
using std::endl;

// Define the alias "the_clock" for the clock type we're going to use.
typedef std::chrono::steady_clock the_serial_clock;
typedef std::chrono::steady_clock the_amp_clock;

std::ofstream timings ("timings.csv");

//get all accelerators available to us and store in a vector so we can extract details
std::vector<accelerator> accls = accelerator::get_all();

void report_accelerator(const accelerator a)
{
	const std::wstring bs[2] = { L"false", L"true" };

	std::wcout << ": " << a.description << " "
		<< endl << "		device_path                       = " << a.device_path
		<< endl << "		version = " << (a.version >> 16) << '.' << (a.version & 0xFFFF)
		<< endl << "		dedicated_memory                  = " << std::setprecision(4) << float(a.dedicated_memory) / (1024.0f * 1024.0f) << " Mb"
		<< endl << "		has_display                       = " << bs[a.has_display]
		<< endl << "		supports debug                    = " << bs[a.is_debug]
		<< endl << "		is_emulated                       = " << bs[a.is_emulated]
		<< endl << "		supports_double_precision         = " << bs[a.supports_double_precision]
		<< endl << "		supports_limited_double_precision = " << bs[a.supports_limited_double_precision]
		<< endl;
}
// List and select the accelerator to use
void list_accelerators()
{
	// iterates over all accelerators and print characteristics
	for (int i = 0; i < accls.size(); i++)
	{
		accelerator a = accls[i];
		report_accelerator(a);
	}


	//http://www.danielmoth.com/Blog/Running-C-AMP-Kernels-On-The-CPU.aspx

	//Use default accelerator
	accelerator a = accelerator(accelerator::default_accelerator);
	accelerator::set_default(a.device_path);
	std::wcout << " Using default acc = " << a.description << endl;

	//https://docs.microsoft.com/en-gb/windows/win32/direct3darticles/directx-warp?redirectedfrom=MSDN

	/*
	Recommended Application Types for WARP
	All applications that can use Direct3D can use WARP. This includes the following types of applications:

	Casual Games
	Existing Non-Gaming Applications
	Advanced Rendering Games
	Other Applications
	*/

	//Use Direct 3D WARP accelerator, this is an emulator, it is software on the CPU that emulates GPU hardware * This does not support double precision *
	/*accelerator a = accelerator(accelerator::direct3d_warp);
	accelerator::set_default(a.device_path);
	std::wcout << " Using Direct3D WARP acc = " << a.description << endl;*/

	//http://www.danielmoth.com/Blog/concurrencyaccelerator.aspx

	//Use Direct 3D REF accelerator, this is an emulator, it is software on the CPU that emulates GPU hardware* This is extremely slow and is only intended for debugging *
	/*
	represents the reference rasterizer emulator that simulates a direct3d device on the CPU (in a very slow manner).
	This emulator is available on systems with Visual Studio installed and is useful for debugging
	*/
	/*accelerator a = accelerator(accelerator::direct3d_ref);
	accelerator::set_default(a.device_path);
	std::wcout << " Using Direct3D REF acc = " << a.description << endl;*/

	/*
	Some great info here about different accelerators:
	//https://www.microsoftpressstore.com/articles/article.aspx?p=2201645
	*/

	//Use CPU accelerator - Does NOT support Debug mode.
	/*
	You cannot execute code on the CPU accelerator.
	ref: //https://docs.microsoft.com/en-us/cpp/parallel/amp/using-accelerator-and-accelerator-view-objects?view=msvc-160
	*/
	/*accelerator a = accelerator(accelerator::cpu_accelerator);
	accelerator::set_default(a.device_path);
	std::wcout << " Using CPU acc = " << a.description << endl;*/
} // list_accelerators

// query if AMP accelerator exists on hardware
void query_AMP_support()
{
	std::vector<accelerator> accls = accelerator::get_all();

	if (accls.empty())
	{
		cout << "No accelerators found that are compatible with C++ AMP" << std::endl;
	}
	else
	{
		cout << "Accelerators found that are compatible with C++ AMP" << std::endl;
		list_accelerators();
	}
} // query_AMP_support

// Unaccelerated CPU element-wise addition of arbitrary length vectors using C++ 11 container vector class
long long vector_add(const int size, const std::vector<double>& v1, const std::vector<double>& v2, std::vector<double>& v3)
{
	//start clock for serial version
	the_serial_clock::time_point start = the_serial_clock::now();
	// loop over and add vector elements
	for (int i = 0; i < v3.size(); i++)
	{
		v3[i] = v2[i] + v1[i];
		
	}
	the_serial_clock::time_point end = the_serial_clock::now();
	//Compute the difference between the two times in milliseconds
	auto time_taken = duration_cast<milliseconds>(end - start).count();
	cout << "Adding the vectors serially using the CPU took: " << time_taken << " ms." << endl;

	return time_taken;
} // vector_add

// Accelerated  element-wise addition of arbitrary length vectors using C++ 11 container vector class
long long vector_add_amp(const int size, const std::vector<double>& v1, const std::vector<double>& v2, std::vector<double>& v3, bool warmup)
{
	concurrency::array_view<const double> av1(size, v1);
	concurrency::array_view<const double> av2(size, v2);
	extent<1> e(size);
	concurrency::array_view<double> av3(e, v3);
	av3.discard_data();
	// start clock for GPU version after array allocation
	the_amp_clock::time_point start = the_amp_clock::now();
	// It is wise to use exception handling here - AMP can fail for many reasons
	// and it useful to know why (e.g. using double precision when there is limited or no support)
	try
	{
		concurrency::parallel_for_each(av3.extent, [=](concurrency::index<1> idx)  restrict(amp)
			{
				av3[idx] = av1[idx] + av2[idx];				
			});
		av3.synchronize();
	}
	catch (const concurrency::runtime_exception & ex)
	{
		MessageBoxA(NULL, ex.what(), "Error", MB_ICONERROR);
	}
	// Stop timing
	the_amp_clock::time_point end = the_amp_clock::now();
	// Compute the difference between the two times in milliseconds
	auto time_taken = duration_cast<milliseconds>(end - start).count();
	cout << "Adding the vectors using AMP (data transfer and compute) took: " << time_taken << " ms." << endl;

	if (warmup)
	{
		av3.get_source_accelerator_view().wait();
	}
	
	return time_taken;
} // vector_add_amp


template <typename T>
long long array_add(const std::array<T, SIZE>& arr1, const std::array<T, SIZE>& arr2, std::array<T, SIZE>& arr3)
{
	//start clock for serial version
	the_serial_clock::time_point start = the_serial_clock::now();
	// loop over and add vector elements
	for (int i = 0; i < arr3.size(); i++)
	{
		arr3[i] = arr2[i] + arr1[i];

	}
	the_serial_clock::time_point end = the_serial_clock::now();
	//Compute the difference between the two times in milliseconds
	auto time_taken = duration_cast<milliseconds>(end - start).count();
	cout << "Adding the arrays serially using the CPU took: " << time_taken << " ms." << endl;

	return time_taken;
} // vector_add

 // Accelerated  element-wise addition of arbitrary length vectors using C++ 11 container vector class
template <typename T>
long long array_add_amp(const std::array<T, SIZE>& arr1, const std::array<T, SIZE>& arr2, std::array<T, SIZE>& arr3, bool warmup)
{
	concurrency::array_view<const T> av1(arr1.size(), arr1);
	concurrency::array_view<const T> av2(arr2.size(), arr2);
	extent<1> e(SIZE);
	concurrency::array_view<T> av3(e, arr3);
	av3.discard_data();
	// start clock for GPU version after array allocation
	the_amp_clock::time_point start = the_amp_clock::now();
	// It is wise to use exception handling here - AMP can fail for many reasons
	// and it useful to know why (e.g. using double precision when there is limited or no support)
	try
	{
		concurrency::parallel_for_each(av3.extent, [=](concurrency::index<1> idx)  restrict(amp)
			{
				av3[idx] = av1[idx] + av2[idx];
			});
		av3.synchronize();
	}
	catch (const concurrency::runtime_exception& ex)
	{
		MessageBoxA(NULL, ex.what(), "Error", MB_ICONERROR);
	}
	// Stop timing
	the_amp_clock::time_point end = the_amp_clock::now();
	// Compute the difference between the two times in milliseconds
	auto time_taken = duration_cast<milliseconds>(end - start).count();
	cout << "Adding the arrays using AMP (data transfer and compute) took: " << time_taken << " ms." << endl;

	if (warmup)
	{
		av3.get_source_accelerator_view().wait();
	}

	return time_taken;

} // vector_add_amp


void vectorWarmUp()
{
	vector_add_amp(SIZE, v1, v2, v3, true);
}

void arrayWarmUp()
{
	array_add_amp(arr1, arr2, arr3, true);
}


int main(int argc, char* argv[])
{
	// Check AMP support
	query_AMP_support();

	///////////////////////////// IMPORTANT INFO RELATED TO THE WARM UP CALLS BELOW /////////////////////////////

	/*
	 * Run these to cache the AMP runtime initialisation.
	 * This way the first timing using the GPU is not significantly slower than the rest.
	 * Otherwise the slower first time is caused by C++ AMP using JIT (Just in Time) lazy initialisation.
	 */
	
	
	// ALWAYS RECORD TIMINGS AFTER THE AMP RUNTIME INTIALISATION HAS BEEN CACHED.

	/* 
	 * Make sure that you have called acceleartor_view.wait() as the parallel_for_each is
	 * an asynchronous call and so we need to make sure that ALL of the pervious uses during
	 * warmup calls are finihsed. Otherwise we may end up including some of the warmups in the timings.
	 */

	 ///////////////////////////// IMPORTANT INFO RELATED TO THE WARM UP CALLS BELOW /////////////////////////////

	timings << "\nVECTOR CPU TIMINGS, ";
	// Time adding vectors on the CPU.
	for (int i = 0; i < 5; ++i)
	{	
		long long time = vector_add(SIZE, v1, v2, v3);	
		timings << time << ", ";
	}

	vectorWarmUp();

	timings << "\nVECTOR GPU TIMINGS, ";
	// Time adding vectors on the GPU.
	for (int i = 0; i < 5; ++i)
	{
		long long time = vector_add_amp(SIZE, v1, v2, v3, false);
		timings << time << ", ";
	}

	timings << "\nARRAY CPU TIMINGS, ";
	// Time adding arrays on the CPU.
	for (int i = 0; i < 5; ++i)
	{		
		long long time = array_add(arr1, arr2, arr3);
		timings << time << ", ";
	}

	arrayWarmUp();

	timings << "\nARRAY GPU TIMINGS, ";
	// Time adding arrays on the GPU.
	for (int i = 0; i < 5; ++i)
	{
		long long time = array_add_amp(arr1, arr2, arr3, false);
		timings << time << ", ";
	}

	
	// THIS CODE IS DEADLY, EVEN UNCOMMENTING IT WILL SLOW DOWN PERFORMANCE OF COMPUTER.
	/*const std::array<int, SIZE>* arr1 = new std::array<int, SIZE> { 1 };
	const std::array<int, SIZE>* arr2 = new std::array<int, SIZE> { 2 };
	std::array<int, SIZE>*  arr3 = new std::array<int, SIZE> { 0 };

	array_add(&arr1, &arr2, &arr3);
	array_add_amp(&arr1, &arr2, &arr3);*/

	return 0;
} // main