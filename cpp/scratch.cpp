#include <vector>
#include <iostream>
#include <inttypes.h>

/*
g++ -o scratch scratch.cpp
*/
void golden_sequence(int8_t *gs) {
  int nc = 1600;
  int l = 7200;
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

float toa(const std::vector<float> &y) {
    const float a = .5f * (y.at(0) - y.at(2)) + y.at(1) - y.at(0);
    const float b = y.at(1) - y.at(0) + a;
    return b / (2.f * a);
}

int main(){

	constexpr size_t L = 7200;
	int8_t* gs = (int8_t*) malloc(L * sizeof(int8_t));

	golden_sequence(gs);

	for (int i = 0; i < 32; ++i) {
		std::cout << static_cast<int>(gs[i]) << " ";
	}
	std::cout << std::endl;

	/*
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