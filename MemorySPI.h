#ifndef memory_spi_h_
#define memory_spi_h_

#include "AudioStream.h"
#include "spi_interrupt.h"

#define SPIRAM_MOSI_PIN  7
#define SPIRAM_MISO_PIN  12
#define SPIRAM_SCK_PIN   14

#define SPIRAM_CS_PIN    6

#define SPISETTING SPISettings(20000000, MSBFIRST, SPI_MODE0)

#define MEM_SIZE (1024 * 1024) /* 1 Mb */

// TODO: Make this class singleton [MAYBE DONE], change in GateRec to use as singleton
class MemorySPI
{
public:
    MemorySPI() {
        initialize(MEM_SIZE);
    }
    uint32_t available(void) {
        return MEM_SIZE - allocated;
    }
    void beginSession();
    void endSession();

    void writeInt16Fast(uint32_t address, int16_t data);
    int16_t readInt16Fast(uint32_t address);

    void read(uint32_t address, uint32_t count, int16_t *data);
    void write(uint32_t address, uint32_t count, const int16_t *data);

    int16_t readInt16(uint32_t address);
    void writeInt16(uint32_t address, int16_t data);

    void zero(uint32_t address, uint32_t count) {
        write(address, count, NULL);
    }

    inline uint32_t memoryBegin(void) {
        return memory_begin;
    }

private:

    void initialize(uint32_t samples);

    uint32_t memory_begin;    // the first address in the memory we're using
    uint32_t memory_length;   // the amount of memory we're using
    uint8_t session;
    static uint32_t allocated;
};

#endif