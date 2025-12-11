/*
배선도
Lepton Thermal Camera Module    Raspberry Pi 4B
        VCC  ----------------->   3.3V
        GND  ----------------->   GND
        SCK  ----------------->   SPI0 SCLK (GPIO 11)
        MISO ----------------->   SPI0 MISO (GPIO 09)
        MOSI ----------------->   SPI0 MOSI (GPIO 10)
        CS   ----------------->   SPI0 CE0 (GPIO 08)
        SDA  ----------------->   I2C SDA0 (GPIO 00)
        SCL  ----------------->   I2C SCL0 (GPIO 01)
        PWR_DWN_L ------------>   GPIO 21
        RESET_L -------------->   GPIO 20
        MASTER_CLK ----------->   GPCLK0 (GPIO 04)
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <limits.h>
#include <string.h>
#include <sys/select.h>
#include <pigpio.h>

#define GPIO_CHIP "/dev/gpiochip0"
#define PWR_DWN_L   21      // gpio 핀 설정
#define RESET_L     20
#define PWM1_PIN    13
#define MASTER_CLK  4
#define MASTER_CLK_FREQ 25000000  // 25MHz


#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static void pabort(const char *s)
{
	perror(s);
	abort();
}

static const char *device = "/dev/spidev0.0";
static uint8_t mode = SPI_MODE_3;
static uint8_t bits = 8;
static uint32_t speed = 10000000;   // 10MHz
static uint16_t delay = 0;

#define VOSPI_FRAME_SIZE (164)
uint8_t lepton_frame_packet[VOSPI_FRAME_SIZE];
static unsigned int lepton_image[80][80];

int transfer(int fd)
{
    int ret;
    int i;
    uint8_t frame_number = 0;
    uint8_t tx[VOSPI_FRAME_SIZE] = {0, };
    struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)lepton_frame_packet,
		.len = VOSPI_FRAME_SIZE,
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
	};

    ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if(ret < 1)
        pabort("can't send spi message");

    // printf(" --- 프레임 0,1: %d %d \n", lepton_frame_packet[0], lepton_frame_packet[1]);
    if(((lepton_frame_packet[0] & 0x0f) != 0x0f))
    {
        frame_number = lepton_frame_packet[1];

        if(frame_number < 60)
        {
            for(i=0;i<80;i++)
            {
                lepton_image[frame_number][i] = (lepton_frame_packet[2*i+4] << 8 | lepton_frame_packet[2*i+5]);
            }
        }
    }

    // 받아온 프레임 찍어보자...
    printf("ID [%d] 프레임 상위 5 bytes -- ", frame_number);
    for (i=0;i<10;i++){
        printf("%02X ", lepton_frame_packet[i]);
        if ((i+1)%2==0)
            printf("/ ");
    }
    printf("\n");

    return frame_number;
}


static void save_pgm_file(void)
{
	int i;
	int j;
	unsigned int maxval = 0;
	unsigned int minval = UINT_MAX;
	char image_name[32];
	int image_index = 0;

	do {
		sprintf(image_name, "IMG_%.4d.pgm", image_index);
		image_index += 1;
		if (image_index > 9999) 
		{
			image_index = 0;
			break;
		}

	} while (access(image_name, F_OK) == 0);

	FILE *f = fopen(image_name, "w");
	if (f == NULL)
	{
		printf("Error opening file!\n");
		exit(1);
	}

	printf("Calculating min/max values for proper scaling...\n");
	for(i=0;i<60;i++)
	{
		for(j=0;j<80;j++)
		{
			if (lepton_image[i][j] > maxval) {
				maxval = lepton_image[i][j];
			}
			if (lepton_image[i][j] < minval) {
				minval = lepton_image[i][j];
			}
		}
	}
	printf("maxval = %u\n",maxval);
	printf("minval = %u\n",minval);
	
	fprintf(f,"P2\n80 60\n%u\n",maxval-minval);
	for(i=0;i<60;i++)
	{
		for(j=0;j<80;j++)
		{
			fprintf(f,"%d ", lepton_image[i][j] - minval);
		}
		fprintf(f,"\n");
	}
	fprintf(f,"\n\n");

	fclose(f);
}


int main(int argc, char *argv[]){
    printf("=== 로직 아날라이저로 SPI 신호 확인용 테스트 프로그램 ===\n");
    int ret = 0;
    int fd;
    int i;

    // // Lepton Thermal Camera Start-up
    // if(lepton_startup() != 0){
    //     printf("Somethings wrong in Lepton startup...\n");
    //     return -1;
    // }

    // datasheet 4.2.2.3.1) Establishing Sync
    printf("=== Lepton SPI Synchronization Procedure ===\n");

    fd = open(device, O_RDWR);
	if (fd < 0)
	{
		pabort("can't open device");
	}

	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
	{
		pabort("can't set spi mode");
	}

	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
	{
		pabort("can't get spi mode");
	}

	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
	{
		pabort("can't set bits per word");
	}

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
	{
		pabort("can't get bits per word");
	}

	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
	{
		pabort("can't set max speed hz");
	}

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
	{
		pabort("can't get max speed hz");
	}

    printf("SPI 동기화 시작...\n");
	printf("spi mode: %d\n", mode);
	printf("bits per word: %d\n", bits);
	printf("max speed: %d Hz (%d MHz)\n", speed, speed/1000000);
    
    // SYNCHRONIZATION PROCEDURE
    //1. Deassert /CS and idle SCK for at least 185ms(5 frame periods)
    usleep(300000);
    
    //2. Assert /CS and enable SCLK. This action causes the Lepton to start trasmission of a first packet.
    //3. Examine the ID field of the packet, identifying a discard packet.
    while(transfer(fd) != 59){}

    //4. Continue reading packets.


    save_pgm_file();
    
    // lepton_cleanup();
    close(fd);

    return 0;
}