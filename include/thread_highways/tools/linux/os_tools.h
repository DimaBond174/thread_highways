#ifndef THREADS_HIGHWAYS_TOOLS_LINUX_OS_TOOLS_H
#define THREADS_HIGHWAYS_TOOLS_LINUX_OS_TOOLS_H

#include <cstdio>
#include <linux/limits.h>
#include <unistd.h>

#include <array>
#include <string>

namespace hi {

inline std::string get_exe_path()
{
  std::string re;
  char buf[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", buf, PATH_MAX);
  return std::string(buf, (len > 0)? len : 0);
} // get_exe_path

inline std::string get_exe_folder()
{
  std::string re;
  char buf[PATH_MAX];
  ssize_t len = 0;
  len = readlink("/proc/self/exe", buf, PATH_MAX);
  if (len>0)  {
      for(--len;  len>0;  --len)  {
          if ('/'  ==  buf[len])  {
              re = std::string(buf,  static_cast<uint32_t>(len));
              break;
          }
      }
  }
  return re;
} // get_exe_folder

inline const std::string exec_cmd(const std::string& cmd)
{
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};
    char buf[128];
    std::string  re;
    while (!feof(pipe))
    {
      if (fgets(buf, 128, pipe) != nullptr)
      {
            re.append(buf);
      }
    }
    pclose(pipe);
    return re;
} // exec_cmd

} // namespace hi

#endif // THREADS_HIGHWAYS_TOOLS_LINUX_OS_TOOLS_H
