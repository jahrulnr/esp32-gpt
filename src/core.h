#pragma once
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WebSocketsClient.h>

struct GPTSpiAllocator : ArduinoJson::Allocator {
  uint32_t getMemoryType() const {
    return MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT;
  }

  /**
   * @brief Allocate memory from SPI RAM with fallback to internal RAM
   * @param size Size of memory to allocate
   * @return Pointer to allocated memory
   */
  void* allocate(size_t size) override {
    return heap_caps_malloc(size, getMemoryType());
  }

  /**
   * @brief Deallocate memory previously allocated
   * @param pointer Pointer to memory to free
   */
  void deallocate(void* pointer) override {
    heap_caps_free(pointer);
  }

  /**
   * @brief Reallocate memory block to different size
   * @param ptr Pointer to memory to reallocate
   * @param new_size New size for memory block
   * @return Pointer to reallocated memory
   */
  void* reallocate(void* ptr, size_t new_size) override {
    return heap_caps_realloc(ptr, new_size, getMemoryType());
  }

  /**
   * @brief Get singleton instance of the allocator
   * @return SpiAllocator* Pointer to the singleton allocator
   */
  static GPTSpiAllocator* instance() {
    static GPTSpiAllocator instance;
    return &instance;
  }
};

class GPTSpiJsonDocument : public ArduinoJson::JsonDocument {
public:
  /**
   * @brief Construct a new SpiJsonDocument
   */
  GPTSpiJsonDocument() : ArduinoJson::JsonDocument(GPTSpiAllocator::instance()) {
    // In ArduinoJson 7, the capacity is managed dynamically
  }

  /**
   * @brief Construct from a JsonVariant
   * @param src The JsonVariant to copy
   */
  GPTSpiJsonDocument(const ArduinoJson::JsonVariant& src)
    : ArduinoJson::JsonDocument(GPTSpiAllocator::instance()) {
    this->set(src);
  }

  /**
   * @brief Get the capacity of the document (for backward compatibility)
   * @return The capacity in bytes
   */
  size_t capacity() const {
    return this->size();
  }
};

class GPTWifiClient : public NetworkClientSecure {
public:
	GPTWifiClient(){}
	~GPTWifiClient(){}
private:
	inline char* _streamLoad(Stream &stream, size_t size) {
		char *dest = (char *) heap_caps_malloc(size + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT);
		if (!dest) {
			return nullptr;
		}
		if (size != stream.readBytes(dest, size)) {
			heap_caps_free(dest);
			dest = nullptr;
			return nullptr;
		}
		dest[size] = '\0';
		return dest;
	}
};

class GPTClient : public HTTPClient {
public:
  GPTClient(){}
  ~GPTClient(){}

  /**
  * sendRequest
  * @param type const char *     "GET", "POST", ....
  * @param stream Stream *       data stream for the message body
  * @param size size_t           size for the message body if 0 not Content-Length is send
  * @return -1 if no info or > 0 when Content-Length is set by server
  */
  inline int sendRequest(const char *type, Stream *stream, size_t size) {

    if (!stream) {
      return returnError(HTTPC_ERROR_NO_STREAM);
    }

    // connect to server
    if (!connect()) {
      return returnError(HTTPC_ERROR_CONNECTION_REFUSED);
    }

    if (size > 0) {
      addHeader("Content-Length", String(size));
    }

    // add cookies to header, if present
    String cookie_string;
    if (generateCookieString(&cookie_string)) {
      addHeader("Cookie", cookie_string);
    }

    // send Header
    if (!sendHeader(type)) {
      return returnError(HTTPC_ERROR_SEND_HEADER_FAILED);
    }

    int buff_size = HTTP_TCP_TX_BUFFER_SIZE;

    int len = size;
    int bytesWritten = 0;

    if (len == 0) {
      len = -1;
    }

    // if possible create smaller buffer then HTTP_TCP_TX_BUFFER_SIZE
    if ((len > 0) && (len < buff_size)) {
      buff_size = len;
    }

    // create buffer for read
    uint8_t *buff = (uint8_t *) heap_caps_malloc(buff_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT);

    if (buff) {
      // read all data from stream and send it to server
      while (connected() && (stream->available() > -1) && (len > 0 || len == -1)) {

        // get available data size
        int sizeAvailable = stream->available();

        if (sizeAvailable) {
          int readBytes = sizeAvailable;

          // read only the asked bytes
          if (len > 0 && readBytes > len) {
            readBytes = len;
          }

          // not read more the buffer can handle
          if (readBytes > buff_size) {
            readBytes = buff_size;
          }

          // read data
          int bytesRead = stream->readBytes(buff, readBytes);

          // write it to Stream
          int bytesWrite = _client->write((const uint8_t *)buff, bytesRead);
          bytesWritten += bytesWrite;

          // are all Bytes a written to stream ?
          if (bytesWrite != bytesRead) {
            log_d("short write, asked for %d but got %d retry...", bytesRead, bytesWrite);

            // check for write error
            if (_client->getWriteError()) {
              log_d("stream write error %d", _client->getWriteError());

              //reset write error for retry
              _client->clearWriteError();
            }

            // some time for the stream
            delay(1);

            int leftBytes = (readBytes - bytesWrite);

            // retry to send the missed bytes
            bytesWrite = _client->write((const uint8_t *)(buff + bytesWrite), leftBytes);
            bytesWritten += bytesWrite;

            if (bytesWrite != leftBytes) {
              // failed again
              log_d("short write, asked for %d but got %d failed.", leftBytes, bytesWrite);
              heap_caps_free(buff);
              return returnError(HTTPC_ERROR_SEND_PAYLOAD_FAILED);
            }
          }

          // check for write error
          if (_client->getWriteError()) {
            log_d("stream write error %d", _client->getWriteError());
            heap_caps_free(buff);
            return returnError(HTTPC_ERROR_SEND_PAYLOAD_FAILED);
          }

          // count bytes to read left
          if (len > 0) {
          len -= readBytes;
          }

          delay(0);
        } else {
          delay(1);
        }
      }

      heap_caps_free(buff);

      if (size && (int)size != bytesWritten) {
      log_d("Stream payload bytesWritten %d and size %d mismatch!.", bytesWritten, size);
      log_d("ERROR SEND PAYLOAD FAILED!");
      return returnError(HTTPC_ERROR_SEND_PAYLOAD_FAILED);
      } else {
      log_d("Stream payload written: %d", bytesWritten);
      }

    } else {
      log_d("too less ram! need %d", buff_size);
      return returnError(HTTPC_ERROR_TOO_LESS_RAM);
    }

    // handle Server Response (Header)
    return returnError(handleHeaderResponse());
  }

