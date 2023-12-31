#include <vector>
#include <iostream>
#include <inttypes.h>
#include <filesystem>
#include <complex>
#include <algorithm>
#include <chrono>
#include <cstring>

using cxf_t = std::complex<float>;
using namespace std::complex_literals;
typedef float fftwf_complex[2];


//typedef float cxf_t[2];

/*
g++ -o scratch -std=c++17 -O2 -march=native scratch.cpp
*/
void 
golden_sequence(int8_t *gs) 
{
  constexpr int nc = 1600;
  constexpr int l = 7200;
  int8_t x1[l+nc+31] = {1};
  int8_t x2[l+nc+31] = {0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0};
  for(int i=0;i<l+nc;i++) {
    x1[i + 31] = (x1[i + 3] + x1[i]) % 2;
    x2[i + 31] = (x2[i + 3] + x2[i + 2] + x2[i + 1] + x2[i]) % 2;
  }

  for(int i=0; i < l; i++) {
    gs[i] = (x1[i + nc] + x2[i + nc]) % 2;
  }
}

float 
toa(const std::vector<float> &y) 
{
    const float a = .5f * (y[0] - y[2]) + y[1] - y[0];
    const float b = y[1] - y[0] + a;
    return b / (2.f * a);
}

void 
mfftshift(std::vector<cxf_t> &in, const int32_t direction) {
	// FFTW3 defines forward as -1
	const size_t n = (in.size() / 2) + ((in.size() % 2) && (direction < 0));
	cxf_t tmp;
  	for(int k = 0; k < n; k++)
	{
		memcpy(&tmp, &in[k], sizeof(cxf_t));
		memcpy(&in[k], &in[k + n], sizeof(cxf_t));
		memcpy(&in[k + n], &tmp, sizeof(cxf_t));
	}	
}

void 
bfftshift(std::vector<cxf_t> &vec, const int32_t direction) {
	// FFTW3 defines forward as -1
	const size_t n = (vec.size() / 2) + ((vec.size() % 2) && (direction < 0));
	std::rotate(vec.begin(), vec.begin() + n, vec.end());
}

void 
fftwf_fftshift(fftwf_complex *in, int N)
{
  fftwf_complex tmp;
  int c = N / 2;
  for(int k = 0; k < c; k++)
  {
    memcpy(&tmp, in + k, sizeof(fftwf_complex));
    memcpy(in + k, in + k + c, sizeof(fftwf_complex));
    memcpy(in + k + c, &tmp, sizeof(fftwf_complex));
  }
}


int 
main(int argc, char** argv)
{
	constexpr int N = 500000;
	std::vector<cxf_t> samples(N);
	std::vector<cxf_t> tmp(N);

	for (int i = 0; i < N; ++i)	{
		samples[i] = (float) i * (1.f + 1.if);
	}
	tmp = samples;



	samples = tmp;
	auto start = std::chrono::high_resolution_clock::now();
	bfftshift(samples, -1);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = (end - start);
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(duration); // Microsecond (as int)
	std::cout << "bfftshift fwd: " << us.count() << " us\n";
	/*
	for (auto &v: samples){
		std::cout << v << " ";
	}
	std::cout << std::endl;
	*/
	
	samples = tmp;
	start = std::chrono::high_resolution_clock::now();
	fftwf_fftshift((fftwf_complex*) samples.data(), samples.size());
    end = std::chrono::high_resolution_clock::now();
    duration = (end - start);
    us = std::chrono::duration_cast<std::chrono::microseconds>(duration); // Microsecond (as int)	
	std::cout << "fftwf_fftshift: " << us.count() << " us\n";
	/*
	for (auto &v: samples){
		std::cout << v << " ";
	}
	std::cout << std::endl;
	*/

	samples = tmp;
	start = std::chrono::high_resolution_clock::now();
	mfftshift(samples, -1);
    end = std::chrono::high_resolution_clock::now();
    duration = (end - start);
    us = std::chrono::duration_cast<std::chrono::microseconds>(duration); // Microsecond (as int)	
	std::cout << "mfftshift: " << us.count() << " us\n";
	/*
	for (auto &v: samples){
		std::cout << v << " ";
	}
	std::cout << std::endl;
	*/	

	return 0;
	/*
	cxf_t x = cxf_t(1.f, 2.f);
	x += 3.f;
	std::cout << x << std::endl;

	cxf_t y = {3,5};
	y += 3.if;
	std::cout << y << std::endl;

	constexpr int N = 8;
	cxf_t z_arr[N];

	float x = 0.f;
	for (int i = 0; i < N; i++) {
		z_arr[i][0] = x;
		x += 1.f;
		z_arr[i][1] = x;
		x += 1.f;
	}
	

	for(auto &v: z_arr) {
		//std::cout << v.real() << "  " << v.imag() << std::endl;
		std::cout << v[0] << "  " << v[1] << std::endl;
	}
	std::vector<std::string> args;
	if (argc > 1) {
		args.assign(argv + 1, argv + argc);
	} else {
		args.push_back(".");
	}
	
	if (!std::filesystem::exists(args[0])) {
		std::cout << "File not found: " << args[0] << "\n";
		return 0;
	}

	std::vector<std::string> files;
	for (const auto &entry: std::filesystem::directory_iterator(args[0])) {
		files.push_back(entry.path());
	}

	for (const auto &f: files) {
		std::cout << f << std::endl;
	}

	return 0;
	constexpr size_t L = 7200;
	int8_t* gs = (int8_t*) malloc(L * sizeof(int8_t));

	golden_sequence(gs);

	for (int i = 0; i <= 32; ++i) {
		std::cout << static_cast<int>(gs[i]) << " ";
	}
	std::cout << std::endl;

	int16_t vec[8] = {1};
	for(auto &v: vec) {
		std::cout << v << " ";
	}

	std::vector<int> vec = {1,2,3,4,5,6,7,8};

	for (auto &v: vec){
		std::cout << v << " ";
	}
	std::cout << std::endl;

	int n = 5;
	std::vector<int> data(n,0);
    std::move(vec.begin(), vec.end() - n, vec.begin() + n );
    std::copy(data.begin(), data.end() , vec.begin() );

	for (auto &v: vec){
		std::cout << v << " ";
	}
	std::cout << std::endl;

	std::vector<int> v2;
	v2.resize(3);
	v2[4] = 1;
	for (auto &v: v2){
		std::cout << v << " ";
	}
	std::cout << std::endl;

	std::vector<float> y = {0.81f, 1.0f, 0.2f};
	std::cout << toa(y) << "\n";
	*/

	return 0;
}