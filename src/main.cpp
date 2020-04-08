#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <libusb.h>

#include <memory>
#include <string>

#include <cmdline.hpp>
#include <usb_protocol.hpp>

std::string get_serial_for_device(libusb_device_handle *handle) {
  struct libusb_device_descriptor desc;
  int ret = libusb_get_device_descriptor(libusb_get_device(handle), &desc);
  if (ret < 0) {
    fprintf(stderr, "Failed to read device descriptor\n");
    return "";
  }

  char buf[256];
  if (desc.iSerialNumber) {
    ret = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber,
                                             reinterpret_cast<uint8_t *>(buf),
                                             sizeof(buf));
    if (ret < 0) {
      fprintf(stderr, "Failed to query serial descriptor\n");
      return "";
    }
    return std::string(buf);
  } else {
    fprintf(stderr, "Device does not have a serial number\n");
    return "";
  }
}

std::string get_serial_for_device(libusb_device *dev) {
  libusb_device_handle *handle;
  int ret = libusb_open(dev, &handle);
  if (ret < 0)
    return "";
  std::string serial = get_serial_for_device(handle);
  libusb_close(handle);
  return serial;
}

libusb_device_handle *get_device(CliArgs &args) {
  // Get a list of all the USB devices in the system
  libusb_device **devices;
  ssize_t device_count = libusb_get_device_list(nullptr, &devices);

  // Create a scoped pointer to free the list again
  std::shared_ptr<void> _defer_free_device_list(
      nullptr,
      [=](...) { // Decref and free device list
        libusb_free_device_list(devices, 1);
      });

  // Iterate the devices, and check if any of them have both the right VID:PID
  // _and_ the right serial
  for (ssize_t i = 0; i < device_count; i++) {
    // Read the descriptor for this device
    libusb_device_descriptor desc{};
    int ret = libusb_get_device_descriptor(devices[i], &desc);
    if (ret < 0) {
      fprintf(stderr, "Failed to get device descriptor\n");
      return nullptr;
    }

    // Is the VID:PID correct?
    if (desc.idVendor != args._usb_vid || desc.idProduct != args._usb_pid)
      continue;

    // Open that device up
    libusb_device_handle *handle;
    ret = libusb_open(devices[i], &handle);
    if (ret < 0) {
      fprintf(stderr, "Failed to open device\n");
      return nullptr;
    }

    // Do the arguments specify a device serial to use?
    if (!args._usb_serial_specified) {
      // Done, return the handle
      return handle;
    } else {
      // Is the serial a match?
      std::string device_serial = get_serial_for_device(handle);
      if (!args._usb_serial.compare(device_serial)) {
        return handle;
      } else {
        // Close it again, not a serial match
        libusb_close(handle);
      }
    }
  }

  // Matching device not found
  return nullptr;
}

void enumerate_devices(CliArgs &args) {
  // Get a list of all the USB devices in the system
  libusb_device **devices;
  ssize_t device_count = libusb_get_device_list(nullptr, &devices);

  // Create a scoped pointer to free the list again
  std::shared_ptr<void> _defer_free_device_list(
      nullptr,
      [=](...) { // Decref and free device list
        libusb_free_device_list(devices, 1);
      });

  fprintf(stderr, "Searching for devices with VID:PID %04x:%04x\n",
          args._usb_vid, args._usb_pid);

  // Iterate the devices, and if they match VID:PID print their serial
  unsigned devices_found = 0;
  for (ssize_t i = 0; i < device_count; i++) {
    // Read the descriptor for this device
    libusb_device_descriptor desc{};
    int ret = libusb_get_device_descriptor(devices[i], &desc);
    if (ret < 0) {
      fprintf(stderr, "Failed to get device descriptor: %s (%d)\n",
              libusb_error_name(ret), ret);
      exit(EXIT_FAILURE);
    }

    // Is the VID:PID correct?
    if (desc.idVendor != args._usb_vid || desc.idProduct != args._usb_pid)
      continue;

    // Open that device up
    libusb_device_handle *handle;
    ret = libusb_open(devices[i], &handle);
    if (ret < 0) {
      fprintf(stderr, "Failed to open device: %s (%d)\n",
              libusb_error_name(ret), ret);
      continue;
    }

    // Get and print the serial
    std::string device_serial = get_serial_for_device(handle);

    fprintf(stderr, "[%u] Serial: %s\n", devices_found++,
            device_serial.c_str());
  }

  if (devices_found) {
    fprintf(stderr, "Found %u devices\n", devices_found);
  } else {
    fprintf(stderr, "Failed to find any devices\n");
  }
}

