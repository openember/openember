#pragma once
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <sys/socket.h>
#include <unistd.h>

namespace openember::launch {
// Call once, after initialization and endpoint registration, before the run
// loop. Standalone processes have no notification channel and succeed without
// action.
inline bool NotifyReady() {
  const char *text = std::getenv("OPENEMBER_READY_FD");
  if (!text)
    return true;
  char *end = nullptr;
  errno = 0;
  const long value = std::strtol(text, &end, 10);
  if (errno || !*text || *end || value < 3 || value > INT_MAX)
    return false;
  const int fd = static_cast<int>(value);
  char ready = 'R';
  ssize_t count;
  do {
    count = ::send(fd, &ready, 1, MSG_NOSIGNAL);
  } while (count < 0 && errno == EINTR);
  ::close(fd);
  ::unsetenv("OPENEMBER_READY_FD");
  return count == 1;
}
} // namespace openember::launch
