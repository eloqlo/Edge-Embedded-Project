/*
<설명>
    Lepton Thermal Camera 이미지를 수신하는 코드.

<배선>
    Lepton Thermal Camera Module    Raspberry Pi 4B
            VCC  ----------------->   3.3V
            GND  ----------------->   GND
            SCK  ----------------->   SPI0 SCLK (GPIO 11)
            MISO ----------------->   SPI0 MISO (GPIO 09)
            MOSI ----------------->   SPI0 MOSI (GPIO 10)
            CS   ----------------->   SPI0 CE0 (GPIO 08)
            SDA  ----------------->   I2C SDA0 (GPIO 00)
            SCL  ----------------->   I2C SCL0 (GPIO 01)
*/
#include "lepton.h"

static const char *device = "/dev/spidev0.0";
static uint8_t mode = SPI_MODE_3;
static uint8_t bits = 8;
static uint32_t speed = 10000000;   // 10MHz
static uint16_t delay = 0;

#define VOSPI_FRAME_SIZE (164)
#define MAX_LOOP_COUNT (1000000000)

static uint16_t image[60][80];

int init_lepton(void)
{
    int fd;
    int ret = 0;

    fd = open(device, O_RDWR);
	if (fd < 0)
	{
		perror("open(): device를 열 수 없습니다");
		return fd;
	}

	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
	{
		perror("SPI 모드 설정 요류");
		return -1;
	}

	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
	{
		perror("BITS_PER_WORD 설정 오류");
		return -1;
	}

	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
	{
		perror("SPI_IOC_WR_MAX_SPEED_HZ 설정 오류");
		return -1;
	}

    // Lepton 2.5 동기화: Deassert /CS and idle SCK for at least 185ms(5 frame periods)
    usleep(300000);	  // >185ms

    return fd;
}

// get_image() 에 종속
static int get_VoSPI_packet(int fd, uint8_t *rx)
{
    int ret;
    int i;
    uint8_t dummy_tx[VOSPI_FRAME_SIZE] = {0, };
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)dummy_tx,
		.rx_buf = (unsigned long)rx,
		.len = VOSPI_FRAME_SIZE,
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
	};
    ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if(ret < 1){
        perror("Error while ioctl SPI communication");
		return -1;
	}
    return 1;
}

// get_image() 에 종속
static int packet_crc(uint8_t *rx)
{
    // TODO 패킷 CRC 검사 polynomial: x^16 + x^12 + x^5 + x^0
    return 1;
}

// image 배열에 이미지 수신받아 저장
// visualize_img(), save_img(), lepton_stream() 에 종속
static int get_image(int fd)
{
    int ret = 0;
    uint8_t frame_number = 0;
    unsigned int loop_count = 0;
    do {
        loop_count++;
        uint8_t rx[VOSPI_FRAME_SIZE] = {0, };
        ret = get_VoSPI_packet(fd, rx);
        if (ret < 0)
        {
            printf("Error while ioctl SPI communication\n");
            return -1;
        }

        if(((rx[0] & 0x0f) != 0x0f) && (packet_crc(rx) > 0))
        {
            frame_number = rx[1];
            if(frame_number < 60)
            {
                for(int i=0;i<80;i++)
                {
                    image[frame_number][i] = (rx[2*i+4] << 8 | rx[2*i+5]);
                }
            }
            else
            {
                printf("잘못된 프레임 ID: %d\n", frame_number);
            }
        }
    } while((frame_number != 59) && (loop_count < MAX_LOOP_COUNT));

    if(loop_count >= MAX_LOOP_COUNT){
        printf("이미지 수신 타임아웃\n");
        return -1;
    }
    else{
        return 1;
    }
}

//(1) 이미지 시각화
int visualize_img(int fd){
    int ret = 0;

    ret = get_image(fd);
    if (ret < 0)
    {
        printf("이미지 수신 불가\n");
        return -1;
    }
    
}

//(2) 이미지 저장 함수
int save_img(int fd){
    int ret = 0;

    ret = get_image(fd);
    if (ret < 0)
    {
        printf("이미지 수신 불가\n");
        return -1;
    }

}

//(3) raspberry pi에서 영상 스트리밍 함수
int lepton_stream(int fd){
    int ret = 0;

    ret = get_image(fd);
    if (ret < 0)
    {
        printf("이미지 수신 불가\n");
        return -1;
    }
}
