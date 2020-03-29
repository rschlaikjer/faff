#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include <cmdline.hpp>

static const char *getopt_optstring = "hf:";
static const struct option cmd_line_options[] = {
    {.name = "usb-vid",
     .has_arg = required_argument,
     .flag = nullptr,
     .val = 0},
    {.name = "usb-pid",
     .has_arg = required_argument,
     .flag = nullptr,
     .val = 0},
    {.name = "usb-serial",
     .has_arg = required_argument,
     .flag = nullptr,
     .val = 0},
    {.name = "lma", .has_arg = required_argument, .flag = nullptr, .val = 0},
    {.name = "file", .has_arg = required_argument, .flag = nullptr, .val = 0},
    {.name = "no-verify", .has_arg = no_argument, .flag = nullptr, .val = 0},
    {.name = "help", .has_arg = no_argument, .flag = nullptr, .val = 0},
    // Final value must be sentinel
    {0, 0, 0, 0},
};

void CliArgs::usage() {
  /* clang-format off */
fprintf(stderr, "faff: Find and Flash FPGA\n"
"Common usage: faff -f top,bin\n\n"
"General options:\n"
"    -h|--help              This help message\n"
"    -f|--file  <binary>    The file that should be written to the target\n"
"    --lma <address>        The load memory address to use for the file.\n"
"                           Defaults to 0x0000\n"
"    --no-verify            Disable reading back the programmed file to\n"
"                           verify that programming was successful."
"\n"
"Target selection:\n"
"    --usb-vid <vid>        Set vendor ID of device to use\n"
"    --usb-pid <pid>        Set product ID of device to use\n"
"    --usb-serial <serial>  Select device with specific serial <serial>. If not\n"
"                           specified, will attempt to program the first device\n"
"                           found with a matching VID:PID\n"
);
  /* clang-format on */
}

bool CliArgs::valid() {
  // If we got any bad / missing arguments, we aren't valid
  if (_arguments_invalid)
    return false;

  // If we didn't get a file specified then args are invalid
  if (_file_path == nullptr)
    return false;

  // If the USB vid/pid is out of range, args are invalid
  if (_usb_pid < 0 || _usb_pid > 0xFFFF)
    return false;
  if (_usb_vid < 0 || _usb_vid > 0xFFFF)
    return false;

  return true;
}

void CliArgs::report_errors() {
  // If we got any bad / missing arguments, we aren't valid
  if (_arguments_invalid)
    fprintf(stderr, "Unexpected arguments encountered\n");

  // If we didn't get a file specified then args are invalid
  if (_file_path == nullptr)
    fprintf(stderr, "No input file specified\n");

  // If the USB vid/pid is out of range, args are invalid
  if (_usb_vid < 0 || _usb_vid > 0xFFFF)
    fprintf(stderr, "USB VID %x is outside allowable range\n", _usb_vid);
  if (_usb_pid < 0 || _usb_pid > 0xFFFF)
    fprintf(stderr, "USB PID %x is outside allowable range\n", _usb_pid);
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
      } else if (!strcmp("usb-pid", option_name)) {
        _usb_pid = std::stoi(optarg, nullptr, 16);
      } else if (!strcmp("usb-vid", option_name)) {
        _usb_vid = std::stoi(optarg, nullptr, 16);
      } else if (!strcmp("usb-serial", option_name)) {
        _usb_serial_specified = true;
        _usb_serial = std::string(optarg);
      } else if (!strcmp("lma", option_name)) {
        _file_lma = std::stoi(optarg);
      } else if (!strcmp("file", option_name)) {
        _file_path = optarg;
      } else if (!strcmp("no-verify", option_name)) {
        _verify_programmed = false;
      }
    }
  }

  return valid();
}
