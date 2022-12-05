#include <complex>
#include <vector>
#include <iostream>
#include <fstream>
#include <iterator>
#include <inttypes.h>
#include <filesystem>
#include <cstring>
#include <string>

#include <volk/volk.h>
#include <fftw3.h>

using cxf_t = std::complex<float>;
using namespace std::complex_literals;
//typedef float fftwf_complex[2];

/*
g++ -o decoder -std=c++17 -O2 decoder.cpp -lvolk
*/
void
golden_sequence(int8_t *gs) 
{
  constexpr int nc = 1600;
  constexpr int l = 7200;
  int8_t x1[l + nc + 31] = {1};
  int8_t x2[l + nc + 31] = {0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0};
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

/*

Metadata in:
    1. Sample rate
    2. Fc
    3. Sample number
    4. Time stamp relative start of buffer

Decoding steps:
  1.  Channelization
  2.  Downsampling
  3.  Low pass filtering
  4.  Integer Frequency Offset
  5.  Find STO and resample
  6.  CFO estimate
  7.  OFDM symbol extraction
  8.  Channel estimate
  9.  Equalize data carriers
  10. Demodulate QPSK
  11. Descramble
  12. Apply turbo coder (w/ QPSK load bearing constants)

Metadata out:
    1. Fc
    2. CFO
    3. TOA
    4. SNR
    5. STO
    6. SCO
    7. Channel estimate
    8. FEC corrected error list
*/

class decoder
{
private:
    static constexpr uint32_t SHORT_CP_1536 = 72;
    static constexpr uint32_t LONG_CP_1536 = 80;
    static constexpr uint32_t OFDM_DATA_LEN_1536 = 1024;
    static constexpr uint32_t N_ZC_1536 = 601;
    static constexpr uint32_t N_LEFT_GUARD_SUBCARRIERS_1536 = 212;
    static constexpr uint32_t OCU_10MHz_ZC_LEN = 1024;
    static constexpr uint32_t FFT_LEN_1536 = 16384;
    static constexpr uint32_t N_BROADCAST_6144_LEN = 35104;
    static constexpr uint32_t N_BROADCAST_1536_LEN = 8776;
    static constexpr uint32_t OFDM_SYMBOL_LEN_1536 = 1096;

    static constexpr uint32_t n_pre_samples = SHORT_CP_1536 + OFDM_SYMBOL_LEN_1536 * 2;
    static constexpr uint32_t n_drone_id_samples = OFDM_SYMBOL_LEN_1536 * 7 + (LONG_CP_1536 + OFDM_DATA_LEN_1536);
    static constexpr uint32_t d_input_file_bit_count = 7200;

    static constexpr int64_t n_pre_samples = SHORT_CP_1536 + OFDM_SYMBOL_LEN_1536 * 2;
    static constexpr int64_t n_post_samples = OFDM_DATA_LEN_1536 + OFDM_SYMBOL_LEN_1536 * 4 + (LONG_CP_1536 + OFDM_DATA_LEN_1536);
    static constexpr int64_t n_drone_id_samples = OFDM_SYMBOL_LEN_1536 * 7 + (LONG_CP_1536 + OFDM_DATA_LEN_1536);    

    fftwf_complex *m_in, *m_out;
    fftwf_complex *m_ofdm_in, *m_ofdm_out;

    fftwf_plan m_plan_fwd, m_ofdm_plan;
    int save_complex64(const std::string filename, const std::vector<cxf_t> &vec);

public:
    void broadcast_signal_demodulation(std::vector<uint8_t> &bits, const std::vector<cxf_t> &samples);
    void channel_estimation(fftwf_complex *ZC, std::vector<std::complex<float>> &h, int q);
    void decode_QPSK(fftwf_complex *broadcast_samples, int8_t *qpsk_bits, std::vector<cxf_t> &symbols);
    float ffo_est(cxf_t *samples);
    void fftwf_fftshift(fftwf_complex *ZC_in_f, int N);
    void fftshift(cxf_t *ZC_in_f, int N);
    void bfftshift(std::vector<cxf_t> &vec, const int32_t direction);
    decoder();
    ~decoder();
};

decoder::decoder() {
    m_in = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * FFT_LEN_1536);
    m_out = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * FFT_LEN_1536);    
    m_plan_fwd = fftwf_plan_dft_1d(OCU_10MHz_ZC_LEN, m_in, m_out, FFTW_FORWARD, FFTW_ESTIMATE);

    m_ofdm_in = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * OFDM_DATA_LEN_1536);
    m_ofdm_out = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * OFDM_DATA_LEN_1536);
    m_ofdm_plan = fftwf_plan_dft_1d(OFDM_DATA_LEN_1536, m_ofdm_in, m_ofdm_out, FFTW_FORWARD, FFTW_MEASURE);
}

