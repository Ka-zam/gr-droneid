#include <vector>
#include <iostream>

/*
g++ -o scratch scratch.cpp
*/

float toa(const std::vector<float> &y) {
    const float a = .5f * (y.at(0) - y.at(2)) + y.at(1) - y.at(0);
    const float b = y.at(1) - y.at(0) + a;
    return b / (2.f * a);
}

int main(){
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

	return 0;
}