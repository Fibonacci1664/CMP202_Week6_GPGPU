// Data Structures and Algorithms II : Intro to AMP and benchmarking exercise
// Ruth Falconer  <r.falconer@abertay.ac.uk>
// Adapted from C++ AMP book http://ampbook.codeplex.com/license.

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

/* 
 * From the size alterations below we can see that for the GPU to perform better than
 * the GPU the size must be of >= 2^18.
 */

#define SIZE 1<<24	// same as 2^24 = 16,777,216			GPU faster
//#define SIZE 1<<10	// same as 2^10 = 1024					CPU faster
//#define SIZE 1<<15	// same as 2^15 = 32,768				CPU faster
//#define SIZE 1<<20	// same as 2^20 = 1,048,576				GPU faster
//#define SIZE 1<<17	// same as 2^17 = 131,072				ROUGHLY THE SAME

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

void report_accelerator(const accelerator a)
{
	const std::wstring bs[2] = { L"false", L"true" };
	std::wcout << ": " << a.description << " "
		<< endl << "       device_path                       = " << a.device_path
		<< endl << "       dedicated_memory                  = " << std::setprecision(4) << float(a.dedicated_memory) / (1024.0f * 1024.0f) << " Mb"
		<< endl << "       has_display                       = " << bs[a.has_display]
		<< endl << "       is_debug                          = " << bs[a.is_debug]
		<< endl << "       is_emulated                       = " << bs[a.is_emulated]
		<< endl << "       supports_double_precision         = " << bs[a.supports_double_precision]
		<< endl << "       supports_limited_double_precision = " << bs[a.supports_limited_double_precision]
		<< endl;
}
// List and select the accelerator to use
void list_accelerators()
{
	//get all accelerators available to us and store in a vector so we can extract details
	std::vector<accelerator> accls = accelerator::get_all();

	// iterates over all accelerators and print characteristics
	for (int i = 0; i < accls.size(); i++)
	{
		accelerator a = accls[i];
		report_accelerator(a);

	}

	//Use default accelerator
	accelerator a = accelerator(accelerator::default_accelerator);
	std::wcout << " default acc = " << a.description << endl;
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
void vector_add(const int size, const std::vector<double>& v1, const std::vector<double>& v2, std::vector<double>& v3)
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
} // vector_add

// Accelerated  element-wise addition of arbitrary length vectors using C++ 11 container vector class
void vector_add_amp(const int size, const std::vector<double>& v1, const std::vector<double>& v2, std::vector<double>& v3)
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
} // vector_add_amp


template <typename T>
void array_add(const std::array<T, SIZE>& arr1, const std::array<T, SIZE>& arr2, std::array<T, SIZE>& arr3)
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
	cout << "Adding the vectors serially using the CPU took: " << time_taken << " ms." << endl;
} // vector_add

 // Accelerated  element-wise addition of arbitrary length vectors using C++ 11 container vector class
template <typename T>
void array_add_amp(const std::array<T, SIZE>& arr1, const std::array<T, SIZE>& arr2, std::array<T, SIZE>& arr3)
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
	cout << "Adding the vectors using AMP (data transfer and compute) took: " << time_taken << " ms." << endl;
} // vector_add_amp



int main(int argc, char* argv[])
{
	// Check AMP support
	query_AMP_support();

	//fill the arrays - try ints versus floats & double -- change via templating
	/*std::vector<double> v1(SIZE, 1.0);
	std::vector<double> v2(SIZE, 2.0);
	std::vector<double> v3(SIZE, 0.0);*/


	//compare a serial and parallel version of vector addtion
	/*vector_add_amp(SIZE, v1, v2, v3);
	vector_add(SIZE, v1, v2, v3);*/

	static const std::array<double, SIZE> arr1 = { 1 };
	static const std::array<double, SIZE> arr2 = { 2 };
	static std::array<double, SIZE>  arr3 = { 0 };

	array_add(arr1, arr2, arr3);
	array_add_amp(arr1, arr2, arr3);

	// THIS CODE IS DEADLY, EVEN UNCOMMENTING IT WILL SLOW DOWN PERFORMANCE OF COMPUTER.
	/*const std::array<int, SIZE>* arr1 = new std::array<int, SIZE> { 1 };
	const std::array<int, SIZE>* arr2 = new std::array<int, SIZE> { 2 };
	std::array<int, SIZE>*  arr3 = new std::array<int, SIZE> { 0 };

	array_add(&arr1, &arr2, &arr3);
	array_add_amp(&arr1, &arr2, &arr3);*/

	return 0;
} // main