decoder::~decoder() {
    fftwf_free(m_in);
    fftwf_free(m_out);
    fftwf_free(m_ofdm_in);
    fftwf_free(m_ofdm_out);
}

int
decoder::save_complex64(const std::string filename, const std::vector<cxf_t> &vec){
    FILE* fp;
    fp = fopen(filename.c_str(), "w");
    if (!fp) {return -1;} 
    int num = fwrite(vec.data(), sizeof(cxf_t), vec.size(), fp);
    fclose(fp);
    return num; 
}

float
decoder::ffo_est(cxf_t *samples)
{
  cxf_t sum = cxf_t(0.f, 0.f);
  std::vector<cxf_t> vec(SHORT_CP_1536);

  volk_32fc_x2_multiply_conjugate_32fc(vec.data(), samples, samples + OFDM_DATA_LEN_1536, SHORT_CP_1536);
  
  for(int i = 0;i < SHORT_CP_1536; i++) {
    sum += vec[i];
  }

  return std::atan2(sum.imag(), sum.real());
}

void 
decoder::bfftshift(std::vector<cxf_t> &vec, const int32_t direction) {
    // FFTW3 defines forward as -1
    const size_t n = (vec.size() / 2) + ((vec.size() % 2) && (direction < 0));
    std::rotate(vec.begin(), vec.begin() + n, vec.end());
}

void 
decoder::fftwf_fftshift(fftwf_complex *ZC_in_f, int N)
{

  fftwf_complex tmp;
  int c = N / 2;
  for(int k = 0; k < c; k++)
  {
    memcpy(&tmp, ZC_in_f + k, sizeof(fftwf_complex));
    memcpy(ZC_in_f + k, ZC_in_f + k + c, sizeof(fftwf_complex));
    memcpy(ZC_in_f + k + c, &tmp, sizeof(fftwf_complex));
  }
}

void 
decoder::fftshift(cxf_t *ZC_in_f, int N)
{
  std::complex<float> tmp;
  int c = N/2;
  for(int k=0;k<c;k++)
  {
    tmp = ZC_in_f[k];
    ZC_in_f[k] = ZC_in_f[k+c];
    ZC_in_f[k+c] = tmp;
  }
}

void
decoder::channel_estimation(fftwf_complex *ZC, std::vector<std::complex<float>> &h, int q)
{
  int N = 1024;
  std::complex<float> a,b;
  std::complex<float> f1 = std::complex<float>(0.2,0.f);
  std::complex<float> f2 = std::complex<float>(0.3,0.f);
  std::complex<float> f3 = std::complex<float>(0.4,0.f);
  std::complex<float> f4 = std::complex<float>(0.5,0.f);
  std::complex<float> smooth_factor = std::complex<float>(0.5,0.f);
  std::complex<float> d_fft_samples[N];
  int Nzc = 601;
  fftwf_execute_dft(m_plan_fwd, ZC, reinterpret_cast<fftwf_complex*>(&d_fft_samples[0]));
  fftshift(d_fft_samples, N);
  for(int k = 0; k < Nzc; k++)
  {
    a.real(0);
    a.imag(float(-1.0) * float(q) * float(M_PI) * float(k) * float(k + 1) / float(Nzc));
    b = std::exp(a);
    h.push_back(d_fft_samples[N_LEFT_GUARD_SUBCARRIERS_1536+k]/b);
  }
  for (int k =3; k < Nzc -3; k++)
  {
    h[k] = f1 * h[k-3] + f2 * h[k-2] + f3 * h[k-1] + f4 * h[k] + f3 * h[k+1] + f2 * h[k+2] + f1 * h[k+3];
  }
}