  /**
  * write one Data Block to Stream
  * @param stream Stream *
  * @param size int
  * @return < 0 = error >= 0 = size written
  */
  inline int writeToStreamDataBlock(Stream *stream, int size) {
    int buff_size = HTTP_TCP_RX_BUFFER_SIZE;
    int len = size;
    int bytesWritten = 0;

    // if possible create smaller buffer then HTTP_TCP_RX_BUFFER_SIZE
    if ((len > 0) && (len < buff_size)) {
      buff_size = len;
    }

    // create buffer for read
    uint8_t *buff = (uint8_t *) heap_caps_malloc(buff_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT);

    if (buff) {
      // read all data from server
      while (connected() && (len > 0 || len == -1)) {

        // get available data size
        size_t sizeAvailable = buff_size;
        if (len < 0) {
          sizeAvailable = _client->available();
        }

        if (sizeAvailable) {

          int readBytes = sizeAvailable;

          // read only the asked bytes
          if (len > 0 && readBytes > len) {
            readBytes = len;
          }

          // not read more the buffer can handle
          if (readBytes > buff_size) {
            readBytes = buff_size;
          }

          // stop if no more reading
          if (readBytes == 0) {
            break;
          }

          // read data
          int bytesRead = _client->readBytes(buff, readBytes);

          // write it to Stream
          int bytesWrite = stream->write(buff, bytesRead);
          bytesWritten += bytesWrite;

          // are all Bytes a written to stream ?
          if (bytesWrite != bytesRead) {
            log_d("short write asked for %d but got %d retry...", bytesRead, bytesWrite);

            // check for write error
            if (stream->getWriteError()) {
              log_d("stream write error %d", stream->getWriteError());

              //reset write error for retry
              stream->clearWriteError();
            }

            // some time for the stream
            delay(1);

            int leftBytes = (bytesRead - bytesWrite);

            // retry to send the missed bytes
            bytesWrite = stream->write((buff + bytesWrite), leftBytes);
            bytesWritten += bytesWrite;

            if (bytesWrite != leftBytes) {
              // failed again
              log_w("short write asked for %d but got %d failed.", leftBytes, bytesWrite);
              heap_caps_free(buff);
              return HTTPC_ERROR_STREAM_WRITE;
            }
          }

          // check for write error
          if (stream->getWriteError()) {
            log_w("stream write error %d", stream->getWriteError());
            heap_caps_free(buff);
            return HTTPC_ERROR_STREAM_WRITE;
          }

          // count bytes to read left
          if (len > 0) {
            len -= bytesRead;
          }

          delay(0);
        } else {
          delay(1);
        }
      }

      heap_caps_free(buff);

      log_v("connection closed or file end (written: %d).", bytesWritten);

      if ((size > 0) && (size != bytesWritten)) {
      log_d("bytesWritten %d and size %d mismatch!.", bytesWritten, size);
      return HTTPC_ERROR_STREAM_WRITE;
      }

    } else {
      log_w("too less ram! need %d", buff_size);
      return HTTPC_ERROR_TOO_LESS_RAM;
    }

    return bytesWritten;
  }
};

extern GPTWifiClient gptWifiClient;
extern GPTClient gptHttp;
extern WebSocketsClient gptWebSocket;