#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include <cmdline.hpp>

static const char *getopt_optstring = "hf:";
static const struct option cmd_line_options[] = {
    {.name = "usb_vid",
     .has_arg = required_argument,
     .flag = nullptr,
     .val = 0},
    {.name = "usb_pid",
     .has_arg = required_argument,
     .flag = nullptr,
     .val = 0},
    {.name = "usb_serial",
     .has_arg = required_argument,
     .flag = nullptr,
     .val = 0},
    {.name = "lma", .has_arg = required_argument, .flag = nullptr, .val = 0},
    {.name = "file", .has_arg = required_argument, .flag = nullptr, .val = 0},
    {.name = "help", .has_arg = no_argument, .flag = nullptr, .val = 0},
    // Final value must be sentinel
    {0, 0, 0, 0},
};

void CliArgs::usage() {
  /* clang-format off */
fprintf(stderr, "faff: Find and Flash FPGAs\n"
"Common usage: faff -f top,bin\n\n"
"General options:\n"
"    -h|--help              This help message\n"
"    -f|--file  <binary>    The file that should be written to the target\n"
"    --lma <address>        The load memory address to use for the file.\n"
"                           Defaults to 0x0000\n"
"\n"
"Target selection:\n"
"    --usb_vid <vid>        Set vendor ID of device to use\n"
"    --usb_pid <pid>        Set product ID of device to use\n"
"    --usb_serial <serial>  Select device with specific serial <serial>. If not"
"                           specified, will attempt to program the first device"
"                           found with a matching VID:PID\n"
);
  /* clang-format on */
}

bool CliArgs::valid() {
  // If we got any bad / missing arguments, we aren't valid
  if (_arguments_invalid)
    return false;

  // If the USB vid/pid is out of range, args are invalid
  if (_usb_pid < 0 || _usb_pid > 0xFFFF)
    return false;
  if (_usb_vid < 0 || _usb_vid > 0xFFFF)
    return false;

  return true;
}

bool CliArgs::parse(int argc, char **argv) {
  int c;
  int longopt_index = 0;
  while ((c = getopt_long(argc, argv, getopt_optstring, cmd_line_options,
                          &longopt_index)) != -1) {
    if (c == '?') {
      // Unexpected argument, or missing required argument
      _arguments_invalid = true;
    } else if (c == 'h') {
      _help_selected = true;
    } else if (c == 'f') {
      _file_path = optarg;
    } else if (c == 0) {
      // Long option
      const char *option_name = cmd_line_options[longopt_index].name;
      if (!strcmp("help", option_name)) {
        _help_selected = true;
      } else if (!strcmp("usb_pid", option_name)) {
        _usb_pid = std::stoi(optarg, nullptr, 16);
      } else if (!strcmp("usb_vid", option_name)) {
        _usb_vid = std::stoi(optarg, nullptr, 16);
      } else if (!strcmp("usb_serial", option_name)) {
        _usb_serial_specified = true;
        _usb_serial = std::string(optarg);
      } else if (!strcmp("lma", option_name)) {
        _file_lma = std::stoi(optarg);
      } else if (!strcmp("file", option_name)) {
        _file_path = optarg;
      }
    }
  }

  return valid();
}