void
decoder::decode_QPSK(fftwf_complex *broadcast_samples, int8_t *qpsk_bits, std::vector<cxf_t> &symbols)
{
  symbols.resize(d_input_file_bit_count / 2);
  int symbols_ptr = 0;
  int index = 0;
  cxf_t tmp;
  std::vector<cxf_t> channel_1;
  std::vector<cxf_t> channel_2;
  channel_estimation(broadcast_samples+1096*2+72, channel_1, 600);
  channel_estimation(broadcast_samples+1096*4+72, channel_2, 147);
  cxf_t d_fft_samples[OFDM_DATA_LEN_1536];
  for(int i = 0;i < 8; i++)
  {
    if(i == 2 or i == 4)
    {
      continue;
    }
    else if(i != 7)
    {
      fftwf_execute_dft(m_ofdm_plan, broadcast_samples + i * 1096 + 72, (fftwf_complex*) d_fft_samples);
      fftwf_fftshift((fftwf_complex*) d_fft_samples, OFDM_DATA_LEN_1536);
    }
    else
    {
      fftwf_execute_dft(m_ofdm_plan, broadcast_samples + i * 1096 + 80, (fftwf_complex*) d_fft_samples);
      fftwf_fftshift((fftwf_complex*) d_fft_samples, OFDM_DATA_LEN_1536);
    }
    if(i <= 2)
    {
      for(int k = 0; k < N_ZC_1536; k++)
      {
        if (k == 300) continue;
        cxf_t temp = d_fft_samples[k + 212];
        //temp.real(d_fft_samples[k + 212][0]);
        //temp.imag(d_fft_samples[k + 212][1]);
        tmp = temp / channel_1[k];
        if (tmp.real() > 0 && tmp.imag() > 0)
        {
          qpsk_bits[index] = 0;
          qpsk_bits[index + 1] = 0;
          index += 2;
        }
        else if (tmp.real() > 0 && tmp.imag() < 0)
        {
          qpsk_bits[index] = 0;
          qpsk_bits[index + 1] = 1;
          index += 2;
        }
        else if (tmp.real() < 0 && tmp.imag() > 0)
        {
          qpsk_bits[index] = 1;
          qpsk_bits[index + 1] = 0;
          index += 2;
        }
        else
        {
          qpsk_bits[index] = 1;
          qpsk_bits[index + 1] = 1;
          index += 2;
        }
        symbols[symbols_ptr] = tmp;
        symbols_ptr++;
      }
    }
    else if(i==3)
    {
      for(int k = 0; k < N_ZC_1536; k++)
      {
        if (k == 300) continue;
        cxf_t temp = d_fft_samples[k + 212];
        //temp.real(d_fft_samples[k + 212][0]);
        //temp.imag(d_fft_samples[k + 212][1]);
        tmp = temp / (channel_1[k]+channel_2[k]);
        if (tmp.real() > 0 && tmp.imag() > 0)
        {
          qpsk_bits[index] = 0;
          qpsk_bits[index + 1] = 0;
          index += 2;
        }
        else if (tmp.real() > 0 && tmp.imag() < 0)
        {
          qpsk_bits[index] = 0;
          qpsk_bits[index + 1] = 1;
          index += 2;
        }
        else if (tmp.real() < 0 && tmp.imag() > 0)
        {
          qpsk_bits[index] = 1;
          qpsk_bits[index + 1] = 0;
          index += 2;
        }
        else
        {
          qpsk_bits[index] = 1;
          qpsk_bits[index + 1] = 1;
          index += 2;
        }
        symbols[symbols_ptr] = tmp;
        symbols_ptr++;        
      }
    }
    else
    {
      for(int k =0;k < N_ZC_1536;k++)
      {
        if (k == 300) continue;
        cxf_t temp = d_fft_samples[k + 212];
        //temp.real(d_fft_samples[k + 212][0]);
        //temp.imag(d_fft_samples[k + 212][1]);
        tmp = temp / channel_2[k];
        if (tmp.real() > 0 && tmp.imag() > 0)
        {
          qpsk_bits[index] = 0;
          qpsk_bits[index + 1] = 0;
          index+=2;
        }
        else if (tmp.real() > 0 && tmp.imag() < 0)
        {
          qpsk_bits[index] = 0;
          qpsk_bits[index + 1] = 1;
          index += 2;
        }
        else if (tmp.real() < 0 && tmp.imag() > 0)
        {
          qpsk_bits[index] = 1;
          qpsk_bits[index + 1] = 0;
          index += 2;
        }
        else
        {
          qpsk_bits[index] = 1;
          qpsk_bits[index + 1] = 1;
          index += 2;
        }
        symbols[symbols_ptr] = tmp;
        symbols_ptr++;        
      }
    }
  }
}

