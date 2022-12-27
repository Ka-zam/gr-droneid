#include <stdio.h>
#include <math.h>
#include <complex.h>
#include <stdlib.h>
#include <fftw3.h>

/*
gcc -o benchfft -march=native -O3 -std=c99 benchfft.c -lm -lfftw3
*/

float _Complex 
fun(float _Complex x,float _Complex y){
    return 1.234f + 4.567*I;
}

void
save(char* fn, const float _Complex *data, const size_t num) {
    FILE* fp;
    fp = fopen(fn,"wb");
    fwrite((void*)data, sizeof(float _Complex), num, fp);
    fclose(fp);
}

float
randn() {
    float u = ((float) rand() / (RAND_MAX)) * 2 - 1;
    float v = ((float) rand() / (RAND_MAX)) * 2 - 1;
    float r = u * u + v * v;
    if (r == 0 || r > 1) return randn();
    float c = sqrtf(-2.f * logf(r) / r);
    return u * c;
}

void
gen_data(_Complex float *data, size_t num) {
    float f1 = 33.f;
    float f2 = 67.f;
    float a1 = 1.234f;
    float a2 = 2.345f;
    float n = 1.f;
    for (int i = 0; i < num; ++i) {
        *data  = a1 * cexp(I * 2.f * M_PI * f1 * (float) i / (float) num);
        *data += a2 * cexp(I * 2.f * M_PI * f2 * (float) i / (float) num);
        *data += n * M_SQRT1_2 * (randn() + I * randn());
        data++;
    }
}

int
main(int argc, char** argv) {
    const size_t N = 1024*2;
    float _Complex *in, *out;
    in = (fftwf_complex*) fftwf_malloc( N * sizeof(fftwf_complex));
    out = (fftwf_complex*) fftwf_malloc( N * sizeof(fftwf_complex));
    fftwf_plan p;
    
    gen_data(in, N);
    save("in.fc32", in, N);
    printf("%s\n", fftwf_version);

    if (fftwf_import_wisdom_from_filename("fftw3f.wisdom")) {
        printf("%s\n", "Imported wisdom");      
    }

    p = fftwf_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_WISDOM_ONLY);
    if (!p) {
        printf("%s\n", "There was no wisdom, generating...");
        p = fftwf_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_PATIENT);
        fftwf_export_wisdom_to_filename("fftw3f.wisdom");
    } else {
        printf("%s\n", "Generated plan from wisdom");
    }

    fftwf_execute(p);
    printf("%s\n", "Executed FFT");
    fftwf_destroy_plan(p);

    save("out.fc32", out, N);

    fftwf_free(in);
    fftwf_free(out);
    return 0;
}