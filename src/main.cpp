#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
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

struct BitstreamFile {
  BitstreamFile(void *data, off_t size) : _data(data), _size(size) {}
  void *_data;
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
  return std::make_unique<BitstreamFile>(mmapped_data, file_size);
}

int main(int argc, char **argv) {
  CliArgs args;
  args.parse(argc, argv);

  // If help was specified, just print that and exit
  if (args._help_selected) {
    args.usage();
    return EXIT_SUCCESS;
  }

  // If we didn't short circuit for help, and the args are invalid, error out
  if (!args.valid()) {
    return EXIT_FAILURE;
  }

  // Try and open the file we're trying to program
  std::unique_ptr<BitstreamFile> file = open_bitstream(args._file_path);
  if (file == nullptr) {
    fprintf(stderr, "Failed to open bitstream file '%s'\n", args._file_path);
  }

  // Attempt to init libusb
  if (libusb_init(NULL) < 0) {
    fprintf(stderr, "Failed to initialize libusb\n");
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
    fprintf(stderr, "Failed to claim usb interface 0x%02x\n",
            args._usb_interface);
    return EXIT_FAILURE;
  }

  // Get the serial for this device
  std::string serial = get_serial_for_device(usb_handle);
  fprintf(stderr, "Claimed device %04x:%04x with serial %s\n", args._usb_vid,
          args._usb_pid, serial.c_str());

  // Wrap it in a protocol layer
  UsbProto::Session session(usb_handle, args);

  // Disable the target FPGA so that we can control the SPI flash
  session.cmd_fpga_reset_assert();
  session.cmd_set_rgb_led(0, 64, 0);

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
          "Flash chip mfgr: 0x%02x, Device ID: 0x%02x Unique ID: 0x%016llx\n",
          flash_mfgr, flash_device, flash_unique_id);

  // Release the FPGA
  // session.cmd_fpga_reset_deassert();

  // Verify we have properly released
  if (session.fpga_is_under_reset()) {
    fprintf(stderr, "Failed to release FPGA reset\n");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
