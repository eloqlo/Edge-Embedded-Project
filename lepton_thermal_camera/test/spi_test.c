// lepton_vospi_frame.c
// Reads full 60x80 Raw14 frames from a FLIR Lepton 2.5 over VoSPI.
// Validates sync, skips discard packets, reconstructs each line, and prints
// summary statistics (min/max + first 10 pixels of line 0).
//
// Tested on Raspberry Pi + Lepton Breakout Board v2
// Requires SPI enabled: `raspi-config -> Interface Options -> SPI`

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#define SPI_DEV             "/dev/spidev0.0"

#define FRAME_WIDTH         80
#define FRAME_HEIGHT        60

#define VOSPI_PACKET_SIZE   164    // 4-byte header + 160-byte payload
#define VOSPI_HEADER_SIZE   4
#define VOSPI_PAYLOAD_SIZE  160
#define VOSPI_PACKETS_PER_FRAME 60

// A discard packet has its upper 4 bits set to 0xF (i.e., 0xFxxx)
#define IS_DISCARD_PACKET(id) (((id) & 0xF000) == 0xF000)
// Lower 12 bits represent the line number (0-59)
#define PACKET_LINE_NUMBER(id) ((id) & 0x0FFF)
// Read exactly `len` bytes from `fd` into `buf`
// This loops until all requested bytes are received.
static int read_exact(int fd, uint8_t *buf, size_t len)
{
    size_t total = 0;
    while (total < len) {
        ssize_t r = read(fd, buf + total, len - total);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (r == 0) {
            // Unexpected end of data (should not happen with Lepton)
            return -1;
        }
        total += (size_t)r;
    }
    return 0;
}

// Synchronize to the first valid video packet (line 0).
// This skips discard packets and any partial frame.
static int vospi_sync_to_first_line(int fd, uint8_t *packet_buf)
{
    while (1) {
        if (read_exact(fd, packet_buf, VOSPI_PACKET_SIZE) < 0) {
            perror("read (sync)");
            return -1;
        }

        uint16_t id = (packet_buf[0] << 8) | packet_buf[1];

        if (IS_DISCARD_PACKET(id)) {
            // Still waiting for the next frame boundary
            continue;
        }

        uint16_t line = PACKET_LINE_NUMBER(id);

        if (line == 0) {
            // Found the first line of a new frame
            return 0;
        }

        // If not discard and not line 0, we're mid-frame �� keep searching
    }
}

// Read a full 60x80 Lepton frame using VoSPI.
// Returns 0 on success, -1 on sync error.
static int vospi_read_frame(int fd, uint16_t frame[FRAME_HEIGHT][FRAME_WIDTH])
{
    uint8_t packet[VOSPI_PACKET_SIZE];

    // 1) Sync to line 0
    if (vospi_sync_to_first_line(fd, packet) < 0) {
        fprintf(stderr, "Failed to sync to first line\n");
        return -1;
    }

    // 2) Process packets for lines 0?59
    for (int expected_line = 0; expected_line < FRAME_HEIGHT; ++expected_line) {

        if (expected_line != 0) {
            if (read_exact(fd, packet, VOSPI_PACKET_SIZE) < 0) {
                perror("read (frame)");
                return -1;
            }
        }

        uint16_t id = (packet[0] << 8) | packet[1];

        if (IS_DISCARD_PACKET(id)) {
            // A discard packet inside a frame means sync was lost
            fprintf(stderr, "Discard packet during frame (expected line %d), resync\n",
                    expected_line);
            return -1;
        }

        uint16_t line = PACKET_LINE_NUMBER(id);

        if (line >= FRAME_HEIGHT) {
            fprintf(stderr, "Invalid line number %u, resync\n", line);
            return -1;
        }

        // Payload holds 80 Raw14 pixels (160 bytes, big-endian)
        uint8_t *payload = packet + VOSPI_HEADER_SIZE;

        for (int x = 0; x < FRAME_WIDTH; ++x) {
            uint8_t hi = payload[2 * x];
            uint8_t lo = payload[2 * x + 1];
            uint16_t value = ((uint16_t)hi << 8) | lo;
            frame[line][x] = value;
        }
    }

    return 0;
}

int main(void)
{
    int fd = -1;
    uint8_t mode = SPI_MODE_3;       // Lepton requires SPI Mode 3 (CPOL=1, CPHA=1)
    uint8_t bits = 8;
    uint32_t speed = 10000000;       // 10 MHz is a safe starting value
    uint16_t frame[FRAME_HEIGHT][FRAME_WIDTH];
    int frame_count = 0;

    fd = open(SPI_DEV, O_RDONLY);
    if (fd < 0) {
        perror("open SPI_DEV");
        return 1;
    }

    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0) {
        perror("SPI_IOC_WR_MODE");
        close(fd);
        return 1;
    }

    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        perror("SPI_IOC_WR_BITS_PER_WORD");
        close(fd);
        return 1;
    }

    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        perror("SPI_IOC_WR_MAX_SPEED_HZ");
        close(fd);
        return 1;
    }

    printf("SPI opened: %s, mode=%u, bits=%u, speed=%u Hz\n",
           SPI_DEV, mode, bits, speed);

    while (1) {
        if (vospi_read_frame(fd, frame) < 0) {
            // If sync breaks, just try again
            fprintf(stderr, "Frame read failed, retrying...\n");
            continue;
        }

        frame_count++;

        // Compute simple frame statistics: min/max
        uint16_t min = 0xFFFF;
        uint16_t max = 0x0000;

        for (int y = 0; y < FRAME_HEIGHT; ++y) {
            for (int x = 0; x < FRAME_WIDTH; ++x) {
                uint16_t v = frame[y][x];
                if (v < min) min = v;
                if (v > max) max = v;
            }
        }

        // Print basic info and first 10 pixels of the first line
        printf("Frame %d: min=%5u, max=%5u, first line[0..9] =",
               frame_count, min, max);

        for (int x = 0; x < 10 && x < FRAME_WIDTH; ++x) {
            printf(" %5u", frame[0][x]);
        }
        printf("\n");
        fflush(stdout);
    }

    close(fd);
    return 0;
}