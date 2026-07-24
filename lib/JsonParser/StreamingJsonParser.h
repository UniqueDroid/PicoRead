#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

struct JsonCallbacks {
  void* ctx;
  void (*onKey)(void* ctx, const char* key, size_t len);
  void (*onString)(void* ctx, const char* value, size_t len);
  void (*onNumber)(void* ctx, const char* value, size_t len);
  void (*onBool)(void* ctx, bool value);
  void (*onNull)(void* ctx);
  void (*onObjectStart)(void* ctx);
  void (*onObjectEnd)(void* ctx);
  void (*onArrayStart)(void* ctx);
  void (*onArrayEnd)(void* ctx);
};

class StreamingJsonParser {
 public:
  static constexpr size_t DEFAULT_TOKEN_BUF_SIZE = 512;
  static constexpr size_t MAX_NESTING = 32;

  // tokenBufSize bounds the longest key/string/number token this parser can hold;
  // longer tokens are dropped (see emitToken()/tokenOverflow), not truncated. The
  // default (512) covers ReleaseJsonParser's short fields (tag names, URLs); a
  // caller expecting long string values (e.g. free-text article extracts) should
  // pass a larger size. Heap-allocated regardless of size, so a larger buffer
  // doesn't eat into the caller's stack.
  explicit StreamingJsonParser(const JsonCallbacks& callbacks, size_t tokenBufSize = DEFAULT_TOKEN_BUF_SIZE);

  void reset();
  void feed(const char* data, size_t len);

  bool hasError() const { return error; }

 private:
  enum class State : uint8_t {
    SCANNING,
    IN_STRING_KEY,
    IN_STRING_VALUE,
    IN_NUMBER,
    IN_LITERAL,
    SKIP_STRING,
  };

  enum class Container : uint8_t {
    NONE,
    OBJECT,
    ARRAY,
  };

  void handleScanning(char c);
  void handleStringChar(char c);
  void handleNumber(char c);
  void handleLiteral(char c);
  void handleSkipString(char c);

  void appendToken(char c);
  void emitToken();

  bool inArray() const { return nestingDepth > 0 && nestingStack[nestingDepth - 1] == Container::ARRAY; }

  JsonCallbacks cb;
  std::unique_ptr<char[]> tokenBuf;
  size_t tokenBufSize;
  size_t tokenLen;
  State state;
  bool expectingValue;
  bool escaped;
  bool tokenOverflow;
  bool error;

  Container nestingStack[MAX_NESTING];
  uint8_t nestingDepth;

  char literalExpected[6];
  uint8_t literalLen;
  uint8_t literalPos;
};
