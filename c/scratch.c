#include <stdio.h>
#include <math.h>
#include <complex.h>

/*
gcc -o scratch -std=c99 scratch.c -lm
*/
float _Complex fun(float _Complex x,float _Complex y){
	return 1.234f + 4.567*I;
}

int
main(int argc, char** argv) {
	float _Complex x, y, z;
	x = -1.f + 2.f*I;
	y = 1.f - 3.f*I;
	z = csinf( y / x );
	printf("x       : %0.3f%+0.3fi\n", creal(x), cimag(x));
	printf("y       : %0.3f%+0.3fi\n", creal(y), cimag(y));
	printf("sin(y/x): %0.3f%+0.3fi\n", creal(z), cimag(z));
	z = fun(1.f, 5.f);
	printf("z       : %0.3f%+0.3fi\n", creal(z), cimag(z));
	return 0;
}