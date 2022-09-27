#include <vector>
#include <iostream>

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
	return 0;
}