/*

Install:
gcc -g -shared -o libdt.so -O2 -march=native -fPIC libdt.c -I/usr/local/include -L/usr/local/lib -lturbofec
sudo cp libdt.so /usr/local/lib
sudo ldconfig

*/

#include <stdint.h>
//#include <stdio.h>
#include <stdlib.h> // malloc
#include <string.h> // memset

#include <turbofec/rate_match.h>
#include <turbofec/turbo.h>

// Forward constants
#define DATA_LEN      (176)
#define DJI_LEN       (91)
#define SCRAP_LEN     (82)
#define TURBO_IN_LEN  ((DATA_LEN * 8) + 4)
#define TURBO_OUT_LEN (900 * 8)

// Reverse constants
#define REV_TURBO_ITERATIONS   (4)
#define REV_TURBO_BIT_CNT      (1412)
#define REV_PAYLOAD_BYTE_CNT   (176)
#define REV_PAYLOAD_BIT_CNT    (REV_PAYLOAD_BYTE_CNT * 8)

uint8_t* d1;
uint8_t* d2;
uint8_t* d3;
uint8_t* data;
uint8_t* up;
uint8_t* msg;
int8_t* soft_msg;
struct lte_rate_matcher* dt_rate_matcher;
struct tdecoder* dt_tdecoder;

static const struct lte_turbo_code dt_fwd_settings = {
    .n = 2,
    .k = 4,
    .len = DATA_LEN * 8,
    .rgen = 11,
    .gen = 13,
};

