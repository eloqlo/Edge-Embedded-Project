#include <stdio.h>          // printf(), perror()
#include <stdint.h>         // uint8_t, uint16_t, uint32_t
#include <fcntl.h>          // open(), O_RDWR
#include <unistd.h>         // close(), usleep()z
#include <sys/ioctl.h>      // ioctl()
#include <linux/spi/spidev.h>  // SPI_MODE_3, SPI_IOC_*, struct spi_ioc_transfer
#include <string.h>

#include "../include/lepton.h"

static const char *device = "/dev/spidev0.0";
static uint8_t mode = SPI_MODE_3;
static uint8_t bits = 8;
static uint32_t speed = 10000000;   // 10MHz
static uint16_t delay = 0;


uint16_t image[LEPTON_HEIGHT][LEPTON_WIDTH + DEBUG_ID_CRC];

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

int cleanup_lepton(int fd){
    return (close(fd) == 0) ? 1 : -1;
}
static int _get_VoSPI_packet(int fd, uint8_t *rx)
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

static int _packet_crc(uint8_t *rx)
{
    // TODO 패킷 CRC 검사 polynomial: x^16 + x^12 + x^5 + x^0
    return 1;
}

//FIXME Thread가 여기서 못 나오고있다.
int lepton_capture(int fd)
{
    int ret = 0;
    uint8_t frame_number = 0;
    unsigned int loop_count = 0;
    do {
        loop_count++;
        uint8_t rx[VOSPI_FRAME_SIZE] = {0, };
        ret = _get_VoSPI_packet(fd, rx);
        if (ret < 0)
        {
            printf("Error while ioctl SPI communication\n");
            return -1;
        }

        if(((rx[0] & 0x0f) != 0x0f) && (_packet_crc(rx) > 0))
        {
            frame_number = rx[1];
            //DEBUG
            printf("%04x, %04x\n",rx[0], rx[1]);    
            if(frame_number < LEPTON_HEIGHT)
            {
                for(int i=0;i<LEPTON_WIDTH + DEBUG_ID_CRC;i++)
                {
                    if (DEBUG_ID_CRC)
                    {
                        image[frame_number][i] = (rx[2*i] << 8 | rx[2*i+1]);
                    }
                    else
                    {
                        image[frame_number][i] = (rx[2*i+4] << 8 | rx[2*i+5]);
                    }
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

void get_image(uint16_t (*cpy_image)[LEPTON_WIDTH])
{
    for (int r=0; r<LEPTON_HEIGHT; r++){
        memcpy(cpy_image[r], &image[r][DEBUG_ID_CRC], sizeof(uint16_t)*LEPTON_WIDTH);
    }
}


// ------------------ DEBUG 함수 ------------------ //
void print_image(int fd)
{
    printf("-- ID들 잘 들어왔나 확인 -- \n");
    for (int r=0; r<60; r++){
        printf("%02X ", image[r][0]);
    }
    printf("\n");
}
