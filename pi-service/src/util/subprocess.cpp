#include "util/subprocess.h"

#include <array>
#include <cerrno>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

namespace util {

ProcResult run(const std::vector<std::string>& argv) {
  ProcResult res;
  if (argv.empty()) return res;

  int out_pipe[2];
  int err_pipe[2];
  if (pipe(out_pipe) != 0) return res;
  if (pipe(err_pipe) != 0) {
    close(out_pipe[0]); close(out_pipe[1]);
    return res;
  }

  pid_t pid = fork();
  if (pid < 0) {
    close(out_pipe[0]); close(out_pipe[1]);
    close(err_pipe[0]); close(err_pipe[1]);
    return res;
  }

  if (pid == 0) {
    // Child: wire stdout/stderr to the pipes and exec. No shell is involved.
    dup2(out_pipe[1], STDOUT_FILENO);
    dup2(err_pipe[1], STDERR_FILENO);
    close(out_pipe[0]); close(out_pipe[1]);
    close(err_pipe[0]); close(err_pipe[1]);

    std::vector<char*> cargv;
    cargv.reserve(argv.size() + 1);
    for (const auto& a : argv) cargv.push_back(const_cast<char*>(a.c_str()));
    cargv.push_back(nullptr);

    execvp(cargv[0], cargv.data());
    _exit(127); // exec failed
  }

  // Parent: read both pipes concurrently via poll so a full stderr pipe can't
  // deadlock against a draining stdout pipe.
  close(out_pipe[1]);
  close(err_pipe[1]);

  struct pollfd fds[2] = {
    {out_pipe[0], POLLIN, 0},
    {err_pipe[0], POLLIN, 0},
  };
  std::string* dst[2] = {&res.out, &res.err};
  int open_fds = 2;

  while (open_fds > 0) {
    int pr = poll(fds, 2, -1);
    if (pr < 0) {
      if (errno == EINTR) continue;
      break;
    }
    for (int i = 0; i < 2; ++i) {
      if (fds[i].fd < 0) continue;
      if (fds[i].revents & (POLLIN | POLLHUP | POLLERR)) {
        std::array<char, 4096> buf{};
        ssize_t n = ::read(fds[i].fd, buf.data(), buf.size());
        if (n > 0) {
          dst[i]->append(buf.data(), static_cast<size_t>(n));
        } else {
          close(fds[i].fd);
          fds[i].fd = -1;
          --open_fds;
        }
      }
    }
  }

  int status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
  if (WIFEXITED(status)) {
    res.exit_code = WEXITSTATUS(status);
  } else {
    res.exit_code = -1;
  }
  return res;
}

} // namespace util
