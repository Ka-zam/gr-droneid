#include <vector>
#include <iostream>
#include <inttypes.h>
#include <filesystem>
#include <complex>

using cxf_t = cxf_t;

/*
g++ -o decoder -std=c++17 -O2 decoder.cpp
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

constexpr uint32_t SHORT_CP_1536 = 72;
constexpr uint32_t LONG_CP_1536 = 80;

float
fractional_frequency_offset_estimation(cxf_t *broadcast_samples)
{
  cxf_t tmp;
  float ffo = 0;
  cxf_t d_multiplied_samples[SHORT_CP_1536];
  volk_32fc_x2_multiply_conjugate_32fc(reinterpret_cast<cxf_t*>(&d_multiplied_samples[0]), reinterpret_cast<cxf_t*>(broadcast_samples), reinterpret_cast<cxf_t*>(broadcast_samples + OFDM_DATA_LEN_1536), SHORT_CP_1536);
  for(int i = 0;i < SHORT_CP_1536; i++)
  {
    tmp.real(tmp.real() + d_multiplied_samples[i][0]);
    tmp.imag(tmp.imag() + d_multiplied_samples[i][1]);
  }
  ffo = std::atan2(tmp.imag(),tmp.real());
  return ffo;
}


void Tdoa_Coordinator::decode_QPSK(cxf_t *broadcast_samples, int8_t *qpsk_bits)
{
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
      fftwf_execute_dft(plan_decode_fwd, broadcast_samples + i * 1096 + 72, &d_fft_samples[0]);
      fftwf_fftshift(d_fft_samples, OFDM_DATA_LEN_1536);
    }
    else
    {
      fftwf_execute_dft(plan_decode_fwd, broadcast_samples + i * 1096 + 80, &d_fft_samples[0]);
      fftwf_fftshift(d_fft_samples, OFDM_DATA_LEN_1536);
    }
    if(i <= 2)
    {
      for(int k = 0; k < N_ZC_1536; k++)
      {
        if (k == 300) continue;
        cxf_t temp;
        temp.real(d_fft_samples[k + 212][0]);
        temp.imag(d_fft_samples[k + 212][1]);
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
      }
    }
    else if(i==3)
    {
      for(int k = 0; k < N_ZC_1536; k++)
      {
        if (k == 300) continue;
        cxf_t temp;
        temp.real(d_fft_samples[k + 212][0]);
        temp.imag(d_fft_samples[k + 212][1]);
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
      }
    }
    else
    {
      for(int k =0;k < N_ZC_1536;k++)
      {
        if (k == 300) continue;
        cxf_t temp;
        temp.real(d_fft_samples[k + 212][0]);
        temp.imag(d_fft_samples[k + 212][1]);
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
      }
    }
  }
}

void Dji_Incision::broadcast_signal_demodulation(cxf_t *s, int64_t origin_samp_rate, int64_t new_samp_rate, int64_t location, int64_t origin_len, int64_t sampling_fc, int64_t drone_id_fc, int16_t current_round, double fpga_cycles_begin)
{
  double fpga_cycles_peak_location = location * (double)origin_samp_rate / (double)new_samp_rate + fpga_cycles_begin; 
  int64_t fc_offset =  drone_id_fc - sampling_fc;
   
  float SNR;
  if(location < n_pre_samples && location > origin_len - n_post_samples)
  {
    logger->debug("signal_demodulation(): peak idx close to the edge of the signal, skip, idx = {}", location);
    return;
  }
  int down_factor = int(origin_samp_rate / new_samp_rate);
  cxf_t broadcast_samples_origin[N_BROADCAST_6144_LEN];
  cxf_t broadcast_samples[N_BROADCAST_1536_LEN];
  memcpy(broadcast_samples_origin, s + (location - n_pre_samples) * 4, N_BROADCAST_6144_LEN*sizeof(cxf_t));
  // create to feed into the resample so that it does not need to recreate it again. in the end move to constructor of tdoa_coordinator
  cxf_t *samples_F = (cxf_t*)fftwf_malloc(sizeof(cxf_t) * N_BROADCAST_6144_LEN);
  resample_broadcast(&broadcast_samples_origin[0], &broadcast_samples[0], N_BROADCAST_6144_LEN, origin_samp_rate, new_samp_rate, fc_offset, samples_F);

  // Estimate and correct the fractional frequency offset(ffo)
  // Calculate the ffo of 8 OFDM symbols and get the mean value
  // ########## SNR estimation ########
  //SNR = OFDM_SNR_estimation(s, origin_samp_rate,new_samp_rate,location,origin_len,sampling_fc,drone_id_fc);
  float d_ffo;
  cxf_t d_ffo2 = {0.f, 0.f};
  for(int k = 0;k < 8; k++)
  {
    d_ffo2 += fractional_frequency_offset_estimation(broadcast_samples + k * OFDM_SYMBOL_LEN_1536);
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
  d_ofstream.write((const char*)broadcast_samples_origin, sizeof(cxf_t)*N_BROADCAST_6144_LEN);
  d_ofstream.close(); 

  free(qpsk_bits);
  free(d1);
  free(d2);
  free(d3);
  free(decoded_bytes);
  free_tdec(turbo_decoder);
  lte_rate_matcher_free(rate_matcher);
}

int main(int argc, char** argv){
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
	/*
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