struct BitstreamFile {
  BitstreamFile(uint8_t *data, off_t size) : _data(data), _size(size) {}
  ~BitstreamFile() { munmap(_data, _size); }

  uint8_t *_data;
  off_t _size;
};

std::unique_ptr<BitstreamFile> open_bitstream(const char *file_path) {
  // Open the file
  int file_fd = open(file_path, O_RDONLY);

  // If we failed to open, return nullptr
  if (file_fd < 0) {
    return nullptr;
  }

  // Defer closing the file again
  std::shared_ptr<void> _defer_close_fd(nullptr, [=](...) { close(file_fd); });

  // Get the file size
  const off_t file_size = lseek(file_fd, 0, SEEK_END);
  if (file_size < 0) {
    return nullptr;
  }

  // Move back to the start of the file
  if (lseek(file_fd, 0, SEEK_SET) < 0) {
    return nullptr;
  }

  // MMap up the data
  void *mmapped_data = mmap(nullptr, // No addressing requirements
                            file_size,
                            PROT_READ,   // Read-only
                            MAP_PRIVATE, // Do not share, do not change the file
                            file_fd,     // File to map from
                            0            // Offset 0
  );

  // If mmap failed, return nullptr
  if (mmapped_data == nullptr) {
    return nullptr;
  }

  // Wrap up and return
  return std::make_unique<BitstreamFile>(
      reinterpret_cast<uint8_t *>(mmapped_data), file_size);
}

char nibble_to_hex(uint8_t nibble) {
  if (nibble < 10) {
    return '0' + nibble;
  }
  return 'A' + (nibble - 10);
}

void byte_to_hex(uint8_t val, char *out_buf) {
  out_buf[0] = nibble_to_hex((val >> 4) & 0xF);
  out_buf[1] = nibble_to_hex((val >> 4) & 0xF);
}

void print_binary_diff(uint8_t *expected, uint8_t *read, unsigned byte_count,
                       uint32_t offset) {
  // Two hex chars + space per byte, plus null terminator
  const int char_count = (byte_count * 3) + 1;
  char expected_str[char_count];
  char read_str[char_count];

  // Convert hex bytes
  for (unsigned i = 0; i < byte_count; i++) {
    byte_to_hex(expected[i], &expected_str[i * 3]);
    expected_str[i * 3 + 2] = ' ';
    byte_to_hex(read[i], &read_str[i * 3]);
    read_str[i * 3 + 2] = ' ';
  }

  // Null terminate
  expected_str[char_count - 1] = '\0';
  read_str[char_count - 1] = '\0';

  // Print diff
  fprintf(stderr,
          "Verify error for block of size %d at 0x%08x:\n"
          "    Expected: %s\n"
          "    Read:     %s\n",
          byte_count, offset, expected_str, read_str);
}