void
decoder::bsd(fftwf_complex *s, 
    int64_t origin_samp_rate, 
    int64_t new_samp_rate, 
    int64_t location, 
    int64_t origin_len, 
    int64_t sampling_fc, 
    int64_t drone_id_fc, 
    int16_t current_round, 
    double fpga_cycles_begin)
{
  double fpga_cycles_peak_location = 
    location * (double)origin_samp_rate / (double)new_samp_rate + fpga_cycles_begin; 
  int64_t fc_offset =  drone_id_fc - sampling_fc;
   
  float SNR;
  if(location < n_pre_samples && location > origin_len - n_post_samples)
  {
    logger->debug("signal_demodulation(): peak idx close to the edge of the signal, skip, idx = {}", location);
    return;
  }
  int down_factor = int(origin_samp_rate / new_samp_rate);
  fftwf_complex broadcast_samples_origin[N_BROADCAST_6144_LEN];
  fftwf_complex broadcast_samples[N_BROADCAST_1536_LEN];
  memcpy(broadcast_samples_origin, s + (location - n_pre_samples) * 4, N_BROADCAST_6144_LEN*sizeof(fftwf_complex));
  // create to feed into the resample so that it does not need to recreate it again. in the end move to constructor of tdoa_coordinator
  fftwf_complex *samples_F = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * N_BROADCAST_6144_LEN);
  resample_broadcast(&broadcast_samples_origin[0], &broadcast_samples[0], N_BROADCAST_6144_LEN, origin_samp_rate, new_samp_rate, fc_offset, samples_F);

  // Estimate and correct the fractional frequency offset(ffo)
  // Calculate the ffo of 8 OFDM symbols and get the mean value
  // ########## SNR estimation ########
  //SNR = OFDM_SNR_estimation(s, origin_samp_rate,new_samp_rate,location,origin_len,sampling_fc,drone_id_fc);
  float d_ffo;
  std::complex<float> d_ffo2 = {0.f, 0.f};
  for(int k = 0;k < 8; k++)
  {
    d_ffo2 += fractional_frequency_offset_estimation(broadcast_samples + k * OFDM_SYMBOL_LEN_1536);
  }
  d_ffo  = std::atan2(d_ffo2.imag(),d_ffo2.real());
  
  std::complex<float> compensate[n_drone_id_samples];
  for(uint i = 0;i < n_drone_id_samples; i++)
  {
    std::complex<float> a;
    a.real(0);
    a.imag(-1*i * d_ffo / 1024);
    compensate[i] = std::exp(a);
  }
  volk_32fc_x2_multiply_conjugate_32fc(reinterpret_cast<std::complex<float>*>(broadcast_samples), reinterpret_cast<std::complex<float>*>(broadcast_samples), &compensate[0], n_drone_id_samples);

  int8_t *qpsk_bits = (int8_t*) malloc(d_input_file_bit_count*sizeof(int8_t));
  int8_t *d1 = (int8_t*) malloc(d_turbo_decoder_bit_count*sizeof(int8_t));
  int8_t *d2 = (int8_t*) malloc(d_turbo_decoder_bit_count*sizeof(int8_t));
  int8_t *d3 = (int8_t*) malloc(d_turbo_decoder_bit_count*sizeof(int8_t));
  uint8_t *decoded_bytes = (uint8_t*) malloc(d_expected_payload_bytes*sizeof(uint8_t));
  struct lte_rate_matcher * rate_matcher = lte_rate_matcher_alloc();
  struct tdecoder * turbo_decoder = alloc_tdec();
  struct lte_rate_matcher_io rate_matcher_io = {
      .D = d_turbo_decoder_bit_count,
      .E = d_input_file_bit_count,
      .d = {d1, d2, d3},
      .e = qpsk_bits
  };

  //Decode the QPSK signal to bits
  decode_QPSK(broadcast_samples, qpsk_bits);
  // Descramble
  for(int i=0;i<d_input_file_bit_count;i++)
  {
      qpsk_bits[i] = (qpsk_bits[i] ^ d_goldSeq[i]) * 2 -1; 
  }

  // Setup the rate matching logic
  lte_rate_match_rv(rate_matcher, &rate_matcher_io, 0);
  // Run the turbo decoder (will do rate matching as well)
  int decode_status = lte_turbo_decode(turbo_decoder, d_expected_payload_bits, d_turbo_iterations,
      decoded_bytes, d1, d2, d3);

  if (decode_status != 0) {
      logger->error("signal_demodulation(): Turbo decoder failed");
  }

  // Validate the CRC24 at the end of the Turbo decoded data
  int calculated_crc = CRC::Calculate(decoded_bytes, d_expected_payload_bytes, CRC::CRC_24_LTEA());

  std::string fname_flag_ ;
  if (calculated_crc != 0) 
  { 
    fname_flag_ = "failed_";
    logger->info("signal_demodulation(): [{}] crc    failed [{}] [{}]", drone_id_fc, location, current_round); 
  }
  else
  {
    fname_flag_ = "correct_";
    std::stringstream stream;
    for (int idx = 0; idx < d_expected_payload_bytes; idx++) 
    { 
      stream << std::setfill ('0') << std::setw(sizeof(uint8_t)*2)<< std::hex << int(decoded_bytes[idx]);
    }
    string result = stream.str();
    logger->debug("signal_demodulation(): bytes [{}][{}][{}]", result, drone_id_fc,current_round);

    json j = initialize_message();
    j = fill_drone_id_bytes_to_message(result, j);
    j["drone_id_peak_fpga_cycles"] = fpga_cycles_peak_location;
    if (j.size() != 0)
    {
      send_webrequests(j.dump());
    }
    logger->trace("signal_demodulation(): [{}] {}",drone_id_fc, result);
    logger->info("signal_demodulation(): [{}] crc succeeded [{}] [{}]", drone_id_fc, location, current_round); 
    logger->debug("signal_demodulation(): found drone id, {}", j.dump());
  } 

  std::ofstream d_ofstream;
  d_time = std::time(0);
  d_time_tm = std::localtime(&d_time);
  d_ofstream.open("../samples/" + fname_flag_ +std::to_string(d_time_tm->tm_hour)+"_"+std::to_string(d_time_tm->tm_min)
    +"_"+std::to_string(d_time_tm->tm_sec)+"_"+std::to_string(drone_id_fc)+"_"+std::to_string(current_round), std::ios::trunc);
  d_ofstream.write((const char*)broadcast_samples_origin, sizeof(fftwf_complex)*N_BROADCAST_6144_LEN);
  d_ofstream.close(); 

  free(qpsk_bits);
  free(d1);
  free(d2);
  free(d3);
  free(decoded_bytes);
  free_tdec(turbo_decoder);
  lte_rate_matcher_free(rate_matcher);
}


