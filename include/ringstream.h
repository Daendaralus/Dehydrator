#pragma once

#include <Stream.h>

/*
 * A LoopbackStream stores all data written in an internal buffer and returns it back when the stream is read.
 * 
 * ring buffer, oldest data is overwritten upon "overflow"
 * 
 * It can be used as a buffering layer between components.
 */
class LoopbackStream : public Stream {
  int *buffer;
  size_t buffer_size;
  int lastvalue;
public:
size_t pos, size;
  static const uint16_t DEFAULT_SIZE = 64;
  
  LoopbackStream(size_t buffer_size = LoopbackStream::DEFAULT_SIZE);
  ~LoopbackStream();
    
  /** Clear the buffer */
  void clear(); 
  
  virtual size_t write(uint8_t);
  virtual size_t write(int);
  virtual int availableForWrite(void);
  
  virtual int available();
  virtual int read();
  virtual int peek();
  virtual void flush();
  int lastWritten();
};
