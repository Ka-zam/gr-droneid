#include <stdio.h>
#include <stdlib.h> //rand()
#include <unistd.h>
#include <complex.h>
#include <math.h>
#include <time.h>
#include <inttypes.h>

#include <fftw3.h>

/*
gcc -o benchfft -march=native -O3 -std=c99 benchfft.c -lm -lfftw3
*/

typedef _Complex float cxf_t;
typedef struct io_s
{
    uint64_t num_fft;
    cxf_t   *data;
    uint64_t num_data;
    cxf_t   *fft_in;
    cxf_t   *fft_out;
    cxf_t   *taps;
    uint64_t num_taps;
    fftwf_plan p;
} io_t;

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

#include <time.h>

struct timespec 
tic() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts;
}

uint64_t 
toc(struct timespec ts){
    struct timespec te;
    clock_gettime(CLOCK_REALTIME, &te);
    uint64_t ns = (te.tv_sec - ts.tv_sec) * 1000000000UL + (te.tv_nsec - ts.tv_nsec);
    return ns;
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

/*
def overlap_save(x, h, Nfft=None):
    Nfft = max(Nfft or x.size + h.size - 1, h.size)
    hf = np.conj(np.fft.fft(h, Nfft))

    nvalid = Nfft - h.size + 1
    y = np.empty_like(x)
    offset = 0
    chunk = np.zeros(Nfft, dtype=x.dtype)
    while offset < x.size:
        thislen = min(offset + Nfft, x.size) - offset
        chunk[thislen:] = 0
        chunk[:thislen] = x[offset:offset + thislen]
        chunk = np.fft.ifft(np.fft.fft(chunk) * hf)

        thisend = min(offset + nvalid, x.size) - offset
        if np.iscomplexobj(y):
            y[offset:offset + thisend] = chunk[:thisend]
        else:
            y[offset:offset + thisend] = chunk[:thisend].real

        offset += nvalid
    return y
*/

void
overlap_save(const fftwf_plan *p, 
        const size_t nfft, 
        const cxf_t *in, 
        size_t num_in, 
        cxf_t *out, 
        cxf_t *taps_f, 
        size_t num_taps) 
{
    const uint64_t overlap = nfft - num_taps + 1;
    uint64_t ptr = 0;

    while (ptr < num_in) {
        ptr += overlap;
    }

}

int
main(int argc, char** argv) {
    //cxf_t c= fun(1.f+I,2.fI);
    //printf("%f%+fi\n", creal(c), cimag(c));
    const size_t N = 1024*4;
    cxf_t *in, *out;
    in = (cxf_t*) fftwf_malloc( N * sizeof(cxf_t));
    out = (cxf_t*) fftwf_malloc( N * sizeof(cxf_t));
    fftwf_plan p;
    
    printf("Using FFTW3 version: %s\n", fftwf_version);
    if (fftwf_import_wisdom_from_filename("fftw3f.wisdom")) {
        printf("%s\n", "Imported wisdom");      
    }

    p = fftwf_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_WISDOM_ONLY);
    if (p) {
        printf("%s\n", "Generated plan from wisdom");
    } else {
        printf("%s\n", "There was no wisdom, generating...");
        p = fftwf_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_PATIENT);
        fftwf_export_wisdom_to_filename("fftw3f.wisdom");
    }
    
    gen_data(in, N);
    save("in.fc32", in, N);

    struct timespec now = tic();

    /*
    */
    for (int i = 0; i < 1000000; ++i)
    {
        fftwf_execute(p);
    }
    //sleep(1.0);
    uint64_t nanos = toc(now);

    printf("%s in %9.3f ms\n", "Executed FFT", nanos / 1000000.f);
    fftwf_destroy_plan(p);

    save("out.fc32", out, N);

    fftwf_free(in);
    fftwf_free(out);
    return 0;
}