void 
decoder::broadcast_signal_demodulation(std::vector<uint8_t> &bits,const std::vector<cxf_t> &samples)
{
  //double fpga_cycles_peak_location = location * (double)origin_samp_rate / (double)new_samp_rate + fpga_cycles_begin; 
  //int64_t fc_offset =  drone_id_fc - sampling_fc;
   
  //float SNR;

  //if(location < n_pre_samples && location > origin_len - n_post_samples)
  //{
  //  logger->debug("signal_demodulation(): peak idx close to the edge of the signal, skip, idx = {}", location);
  //  return;
  //}
  //int down_factor = int(origin_samp_rate / new_samp_rate);
  //cxf_t broadcast_samples_origin[N_BROADCAST_6144_LEN];
  cxf_t broadcast_samples[N_BROADCAST_1536_LEN];

  for (uint32_t i = 0; i < N_BROADCAST_1536_LEN; i++){
    broadcast_samples[i] = samples[i];
  }
  //memcpy(broadcast_samples_origin, s + (location - n_pre_samples) * 4, N_BROADCAST_6144_LEN*sizeof(cxf_t));
  // create to feed into the resample so that it does not need to recreate it again. in the end move to constructor of tdoa_coordinator
  //cxf_t *samples_F = (cxf_t*)fftwf_malloc(sizeof(cxf_t) * N_BROADCAST_6144_LEN);
  //resample_broadcast(&broadcast_samples_origin[0], &broadcast_samples[0], N_BROADCAST_6144_LEN, origin_samp_rate, new_samp_rate, fc_offset, samples_F);

  // Estimate and correct the fractional frequency offset(ffo)
  // Calculate the ffo of 8 OFDM symbols and get the mean value
  // ########## SNR estimation ########
  //SNR = OFDM_SNR_estimation(s, origin_samp_rate,new_samp_rate,location,origin_len,sampling_fc,drone_id_fc);
  float d_ffo;
  cxf_t d_ffo2 = {0.f, 0.f};
  for(int k = 0;k < 8; k++)
  {
    d_ffo2 += ffo_est(broadcast_samples + k * OFDM_SYMBOL_LEN_1536);
  }
  d_ffo  = std::atan2(d_ffo2.imag(),d_ffo2.real());
  
  cxf_t compensate[n_drone_id_samples];
  for(uint i = 0;i < n_drone_id_samples; i++)
  {
    cxf_t a;
    a.real(0);
    a.imag(-1*i * d_ffo / 1024);
    compensate[i] = std::exp(a);
  }
  volk_32fc_x2_multiply_conjugate_32fc(reinterpret_cast<cxf_t*>(broadcast_samples), reinterpret_cast<cxf_t*>(broadcast_samples), &compensate[0], n_drone_id_samples);

  int8_t *qpsk_bits = (int8_t*) malloc(d_input_file_bit_count*sizeof(int8_t));
  // int8_t *d1 = (int8_t*) malloc(d_turbo_decoder_bit_count*sizeof(int8_t));
  // int8_t *d2 = (int8_t*) malloc(d_turbo_decoder_bit_count*sizeof(int8_t));
  // int8_t *d3 = (int8_t*) malloc(d_turbo_decoder_bit_count*sizeof(int8_t));
  // uint8_t *decoded_bytes = (uint8_t*) malloc(d_expected_payload_bytes*sizeof(uint8_t));
  // struct lte_rate_matcher * rate_matcher = lte_rate_matcher_alloc();
  // struct tdecoder * turbo_decoder = alloc_tdec();
  // struct lte_rate_matcher_io rate_matcher_io = {
  //     .D = d_turbo_decoder_bit_count,
  //     .E = d_input_file_bit_count,
  //     .d = {d1, d2, d3},
  //     .e = qpsk_bits
  // };

  //Decode the QPSK signal to bits
  std::vector<cxf_t> syms;
  decode_QPSK((fftwf_complex*) broadcast_samples, qpsk_bits, syms);
  save_complex64("syms.fc32",syms);
  bits.resize(d_input_file_bit_count);
  for(int i=0; i < d_input_file_bit_count;i++) {
    bits[i] = qpsk_bits[i];
  }
  free(qpsk_bits);





  // Descramble
  // for(int i=0;i<d_input_file_bit_count;i++)
  // {
  //     qpsk_bits[i] = (qpsk_bits[i] ^ d_goldSeq[i]) * 2 -1; 
  // }

  // Setup the rate matching logic
  //lte_rate_match_rv(rate_matcher, &rate_matcher_io, 0);
  // Run the turbo decoder (will do rate matching as well)
  //int decode_status = lte_turbo_decode(turbo_decoder, d_expected_payload_bits, d_turbo_iterations,
  //    decoded_bytes, d1, d2, d3);

  //if (decode_status != 0) {
  //    logger->error("signal_demodulation(): Turbo decoder failed");
  //}

  // Validate the CRC24 at the end of the Turbo decoded data
  //int calculated_crc = CRC::Calculate(decoded_bytes, d_expected_payload_bytes, CRC::CRC_24_LTEA());

  // std::string fname_flag_ ;
  // if (calculated_crc != 0) 
  // { 
  //   fname_flag_ = "failed_";
  //   logger->info("signal_demodulation(): [{}] crc    failed [{}] [{}]", drone_id_fc, location, current_round); 
  // }
  // else
  // {
  //   fname_flag_ = "correct_";
  //   std::stringstream stream;
  //   for (int idx = 0; idx < d_expected_payload_bytes; idx++) 
  //   { 
  //     stream << std::setfill ('0') << std::setw(sizeof(uint8_t)*2)<< std::hex << int(decoded_bytes[idx]);
  //   }
  //   string result = stream.str();
  //   logger->debug("signal_demodulation(): bytes [{}][{}][{}]", result, drone_id_fc,current_round);

  //   json j = initialize_message();
  //   j = fill_drone_id_bytes_to_message(result, j);
  //   j["drone_id_peak_fpga_cycles"] = fpga_cycles_peak_location;
  //   if (j.size() != 0)
  //   {
  //     send_webrequests(j.dump());
  //   }
  //   logger->trace("signal_demodulation(): [{}] {}",drone_id_fc, result);
  //   logger->info("signal_demodulation(): [{}] crc succeeded [{}] [{}]", drone_id_fc, location, current_round); 
  //   logger->debug("signal_demodulation(): found drone id, {}", j.dump());
  // } 

  //std::ofstream d_ofstream;
  //d_time = std::time(0);
  //d_time_tm = std::localtime(&d_time);
  //d_ofstream.open("../samples/" + fname_flag_ +std::to_string(d_time_tm->tm_hour)+"_"+std::to_string(d_time_tm->tm_min)
  //  +"_"+std::to_string(d_time_tm->tm_sec)+"_"+std::to_string(drone_id_fc)+"_"+std::to_string(current_round), std::ios::trunc);
  //d_ofstream.write((const char*)broadcast_samples_origin, sizeof(cxf_t)*N_BROADCAST_6144_LEN);
  //d_ofstream.close(); 

  // free(qpsk_bits);
  // free(d1);
  // free(d2);
  // free(d3);
  // free(decoded_bytes);
  // free_tdec(turbo_decoder);
  // lte_rate_matcher_free(rate_matcher);
}
/*

void 
readcomplex64(const std::string filename, std::vector<cxf_t> &v){
    std::ifstream input(filename, std::ios::binary);

    auto start = std::istream_iterator<std::complex<float>>(input);
    auto stop = std::istream_iterator<std::complex<float>>();

    std::vector<std::complex<float>> cbuf(start, stop);
    v = cbuf;
    std::cout << "Number of raw samples read: " << cbuf.size();
}
*/

