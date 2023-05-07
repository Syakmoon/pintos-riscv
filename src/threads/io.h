#ifndef THREADS_IO_H
#define THREADS_IO_H

#include <stddef.h>
#include <stdint.h>

/* Reads and returns a byte from ADDRESS. */
static inline uint8_t inb(volatile uint8_t* address) {
  uint8_t data;
  data = *address;
  return data;
}

/* Reads CNT bytes from ADDRESS, one after another, and stores them
   into the buffer starting at ADDR. */
static inline void insb(volatile uint8_t* address, void* addr, size_t cnt) {
  uint8_t* type_addr = addr;
  while (cnt > 0) {
    *(type_addr++) = *(address++);
    --cnt;
  }
}

/* Reads and returns 16 bits from ADDRESS. */
static inline uint16_t inw(uint16_t* address) {
  uint16_t data;
  data = *address;
  return data;
}

/* Reads CNT 16-bit (halfword) units from ADDRESS, one after
   another, and stores them into the buffer starting at ADDR. */
static inline void insw(volatile uint16_t* address, void* addr, size_t cnt) {
  uint16_t* type_addr = addr;
  while (cnt > 0) {
    *(type_addr++) = *(address++);
    --cnt;
  }
}

/* Reads and returns 32 bits from ADDRESS. */
static inline uint32_t inl(volatile uint32_t* address) {
  uint32_t data;
  data = *address;
  return data;
}

/* Reads CNT 32-bit (word) units from ADDRESS, one after another,
   and stores them into the buffer starting at ADDR. */
static inline void insl(volatile uint32_t* address, void* addr, size_t cnt) {
  uint32_t* type_addr = addr;
  while (cnt > 0) {
    *(type_addr++) = *(address++);
    --cnt;
  }
}

/* Writes byte DATA to ADDRESS. */
static inline void outb(volatile uint8_t* address, uint8_t data) {
  *address = data;
}

/* Writes to ADDRESS each byte of data in the CNT-byte buffer
   starting at ADDR. */
static inline void outsb(volatile uint8_t* address, const void* addr, size_t cnt) {
  uint8_t* type_addr = addr;
  while (cnt > 0) {
    *(address++) = *(type_addr++);
    --cnt;
  }
}

/* Writes the 16-bit DATA to ADDRESS. */
static inline void outw(volatile uint16_t* address, uint16_t data) {
  *address = data;
}

/* Writes to ADDRESS each 16-bit unit (halfword) of data in the
   CNT-halfword buffer starting at ADDR. */
static inline void outsw(volatile uint16_t* address, const void* addr, size_t cnt) {
  uint16_t* type_addr = addr;
  while (cnt > 0) {
    *(address++) = *(type_addr++);
    --cnt;
  }
}

/* Writes the 32-bit DATA to ADDRESS. */
static inline void outl(volatile uint32_t* address, uint32_t data) {
  *address = data;
}

/* Writes to ADDRESS each 32-bit unit (word) of data in the CNT-word
   buffer starting at ADDR. */
static inline void outsl(volatile uint32_t* address, const void* addr, size_t cnt) {
  uint32_t* type_addr = addr;
  while (cnt > 0) {
    *(address++) = *(type_addr++);
    --cnt;
  }
}

#endif /* threads/io.h */
