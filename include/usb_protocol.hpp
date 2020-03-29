#pragma once

#include <libusb.h>
#include <stdint.h>

#include <cmdline.hpp>

namespace UsbProto {

enum class Opcode : uint8_t {
  // General
  NOP = 0x00,
  SET_RGB_LED = 0x01,
  // FPGA interface
  FPGA_RESET_ASSERT = 0x10,
  FPGA_RESET_DEASSERT = 0x11,
  FPGA_QUERY_STATUS = 0x012,
  // Flash interface
  FLASH_IDENTIFY = 0x20,
  FLASH_ERASE_4K = 0x21,
  FLASH_ERASE_32K = 0x22,
  FLASH_ERASE_64K = 0x23,
  FLASH_ERASE_CHIP = 0x24,
  FLASH_WRITE = 0x25,
  FLASH_READ = 0x26,
  FLASH_QUERY_STATUS = 0x27,
};

enum class FpgaStatusFlags : uint8_t {
  FLAG_FPGA_UNDER_RESET = (1 << 0),
};

class Session {
public:
  Session(libusb_device_handle *usb_handle, CliArgs &args)
      : _usb_handle(usb_handle), _args(args) {}

  // General
  void cmd_set_rgb_led(uint8_t r, uint8_t g, uint8_t b);

  // FPGA
  void cmd_fpga_reset_assert();
  void cmd_fpga_reset_deassert();
  void cmd_fpga_query_status(uint8_t *out_status);
  bool fpga_is_under_reset();

  // Flash
  void cmd_flash_identify(uint8_t *out_mfgr, uint8_t *out_device);

private:
  void assert_libusb_ok(int code, const char *action);

private:
  libusb_device_handle *_usb_handle;
  CliArgs _args;
};
} // namespace UsbProto