int 
read_complex64(const std::string filename, std::vector<cxf_t> &vec)
{
    FILE* fp;
    fp = fopen(filename.c_str(), "r");
    if (!fp) {return -1;} 
    fseek(fp, 0, SEEK_END);
    size_t num = ftell(fp) / sizeof(cxf_t);
    fseek(fp, 0, SEEK_SET);
    vec.resize(num);
    num = fread(vec.data(), sizeof(cxf_t), num, fp);
    fclose(fp);
    return num;
}

int
save_complex64(const std::string filename, const std::vector<cxf_t> &vec){
    FILE* fp;
    fp = fopen(filename.c_str(), "w");
    if (!fp) {return -1;} 
    int num = fwrite(vec.data(), sizeof(cxf_t), vec.size(), fp);
    fclose(fp);
    return num; 
}

int main(int argc, char** argv){
    std::vector<std::string> args;
    if (argc > 1) {
        args.assign(argv + 1, argv + argc);
    } else {
        std::cout << "Need an input bin file\n";
        return 0;
    }
    
    if (!std::filesystem::exists(args[0])) {
        std::cout << "File not found: " << args[0] << "\n";
        return 0;
    } else {
        std::cout << "Reading from: " << args[0] << "\n";       
    }

    std::vector<cxf_t> samples;
    int n = read_complex64(args[0], samples);
    std::cout << "Number of raw samples read: " << samples.size() << "\n";    
    std::cout << "Sample 0: " << samples[0] << "\n";

    std::vector<uint8_t> bits;
    decoder d = decoder();
    d.broadcast_signal_demodulation(bits, samples);
    std::cout << "Raw number of bits: " << bits.size() << "\n"; 
    for (int i = 0; i < 17; ++i) {
        std::cout << static_cast<int>(bits[i]) << " ";
    }
    std::cout << "\n";



    

    return 0;
    /*
    constexpr int N = 2000;
    std::vector<cxf_t> samples(N);

    for (int i = 0; i < N; ++i) {
        samples[i] = ((float) i) / N + ((float) (N - i) ) / N * 1if;
    }


    std::cout << d.ffo_est(samples.data()) << std::endl;

    constexpr int M=7;
    fftwf_complex arr[M] = {0,0,1,1,2,2,3,3,4,4,5,5,6,6};//7,7};
    for (int i = 0; i < M; i++){
        std::cout << arr[i][0] << " " << arr[i][1] << "i  ";
    }
    std::cout << std::endl; 
    d.fftwf_fftshift(arr, M);
    for (int i = 0; i < M; i++){
        std::cout << arr[i][0] << " " << arr[i][1] << "i  ";
    }
    std::cout << std::endl; 


    std::vector<std::string> files;
    for (const auto &entry: std::filesystem::directory_iterator(args[0])) {
        files.push_back(entry.path());
    }

    for (const auto &f: files) {
        std::cout << f << std::endl;
    }
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