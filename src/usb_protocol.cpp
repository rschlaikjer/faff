#include <string.h>

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

bool Session::fpga_is_under_reset() {
  uint8_t fpga_status;
  cmd_fpga_query_status(&fpga_status);
  return fpga_status &
         static_cast<uint8_t>(UsbProto::FpgaStatusFlags::FLAG_FPGA_UNDER_RESET);
}

void Session::cmd_flash_identify(uint8_t *out_mfgr, uint8_t *out_device,
                                 uint64_t *out_unique_id) {
  uint8_t cmd_out[] = {static_cast<uint8_t>(Opcode::FLASH_IDENTIFY)};
  int transferred = 0;
  int ret =
      libusb_bulk_transfer(_usb_handle, _args._usb_endpoint_tx, cmd_out,
                           sizeof(cmd_out), &transferred, libusb_timeout_ms);
  assert_libusb_ok(ret, "Failed to request Flash properties");

  // Read response
  uint8_t resp[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  ret = libusb_bulk_transfer(_usb_handle, _args._usb_endpoint_rx, resp,
                             sizeof(resp), &transferred, libusb_timeout_ms);
  assert_libusb_ok(ret, "Failed to read Flash properties response");

  // Pull out the mfgr/device
  *out_mfgr = resp[0];
  *out_device = resp[1];

  // Unique ID
  *out_unique_id = ((((uint64_t)resp[2]) << 56) | (((uint64_t)resp[3]) << 48) |
                    (((uint64_t)resp[4]) << 40) | (((uint64_t)resp[5]) << 32) |
                    (((uint64_t)resp[6]) << 24) | (((uint64_t)resp[7]) << 16) |
                    (((uint64_t)resp[8]) << 8) | (((uint64_t)resp[9]) << 0));
}

void Session::cmd_flash_erase_4k(uint32_t addr) {
  uint8_t cmd_out[] = {
      static_cast<uint8_t>(Opcode::FLASH_ERASE_4K),
      ((uint8_t)(addr >> 24)),
      ((uint8_t)(addr >> 16)),
      ((uint8_t)(addr >> 8)),
      ((uint8_t)(addr >> 0)),
  };
  int transferred = 0;
  int ret =
      libusb_bulk_transfer(_usb_handle, _args._usb_endpoint_tx, cmd_out,
                           sizeof(cmd_out), &transferred, libusb_timeout_ms);
  assert_libusb_ok(ret, "Failed to initiate 4k sector erase");
}

void Session::cmd_flash_erase_32k(uint32_t addr) {
  uint8_t cmd_out[] = {
      static_cast<uint8_t>(Opcode::FLASH_ERASE_32K),
      ((uint8_t)(addr >> 24)),
      ((uint8_t)(addr >> 16)),
      ((uint8_t)(addr >> 8)),
      ((uint8_t)(addr >> 0)),
  };
  int transferred = 0;
  int ret =
      libusb_bulk_transfer(_usb_handle, _args._usb_endpoint_tx, cmd_out,
                           sizeof(cmd_out), &transferred, libusb_timeout_ms);
  assert_libusb_ok(ret, "Failed to initiate 32k sector erase");
}

void Session::cmd_flash_erase_64k(uint32_t addr) {
  uint8_t cmd_out[] = {
      static_cast<uint8_t>(Opcode::FLASH_ERASE_64K),
      ((uint8_t)(addr >> 24)),
      ((uint8_t)(addr >> 16)),
      ((uint8_t)(addr >> 8)),
      ((uint8_t)(addr >> 0)),
  };
  int transferred = 0;
  int ret =
      libusb_bulk_transfer(_usb_handle, _args._usb_endpoint_tx, cmd_out,
                           sizeof(cmd_out), &transferred, libusb_timeout_ms);
  assert_libusb_ok(ret, "Failed to initiate 64k sector erase");
}

void Session::cmd_flash_erase_chip() {
  uint8_t cmd_out[] = {static_cast<uint8_t>(Opcode::FLASH_WRITE)};
  int transferred = 0;
  int ret =
      libusb_bulk_transfer(_usb_handle, _args._usb_endpoint_tx, cmd_out,
                           sizeof(cmd_out), &transferred, libusb_timeout_ms);
  assert_libusb_ok(ret, "Failed to initiate chip erase");
}

void Session::cmd_flash_write(uint32_t addr, const uint8_t *data,
                              uint8_t size) {
  uint8_t cmd_out[size + 6] = {
      static_cast<uint8_t>(Opcode::FLASH_WRITE),
      ((uint8_t)(addr >> 24)),
      ((uint8_t)(addr >> 16)),
      ((uint8_t)(addr >> 8)),
      ((uint8_t)(addr >> 0)),
      size,
  };
  memcpy(&cmd_out[6], data, size);
  int transferred = 0;
  int ret =
      libusb_bulk_transfer(_usb_handle, _args._usb_endpoint_tx, cmd_out,
                           sizeof(cmd_out), &transferred, libusb_timeout_ms);
  assert_libusb_ok(ret, "Failed to initiate flash write");
}

void Session::cmd_flash_read(uint32_t addr, uint8_t *out_data, uint8_t size) {
  uint8_t cmd_out[] = {
      static_cast<uint8_t>(Opcode::FLASH_READ),
      ((uint8_t)(addr >> 24)),
      ((uint8_t)(addr >> 16)),
      ((uint8_t)(addr >> 8)),
      ((uint8_t)(addr >> 0)),
      size,
  };
  int transferred = 0;
  int ret =
      libusb_bulk_transfer(_usb_handle, _args._usb_endpoint_tx, cmd_out,
                           sizeof(cmd_out), &transferred, libusb_timeout_ms);
  assert_libusb_ok(ret, "Failed to request Flash status");
  // Read response
  ret = libusb_bulk_transfer(_usb_handle, _args._usb_endpoint_rx, out_data,
                             size, &transferred, libusb_timeout_ms);
  assert_libusb_ok(ret, "Failed to read Flash status response");
}

void Session::cmd_flash_query_status(uint8_t *out_status) {
  uint8_t cmd_out[] = {static_cast<uint8_t>(Opcode::FLASH_QUERY_STATUS)};
  int transferred = 0;
  int ret =
      libusb_bulk_transfer(_usb_handle, _args._usb_endpoint_tx, cmd_out,
                           sizeof(cmd_out), &transferred, libusb_timeout_ms);
  assert_libusb_ok(ret, "Failed to request Flash status");
  // Read response
  ret = libusb_bulk_transfer(_usb_handle, _args._usb_endpoint_rx, out_status, 1,
                             &transferred, libusb_timeout_ms);
  assert_libusb_ok(ret, "Failed to read Flash status response");
}

bool Session::flash_busy() {
  uint8_t flash_status;
  cmd_flash_query_status(&flash_status);
  return flash_status &
         static_cast<uint8_t>(UsbProto::FlashStatusFlash::FLAG_FLASH_BUSY);
}

} // namespace UsbProto
