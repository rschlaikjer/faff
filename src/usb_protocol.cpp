#include <usb_protocol.hpp>

namespace UsbProto {

static const unsigned libusb_timeout_ms = 100;

/*
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
*/
void Session::assert_libusb_ok(int code, const char *action) {
  if (code >= 0)
    return;

  fprintf(stderr, "%s: %s (%d)\n", action, libusb_error_name(code), code);
  exit(EXIT_FAILURE);
}

void Session::cmd_set_rgb_led(uint8_t r, uint8_t g, uint8_t b) {
  uint8_t cmd_out[] = {static_cast<uint8_t>(Opcode::SET_RGB_LED), r, g, b};
  int transferred = 0;
  int ret =
      libusb_bulk_transfer(_usb_handle, _args._usb_endpoint_tx, cmd_out,
                           sizeof(cmd_out), &transferred, libusb_timeout_ms);
  assert_libusb_ok(ret, "Failed to set LED colour");
}

void Session::cmd_fpga_reset_assert() {
  uint8_t cmd_out[] = {static_cast<uint8_t>(Opcode::FPGA_RESET_ASSERT)};
  int transferred = 0;
  int ret =
      libusb_bulk_transfer(_usb_handle, _args._usb_endpoint_tx, cmd_out,
                           sizeof(cmd_out), &transferred, libusb_timeout_ms);
  assert_libusb_ok(ret, "Failed to set assert FPGA reset line");
}

void Session::cmd_fpga_reset_deassert() {
  uint8_t cmd_out[] = {static_cast<uint8_t>(Opcode::FPGA_RESET_DEASSERT)};
  int transferred = 0;
  int ret =
      libusb_bulk_transfer(_usb_handle, _args._usb_endpoint_tx, cmd_out,
                           sizeof(cmd_out), &transferred, libusb_timeout_ms);
  assert_libusb_ok(ret, "Failed to deassert FPGA reset line");
}

void Session::cmd_fpga_query_status(uint8_t *out_status) {
  uint8_t cmd_out[] = {static_cast<uint8_t>(Opcode::FPGA_QUERY_STATUS)};
  int transferred = 0;
  int ret =
      libusb_bulk_transfer(_usb_handle, _args._usb_endpoint_tx, cmd_out,
                           sizeof(cmd_out), &transferred, libusb_timeout_ms);
  assert_libusb_ok(ret, "Failed to request FPGA state");
  // Read response
  ret = libusb_bulk_transfer(_usb_handle, _args._usb_endpoint_rx, out_status, 1,
                             &transferred, libusb_timeout_ms);
  assert_libusb_ok(ret, "Failed to read FPGA state response");
}

} // namespace UsbProto