int main(int argc, char **argv) {
  CliArgs args;
  args.parse(argc, argv);

  // If help was specified, just print that and exit
  if (args._help_selected) {
    args.usage();
    return EXIT_SUCCESS;
  }

  // Attempt to init libusb
  if (libusb_init(NULL) < 0) {
    fprintf(stderr, "Failed to initialize libusb\n");
    return EXIT_FAILURE;
  }

  // If we are in enumerate only mode, print available devices and exit
  if (args._enumerate_only) {
    enumerate_devices(args);
    return EXIT_SUCCESS;
  }

  // If we didn't short circuit for help, and the args are invalid, error out
  if (!args.valid()) {
    args.report_errors();
    fprintf(stderr, "To view help, ruh %s -h\n", argv[0]);
    return EXIT_FAILURE;
  }

  // Try and open the file we're trying to program
  std::unique_ptr<BitstreamFile> file = open_bitstream(args._file_path);
  if (file == nullptr) {
    fprintf(stderr, "Failed to open bitstream file '%s'\n", args._file_path);
    return EXIT_FAILURE;
  }

  // Try and open USB device
  libusb_device_handle *usb_handle = get_device(args);
  if (usb_handle == nullptr) {
    fprintf(stderr, "Failed to find device with VID:PID %04x:%04x\n",
            args._usb_vid, args._usb_pid);
    return EXIT_FAILURE;
  }

  // Claim programming interface
  if (libusb_claim_interface(usb_handle, args._usb_interface) < 0) {
    fprintf(stderr, "Failed to claim usb interface 0x%02" PRIx16 "\n",
            args._usb_interface);
    return EXIT_FAILURE;
  }

  // Get the serial for this device
  std::string serial = get_serial_for_device(usb_handle);
  fprintf(stderr, "Claimed device %04" PRIx16 ":%04" PRIx16 " with serial %s\n",
          args._usb_vid, args._usb_pid, serial.c_str());

  // Wrap it in a protocol layer
  UsbProto::Session session(usb_handle, args);

  // Disable the target FPGA so that we can control the SPI flash
  session.cmd_fpga_reset_assert();
  session.cmd_set_rgb_led(0, 128, 0);

  // Verify we are now in programming mode
  if (!session.fpga_is_under_reset()) {
    fprintf(stderr, "Failed to assert FPGA reset\n");
    return EXIT_FAILURE;
  }

  // Get the flash chip ID
  uint8_t flash_mfgr, flash_device;
  uint64_t flash_unique_id;
  session.cmd_flash_identify(&flash_mfgr, &flash_device, &flash_unique_id);
  fprintf(stderr,
          "Flash chip mfgr: 0x%02" PRIx16 ", Device ID: 0x%02" PRIx16
          " Unique ID: 0x%016" PRIx64 "\n",
          flash_mfgr, flash_device, flash_unique_id);

  // Indicator LED to yellow for act
  session.cmd_set_rgb_led(64, 32, 0);

  // Start writing the flash. Every time we touch a new 4k sector, we need to
  // erase it before we can write it.
  uint32_t previous_sector = 0xFFFF'FFFF;
  // TODO(ross): respect the LMA option
  // unsigned _file_lma = 0x0;
  for (unsigned byte_offset = 0; byte_offset < file->_size;) {
    // Mask off the sector bits
    uint32_t sector = (args._file_lma + byte_offset) & 0xFF'FF'F0'00;
    if (sector != previous_sector) {
      // Start the erase operation
      session.cmd_flash_erase_4k(sector);
      // Wait for erase complete
      do {
        usleep(5'000);
      } while (session.flash_busy());
      // Update the prev sector value
      previous_sector = sector;
    }

    // USB FS max packet size is 64 bytes. We have some overhead, so biggest
    // power of 2 is 32.
    uint8_t data[32];
    size_t bytes_to_copy = ((long)sizeof(data)) < (file->_size - byte_offset)
                               ? sizeof(data)
                               : (file->_size - byte_offset);
    memcpy(data, &file->_data[byte_offset], bytes_to_copy);
    fprintf(stderr, "Programming block 0x%012" PRIx32 " / 0x%012lx\r",
            args._file_lma + byte_offset, args._file_lma + file->_size);
    session.cmd_flash_write(args._file_lma + byte_offset, data, bytes_to_copy);

    // Increment byte offset
    byte_offset += bytes_to_copy;

    // Wait for write in progress bit to clear again
    do {
      usleep(1'000);
    } while (session.flash_busy());
  }
  fprintf(stderr, "\n");

  // If it wasn't disabled, perform a re-read of the flash to verify
  if (args._verify_programmed) {
    for (unsigned byte_offset = 0; byte_offset < file->_size;) {
      uint8_t data[32];
      size_t bytes_to_copy = ((long)sizeof(data)) < (file->_size - byte_offset)
                                 ? sizeof(data)
                                 : (file->_size - byte_offset);
      fprintf(stderr, "Reading block 0x%012" PRIx32 " / 0x%012lx\r",
              args._file_lma + byte_offset, args._file_lma + file->_size);
      session.cmd_flash_read(args._file_lma + byte_offset, data, bytes_to_copy);

      // Compare the read block with the real bitstream
      if (memcmp(data, &file->_data[byte_offset], bytes_to_copy) != 0) {
        print_binary_diff(&file->_data[byte_offset], data, bytes_to_copy,
                          byte_offset);
        return EXIT_FAILURE;
      }

      // Increment byte offset
      byte_offset += bytes_to_copy;
    }
    fprintf(stderr, "\n");
  }

  // Release the FPGA
  session.cmd_fpga_reset_deassert();

  // Verify we have properly released
  if (session.fpga_is_under_reset()) {
    fprintf(stderr, "Failed to release FPGA reset\n");
    return EXIT_FAILURE;
  }

  // Idle led to low green
  session.cmd_set_rgb_led(0, 16, 0);

  return EXIT_SUCCESS;
}