static const uint32_t table[256] = {
    0x000000U,0x864CFBU,0x8AD50DU,0x0C99F6U,0x93E6E1U,0x15AA1AU,0x1933ECU,0x9F7F17U,
    0xA18139U,0x27CDC2U,0x2B5434U,0xAD18CFU,0x3267D8U,0xB42B23U,0xB8B2D5U,0x3EFE2EU,
    0xC54E89U,0x430272U,0x4F9B84U,0xC9D77FU,0x56A868U,0xD0E493U,0xDC7D65U,0x5A319EU,
    0x64CFB0U,0xE2834BU,0xEE1ABDU,0x685646U,0xF72951U,0x7165AAU,0x7DFC5CU,0xFBB0A7U,
    0x0CD1E9U,0x8A9D12U,0x8604E4U,0x00481FU,0x9F3708U,0x197BF3U,0x15E205U,0x93AEFEU,
    0xAD50D0U,0x2B1C2BU,0x2785DDU,0xA1C926U,0x3EB631U,0xB8FACAU,0xB4633CU,0x322FC7U,
    0xC99F60U,0x4FD39BU,0x434A6DU,0xC50696U,0x5A7981U,0xDC357AU,0xD0AC8CU,0x56E077U,
    0x681E59U,0xEE52A2U,0xE2CB54U,0x6487AFU,0xFBF8B8U,0x7DB443U,0x712DB5U,0xF7614EU,
    0x19A3D2U,0x9FEF29U,0x9376DFU,0x153A24U,0x8A4533U,0x0C09C8U,0x00903EU,0x86DCC5U,
    0xB822EBU,0x3E6E10U,0x32F7E6U,0xB4BB1DU,0x2BC40AU,0xAD88F1U,0xA11107U,0x275DFCU,
    0xDCED5BU,0x5AA1A0U,0x563856U,0xD074ADU,0x4F0BBAU,0xC94741U,0xC5DEB7U,0x43924CU,
    0x7D6C62U,0xFB2099U,0xF7B96FU,0x71F594U,0xEE8A83U,0x68C678U,0x645F8EU,0xE21375U,
    0x15723BU,0x933EC0U,0x9FA736U,0x19EBCDU,0x8694DAU,0x00D821U,0x0C41D7U,0x8A0D2CU,
    0xB4F302U,0x32BFF9U,0x3E260FU,0xB86AF4U,0x2715E3U,0xA15918U,0xADC0EEU,0x2B8C15U,
    0xD03CB2U,0x567049U,0x5AE9BFU,0xDCA544U,0x43DA53U,0xC596A8U,0xC90F5EU,0x4F43A5U,
    0x71BD8BU,0xF7F170U,0xFB6886U,0x7D247DU,0xE25B6AU,0x641791U,0x688E67U,0xEEC29CU,
    0x3347A4U,0xB50B5FU,0xB992A9U,0x3FDE52U,0xA0A145U,0x26EDBEU,0x2A7448U,0xAC38B3U,
    0x92C69DU,0x148A66U,0x181390U,0x9E5F6BU,0x01207CU,0x876C87U,0x8BF571U,0x0DB98AU,
    0xF6092DU,0x7045D6U,0x7CDC20U,0xFA90DBU,0x65EFCCU,0xE3A337U,0xEF3AC1U,0x69763AU,
    0x578814U,0xD1C4EFU,0xDD5D19U,0x5B11E2U,0xC46EF5U,0x42220EU,0x4EBBF8U,0xC8F703U,
    0x3F964DU,0xB9DAB6U,0xB54340U,0x330FBBU,0xAC70ACU,0x2A3C57U,0x26A5A1U,0xA0E95AU,
    0x9E1774U,0x185B8FU,0x14C279U,0x928E82U,0x0DF195U,0x8BBD6EU,0x872498U,0x016863U,
    0xFAD8C4U,0x7C943FU,0x700DC9U,0xF64132U,0x693E25U,0xEF72DEU,0xE3EB28U,0x65A7D3U,
    0x5B59FDU,0xDD1506U,0xD18CF0U,0x57C00BU,0xC8BF1CU,0x4EF3E7U,0x426A11U,0xC426EAU,
    0x2AE476U,0xACA88DU,0xA0317BU,0x267D80U,0xB90297U,0x3F4E6CU,0x33D79AU,0xB59B61U,
    0x8B654FU,0x0D29B4U,0x01B042U,0x87FCB9U,0x1883AEU,0x9ECF55U,0x9256A3U,0x141A58U,
    0xEFAAFFU,0x69E604U,0x657FF2U,0xE33309U,0x7C4C1EU,0xFA00E5U,0xF69913U,0x70D5E8U,
    0x4E2BC6U,0xC8673DU,0xC4FECBU,0x42B230U,0xDDCD27U,0x5B81DCU,0x57182AU,0xD154D1U,
    0x26359FU,0xA07964U,0xACE092U,0x2AAC69U,0xB5D37EU,0x339F85U,0x3F0673U,0xB94A88U,
    0x87B4A6U,0x01F85DU,0x0D61ABU,0x8B2D50U,0x145247U,0x921EBCU,0x9E874AU,0x18CBB1U,
    0xE37B16U,0x6537EDU,0x69AE1BU,0xEFE2E0U,0x709DF7U,0xF6D10CU,0xFA48FAU,0x7C0401U,
    0x42FA2FU,0xC4B6D4U,0xC82F22U,0x4E63D9U,0xD11CCEU,0x575035U,0x5BC9C3U,0xDD8538U,
};

uint32_t 
crc(uint8_t *data, uint32_t bytes) {
	uint32_t n = bytes;
    uint32_t crc = 0x00000000U;
    while (n > 0) {
        crc = table[(*data++) ^ (uint8_t) (crc >> 16)] ^ (crc << 8);
        n--;
    }
    return crc & 0x00FFFFFFU;
}

void 
dt_allocate_fwd(){
	d1 = (uint8_t*) malloc( TURBO_IN_LEN );
	d2 = (uint8_t*) malloc( TURBO_IN_LEN );
	d3 = (uint8_t*) malloc( TURBO_IN_LEN );	
	data = (uint8_t*) malloc( DATA_LEN );
	up = (uint8_t*) malloc( DATA_LEN * 8 );
	msg = (uint8_t*) malloc( DJI_LEN );
    dt_rate_matcher = lte_rate_matcher_alloc();
}

void 
dt_free_fwd() {
	free(d1);
	free(d2);
	free(d3);
	free(data);
	free(msg);
	free(up);
    lte_rate_matcher_free(dt_rate_matcher);
}

