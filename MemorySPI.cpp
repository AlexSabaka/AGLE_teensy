#include "MemorySPI.h"

uint32_t MemorySPI::allocated = 0;

void MemorySPI::initialize(uint32_t samples)
{
    uint32_t memsize, avail;

    session = 0;
    // Specified only for 23LC1024
    memsize = MEM_SIZE;
    pinMode(SPIRAM_CS_PIN, OUTPUT);
    digitalWriteFast(SPIRAM_CS_PIN, HIGH);


    avail = memsize - allocated;
    if (avail < AUDIO_BLOCK_SAMPLES * 2 + 1) {
        return;
    }

    if (samples > avail) samples = avail;
    memory_begin = allocated;
    allocated += samples;
    memory_length = samples;

    SPI.setMOSI(SPIRAM_MOSI_PIN);
    SPI.setMISO(SPIRAM_MISO_PIN);
    SPI.setSCK(SPIRAM_SCK_PIN);

    SPI.begin();
    zero(0, memory_length);
}


void MemorySPI::beginSession() {
    if (session) return;

    session = 1;

    SPI.beginTransaction(SPISETTING);
    digitalWriteFast(SPIRAM_SCK_PIN, LOW);
}

void MemorySPI::endSession() {
    if (!session) return;

    session = 0;

    digitalWriteFast(SPIRAM_SCK_PIN, HIGH);
    SPI.endTransaction();
}

void MemorySPI::writeInt16Fast(uint32_t address, int16_t data) {
    if (!session) return;

    uint32_t addr = memory_begin + address;
    addr *= 2;

    SPI.transfer16((0x03 << 8) | (addr >> 16));
    SPI.transfer16(addr & 0xFFFF);

    SPI.transfer16(data);
}

int16_t MemorySPI::readInt16Fast(uint32_t address) {
    if (!session) return 0; //TODO: add error handling

    uint32_t addr = memory_begin + address;
    addr *= 2;

    SPI.transfer16((0x03 << 8) | (addr >> 16));
    SPI.transfer16(addr & 0xFFFF);

    return (int16_t)SPI.transfer16(0);
}

int16_t MemorySPI::readInt16(uint32_t address) {
    uint32_t addr = memory_begin + address;
    addr *= 2;
    SPI.beginTransaction(SPISETTING);
    digitalWriteFast(SPIRAM_SCK_PIN, LOW);

    SPI.transfer16((0x03 << 8) | (addr >> 16));
    SPI.transfer16(addr & 0xFFFF);

    int16_t data = (int16_t)SPI.transfer16(0);

    digitalWriteFast(SPIRAM_SCK_PIN, HIGH);
    SPI.endTransaction();

    return data;
}

void MemorySPI::writeInt16(uint32_t address, int16_t data) {
    uint32_t addr = memory_begin + address;
    addr *= 2;
    SPI.beginTransaction(SPISETTING);
    digitalWriteFast(SPIRAM_SCK_PIN, LOW);

    SPI.transfer16((0x03 << 8) | (addr >> 16));
    SPI.transfer16(addr & 0xFFFF);

    SPI.transfer16(data);

    digitalWriteFast(SPIRAM_SCK_PIN, HIGH);
    SPI.endTransaction();
}

void MemorySPI::read(uint32_t offset, uint32_t count, int16_t *data)
{
    uint32_t addr = memory_begin + offset;

    addr *= 2;
    SPI.beginTransaction(SPISETTING);
    digitalWriteFast(SPIRAM_CS_PIN, LOW);       // begin receiving data
    SPI.transfer16((0x03 << 8) | (addr >> 16)); // Set higt(addr)
    SPI.transfer16(addr & 0xFFFF);              // Set low(addr)

    while (count) {
        *data++ = (int16_t)(SPI.transfer16(0)); // Gets 16 bits from SPI
        count--;                                // while count > 0
    }

    digitalWriteFast(SPIRAM_CS_PIN, HIGH);      // end receiving data
    SPI.endTransaction();
}

void MemorySPI::write(uint32_t offset, uint32_t count, const int16_t *data)
{
    uint32_t addr = memory_begin + offset;

    addr *= 2;
    SPI.beginTransaction(SPISETTING);           // Same as in the read function
    digitalWriteFast(SPIRAM_CS_PIN, LOW);
    SPI.transfer16((0x02 << 8) | (addr >> 16));
    SPI.transfer16(addr & 0xFFFF);

    while (count) {
        int16_t w = 0;                          // except this lines
        if (data) w = *data++;                  // while can read data
        SPI.transfer16(w);                      // write transfer it into SPI
        count--;
    }

    digitalWriteFast(SPIRAM_CS_PIN, HIGH);
    SPI.endTransaction();
}