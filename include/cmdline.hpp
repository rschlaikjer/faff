#pragma once

#include <string>

struct CliArgs {
  void usage();
  bool parse(int argc, char **argv);
  bool valid();

public:
  // Were sufficient arguments parsed to perform a useful action, or should the
  // program print the usage intormation and exit?
  bool _arguments_invalid = false;

  // Should we just print help and exit?
  bool _help_selected = false;

  // Default VID:PID values are a test value from
  // http://pid.codes/pids/
  int _usb_vid = 0x1209;
  int _usb_pid = 0x0001;
  // The firmware this tool was designed to work with happens to have
  // this as the interface number.
  int _usb_interface = 2;

  // Serial selection. If a serial number is specified in the cli args, only
  // bind to a device with that serial. If no serial is specified, bind to any
  // device that has the right vid:pid
  bool _usb_serial_specified = false;
  std::string _usb_serial = "";
};