uint32_t
dt_turbo_fwd(uint8_t* out, uint8_t* msg) {
	//  out must be able to hold 7200 bytes
	//  in must be of length 176
	//
	dt_allocate_fwd();
	// data init
	//memset(data, 0x00, DATA_LEN);
	memcpy(data, msg, DATA_LEN);

    // Add CRC to payload
	uint32_t res = crc(data, DJI_LEN + SCRAP_LEN);
	data[DATA_LEN - 3] = (res >> 16) & 0xff;
	data[DATA_LEN - 2] = (res >>  8) & 0xff;
	data[DATA_LEN - 1] = (res >>  0) & 0xff;

    // Unpack data into bytes
    uint32_t idx = 0;
    for (uint32_t i = 0; i < DATA_LEN; ++i) {
    	for (int32_t s = 7; s >= 0; s--) {
    		up[idx] = (data[i] >> s) & 0x01;
    		idx++;
    	}
    }

    // Setup IO sizes and buffers
    struct lte_rate_matcher_io rm_io = {
        .D = TURBO_IN_LEN,
        .E = TURBO_OUT_LEN,
        .d = {d1, d2, d3},
        .e = out,
    };

    lte_turbo_encode(&dt_fwd_settings, up, d1, d2, d3);
    lte_rate_match_fw(dt_rate_matcher, &rm_io, 0);
    dt_free_fwd();
    return res;
}

void 
dt_allocate_rev(){
	d1 = (uint8_t*) malloc( REV_TURBO_BIT_CNT );
	d2 = (uint8_t*) malloc( REV_TURBO_BIT_CNT );
	d3 = (uint8_t*) malloc( REV_TURBO_BIT_CNT );	
	data = (uint8_t*) malloc( REV_PAYLOAD_BYTE_CNT );
	//up = (uint8_t*) malloc( DATA_LEN * 8 * sizeof(uint8_t) );
	//msg = (uint8_t*) malloc( DJI_LEN * sizeof(uint8_t) );
    dt_rate_matcher = lte_rate_matcher_alloc();
    dt_tdecoder = alloc_tdec();
}

void 
dt_free_rev() {
	free(d1);
	free(d2);
	free(d3);
	free(data);
	//free(msg);
	//free(up);
    lte_rate_matcher_free(dt_rate_matcher);
    free_tdec(dt_tdecoder);
}

uint64_t
dt_turbo_rev(uint8_t* out, uint8_t* msg) {
	//  out must be able to hold 176 bytes
	//  in must be of length 7200
	//
	uint64_t res = 0x00;
	dt_allocate_rev();

	int8_t* soft_msg = (int8_t*) malloc(TURBO_OUT_LEN);
	for (int i = 0; i < TURBO_OUT_LEN; ++i)	{
		soft_msg[i] = ((*msg++) == 0x00) ? -63 : 63;
	}

    // Setup IO sizes and buffers
    struct lte_rate_matcher_io rm_io = {
        .D = REV_TURBO_BIT_CNT,
        .E = TURBO_OUT_LEN,
        .d = {d1, d2, d3},
        .e = soft_msg,
    };

    lte_rate_match_rv(dt_rate_matcher, &rm_io, 0);
    int status = lte_turbo_decode(dt_tdecoder, 
    	REV_PAYLOAD_BIT_CNT, 
    	REV_TURBO_ITERATIONS,
        out, d1, d2, d3);

  	uint32_t crc_res = crc(out, REV_PAYLOAD_BYTE_CNT);
    
    res = ((status & 0x00000000FFFFFFFFUL) << 32) | (crc_res & 0x00000000FFFFFFFFUL);
    free(soft_msg);
    dt_free_rev();
    return res;
}

/*
int main() {
	dt_allocate();

	//Read the 91 bytes from 'frame.bin'
	uint8_t* msg = (uint8_t*) malloc(DJI_LEN);
	FILE* fid = fopen("frame.bin", "rb");
	if (!fid) {
		printf("%s\n", "Could not open file");
		return 0;
	}
	fread(msg, sizeof(uint8_t), DJI_LEN, fid);
	fclose(fid);

    uint8_t* outbuf = (uint8_t*) malloc(TURBO_OUT_LEN);
    uint32_t msg_crc = dt_turbo_fwd(outbuf, msg);

    fid = fopen("frame.pyfec.bin","wb");
    int b = fwrite(outbuf, sizeof(uint8_t), TURBO_OUT_LEN, fid);
    fclose(fid);
    printf("CRC: %02x\nWrote %d bytes\n", msg_crc, b);
    dt_free();
   	return 0;
}
*/