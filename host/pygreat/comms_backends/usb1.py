#
# This file is part of libgreat
#

"""
Module containing the definitions necessary to communicate with libgreat
devices over USB.
"""

from __future__ import absolute_import

import sys

import usb1
import time
import errno
import array
import atexit
import struct
import platform
import logging

from ..comms import CommsBackend
from ..errors import DeviceNotFoundError

class USB1CommsBackend(CommsBackend):
    """
    Class representing an abstract communications channel used to
    connect with a libgreat board.
    """

    """ Class variable that stores our global libusb library context. """
    context = None

    """ Class variable that stores any devices that still need to be closed at program exit. """
    device_handles = []

    # TODO this should be defined by the board!
    """ The maximum input/output buffer size for libgreat commands. """
    LIBGREAT_MAX_COMMAND_SIZE = 1024

    """ Bulk OUT endpoint address for libgreat interfaces. """
    LIBGREAT_BULK_OUT_ENDPOINT_ADDRESS = 0x02

    """ Bulk IN endpoint address for libgreat interfaces. """
    LIBGREAT_BULK_IN_ENDPOINT_ADDRESS  = 0x81

    """ Bulk OUT endpoint number for libgreat interfaces. """
    LIBGREAT_BULK_OUT_ENDPOINT_NUMBER  = LIBGREAT_BULK_OUT_ENDPOINT_ADDRESS

    """ Bulk IN endpoint number for libgreat interfaces. """
    LIBGREAT_BULK_IN_ENDPOINT_NUMBER   = LIBGREAT_BULK_IN_ENDPOINT_ADDRESS & 0x7f

    """ The configuration number for libgreat interfaces. """
    LIBGREAT_CONFIGURATION = 1

    """ The interface number for the libgreat command interface. """
    LIBGREAT_COMMAND_INTERFACE = 0

    """ The request number for issuing vendor-request encapsulated libgreat commands. """
    LIBGREAT_REQUEST_NUMBER = 0x65

    """
    Constant value provided to libgreat vendor requests to indicate that the command
    should execute normally.
    """
    LIBGREAT_VALUE_EXECUTE = 0

    """
    Constant value provided to the libgreat vendor requests indicating that the active
    command should be cancelled.
    """
    LIBGREAT_VALUE_CANCEL = 0xDEAD

    """
    Constant size of errors returned by libgreat commands on failure.
    """
    LIBGREAT_ERRNO_SIZE = 4

    """
    A flag passed to command execution that indicates we expect no response, and don't need to wait
    for anything more than the initial ACK.
    """
    LIBGREAT_FLAG_SKIP_RESPONSE = (1 << 0)

    """
    A flag passed to command execution that indicates that the host should re-use all of the in-arguments
    from a previous iteration. See execute_raw_command for documentation.
    """
    LIBGREAT_FLAG_REPEAT_LAST = (1 << 1)


    @classmethod
    def _get_libusb_context(cls):
        """ Retrieves the libusb context we'll use to fetch libusb device instances. """

        # If we don't have a libusb context, create one.
        if cls.context is None:
            cls.context = usb1.USBContext().__enter__()
            atexit.register(cls._destroy_libusb_context)

        return cls.context


    @classmethod
    def _destroy_libusb_context(cls):
        """ Destroys our libusb context on closing our python instance. """
        for handle in cls.device_handles:
            handle.close()
        cls.context.close()
        cls.context = None


    @classmethod
    def _device_matches_identifiers(cls, device, device_identifiers):
        def _getInterfaceSubClass(cls, device):
            for configuration in device:
                if configuration.getConfigurationValue() == cls.LIBGREAT_CONFIGURATION:
                    try:
                        interface = configuration[cls.LIBGREAT_COMMAND_INTERFACE]
                    except IndexError:
                        return None
                    return interface[0].getSubClass()
            return None

        def _getInterfaceProtocol(cls, device):
            for configuration in device:
                if configuration.getConfigurationValue() == cls.LIBGREAT_CONFIGURATION:
                    try:
                        interface = configuration[cls.LIBGREAT_COMMAND_INTERFACE]
                    except IndexError:
                        return None
                    return interface[0].getProtocol()
            return None

        property_fetchers = {
            'idVendor':           device.getVendorID,
            'idProduct':          device.getProductID,
            'serial_number':      device.getSerialNumber,
            'bus':                device.getBusNumber,
            'address':            device.getDeviceAddress,
            'interface_subclass': lambda: _getInterfaceSubClass(cls, device),
            'interface_protocol': lambda: _getInterfaceProtocol(cls, device),
         }

        # Check for a match on each of our properties.
        for identifier, fetcher in property_fetchers.items():

            # If we have a constraint on the given identifier, check to make sure
            # it matches our requested value...
            if identifier in device_identifiers:
                if device_identifiers[identifier] != fetcher():

                    # ... and return False if it doesn't.
                    return False

        # If we didn't fail to meet any constraints, return True.
        return True


    @classmethod
    def _find_device(cls, device_identifiers, find_all=False):
        """ Finds a USB device by its identifiers. See __init__ for their definitions. """

        matching_devices = []
        context = cls._get_libusb_context()


        # Search our list of devices until we find one that matches each of our identifiers.
        for device in context.getDeviceList():

            # If it matches all of our properties, add it to our list.
            if cls._device_matches_identifiers(device, device_identifiers):
                matching_devices.append(device)

        # If we have find_all, return all relevant devices...
        if find_all:
            return matching_devices

        # otherwise, return the first found device, or None if we haven't found any.
        elif matching_devices:
            return matching_devices[0]
        else:
            return None


    # TODO: handle providing board "URIs", like "usb;serial_number=0x123",
    # and automatic resolution to a backend?

    def __init__(self, **device_identifiers):
        """
        Instantiates a new comms connection to a libgreat device; by default connects
        to the first available board.

        Accepts the same arguments as pyusb's usb.find() method, allowing narrowing
        to a more specific board by serial number.
        """

        # The usb1.USBDevice
        self.device = None

        # The usb1.USBDeviceHandle
        self.device_handle = None

        # Zero pad serial numbers to 32 characters to match those
        # provided by the USB descriptors.
        if 'serial_number' in device_identifiers and len(device_identifiers['serial_number']) < 32:
            device_identifiers['serial_number'] = device_identifiers['serial_number'].zfill(32)

        # Connect to the device that matches our identifiers.
        self.device = self._find_device(device_identifiers)

        # If we couldn't find a board, bail out early.
        if self.device is None:
            raise DeviceNotFoundError()

        # Start off with no knowledge of the device's state.
        self._last_command_arguments = None
        self._have_exclusive_access = False

        # Run the parent initialization.
        super(USB1CommsBackend, self).__init__(**device_identifiers)


    def _hold_libgreat_interface(self, timeout=1000):
        """
        Ensures we have exclusive access to the USB interface used by libgreat.

        This is used internally both to get long-term exclusive access and to
        take temporary exclusive access (e.g. during a libgreat command).

        parameters:
            timeout -- the maximum wait time in ms
        """

        # If we already have exclusive access, there's no need to grab it again.
        if self._have_exclusive_access:
            return

        # If timeout is none, set it to a microsecond.
        if timeout is None:
            timeout = 0.001

        # Claim the first interface on the device, which we consider the standard
        # interface used by libgreat.
        timeout = time.time() + (timeout / 1000)

        # Try to open, configure, and claim until success or timeout.
        while True:
            try:
                # Open the device, and get a libusb device handle.
                self.device_handle = self.device.open()
                USB1CommsBackend.device_handles.append(self.device_handle)
            except (usb1.USBErrorAccess, usb1.USBErrorBusy):
                pass

            if self.device_handle:
                try:
                    if self.device_handle.getConfiguration() != self.LIBGREAT_CONFIGURATION:
                        self.device_handle.setConfiguration(self.LIBGREAT_CONFIGURATION)
                    self.device_handle.claimInterface(self.LIBGREAT_COMMAND_INTERFACE)
                    return
                except (usb1.USBErrorAccess, usb1.USBErrorBusy):
                    pass

            if time.time() > timeout:
                raise IOError("timed out trying to claim access to a libgreat device!")


    def _release_libgreat_interface(self, maintain_exclusivity=True):
        """
        Releases our hold on the USB interface used by libgreat, allowing others
        to use it.

        Parameters:
            maintain_exclusivity -- if true, we won't release an interface held for
                                    long-term exclusive access
        """

        # If we have exclusive access to the device, there's no need to release anything.
        if maintain_exclusivity and self._have_exclusive_access:
            return

        # Release our control over interface #0.
        self.device_handle.releaseInterface(self.LIBGREAT_COMMAND_INTERFACE)
        USB1CommsBackend.device_handles.remove(self.device_handle)
        self.device_handle.close()
        self.device_handle = None


    def get_exclusive_access(self):
        """
        Take (and hold) exclusive access to the device. Enables optimizations,
        as we can make assumptions base on our holding of the device.
        """

        self._hold_libgreat_interface()
        self._have_exclusive_access = True


    def release_exclusive_access(self):
         """
         Release exclusive access to the device.
         """

         if not self._have_exclusive_access:
             return

         self._release_libgreat_interface(maintain_exclusivity=False)
         self._have_exclusive_access = False


    @staticmethod
    def _build_command_prelude(class_number, verb):
        """Builds a libgreat command prelude, which identifies the command
        being executed to libgreat.
        """
        return struct.pack("<II", class_number, verb)


    def _check_for_repeat(self, class_number, verb, data):
        """ Check to see if the class_number, verb, and data are the same as the immediately
            preceeding call to this function. This is used to determine when we can perform a repeat-optimization,
            which is documented in execute_raw_command.
        """

        # Compile the in-arguments into a simple container.
        data_set = (class_number, verb, data,)

        # If this data set matches our most recent arguments,
        # we can use our repeat optimization!
        if data_set == self._last_command_arguments:
            return True

        # Otherwise, mark the data to be used and report that we can't.
        else:
            self._last_command_arguments = data_set
            return False


    def execute_raw_command(self, class_number, verb, data=None, timeout=1000, encoding=None,
                            max_response_length=4096, comms_timeout=1000, pretty_name="unknown",
                            rephrase_errors=True):

        """Executes a libgreat command.

        Args:
            class_number -- The class number for the given command.
                See the GreatFET wiki for a list of class numbers.
            verb -- The verb number for the given command.
                See the GreatFET wiki for the given class.
            data -- Data to be transmitted to the GreatFET.
            timeout -- Maximum command execution time, in ms.
            encoding -- If specified, the response data will attempt to be
                decoded in the provided format.
            max_response_length -- If less than 4096, this parameter will
                cut off the provided response at the given length.
            comms_timeout -- Maximum execution time for communications that do
                not directly execute the command.
            pretty_name -- String describing the RPC; used for error handling.
            rephrase_errors -- Allow exceptions to be intercepted and rephrased with more details.


        Returns any data received in response.
        """

        # Grab the libgreat interface, to ensure our libgreat transactions are atomic.
        self._hold_libgreat_interface()

        try:

            # Build the command header, which identifies the command to be executed.
            prelude = self._build_command_prelude(class_number, verb)

            # If we have data, build it into our request.
            if data:
                to_send = prelude + bytes(data)

                if len(to_send) > self.LIBGREAT_MAX_COMMAND_SIZE:
                    raise ValueError("Command payload is too long!")

            # Otherwise, just send the prelude.
            else:
                to_send = prelude

            # If our verb signature has no return value(s), make sure we will still check for an error return.
            if max_response_length == 0:
                max_response_length = 4

            # To save on the overall number of command transactions, the backend provides an optimization
            # that allows us to skip the "send" phase if the class, verb, and data are the same as the immediately
            # preceeding call. Check to see if we can use that optimization.
            use_repeat_optimization = False #self._have_exclusive_access and self._check_for_repeat(class_number, verb, data)

            # TODO: upgrade this to be able to not block?
            try:
                # If we're not using the repeat-optimization, send the in-arguments to the device.
                if not use_repeat_optimization:
                    flags = 0
                    self.device_handle.controlWrite(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE,
                        self.LIBGREAT_REQUEST_NUMBER, self.LIBGREAT_VALUE_EXECUTE, flags, to_send, timeout)

                # Set the FLAG_REPEAT_LAST if we're using our repeat-last optimization.
                flags = self.LIBGREAT_FLAG_REPEAT_LAST if use_repeat_optimization else 0

                # Truncate our maximum, if necessary.
                if max_response_length > 4096:
                    max_response_length = self.LIBGREAT_MAX_COMMAND_SIZE

                # ... and read any response the device has prepared for us.
                response = self.device_handle.controlRead(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE,
                    self.LIBGREAT_REQUEST_NUMBER, self.LIBGREAT_VALUE_EXECUTE, flags, max_response_length, comms_timeout)
                response = bytes(response)

                # If we were passed an encoding, attempt to decode the response data.
                if encoding and response:
                    response = response.decode(encoding, errors='ignore')

                # Return the device's response.
                return response

            except Exception as e:

                # Abort the command, and grab the last error number, if possible.
                error_number = self.abort_command()

                # If we got a pipe error, this indicates the device issued a realerror,
                # and we should convert this into a failed command error.
                is_signaled_error = isinstance(e, usb1.USBErrorPipe)

                # If this was an error raised on the device side, convert it to a CommandFailureError.
                if is_signaled_error and rephrase_errors:
                    raise self._exception_for_command_failure(error_number, pretty_name) from None
                else:
                    raise
        finally:

            # Always release the libgreat interface before we return.
            self._release_libgreat_interface()


    def abort_command(self, timeout=1000, retry_delay=1):
        """ Aborts execution of a current libgreat command. Used for error handling.

        Returns:
            the last error code returned by a command; only meaningful if
        """

        # Invalidate any existing knowledge of the device's state.
        self._last_command_arguments = None

        # Create a quick function to issue the abort request.
        execute_abort = lambda device : device.controlRead(
            usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE,
            self.LIBGREAT_REQUEST_NUMBER,
            self.LIBGREAT_VALUE_CANCEL,
            0,
            self.LIBGREAT_ERRNO_SIZE,
            timeout
        )

        # And try executing the abort progressively, multiple times.
        try:
            result = execute_abort(self.device_handle)
        except:
            if retry_delay:
                time.sleep(retry_delay)
                result = execute_abort(self.device_handle)
            else:
                raise

        # Parse the value returned from the request, which may be an error code.
        return struct.unpack("<I", result)[0]


    def still_connected(self):
        """ Attempts to detect if the device is still connected. Returns true iff it is. """

        USB_ERROR_NO_SUCH_DEVICE = 19

        try:
            self.device_handle.clearHalt(0)
            return True
        except usb1.USBErrorNotFound:
            return True
        except usb1.USBErrorNoDevice:
            return False


    def handle_events(self):
        """
        Performs any computations necessary to perform background processing of asynchronous communications.
        Should be called periodically when asynchronous transfers are used.
        """
        self._get_libusb_context().handleEvents()


    def close(self):
        """
        Dispose resources allocated by this connection.  This connection
        will no longer be usable.
        """
        if self.device_handle is not None:
            USB1CommsBackend.device_handles.remove(self.device_handle)
            self.device_handle.close()
            self.device_handle = None


    def __del__(self):
        """ Cleans up any hanging USB context before closing. """
        if USB1CommsBackend.context is not None and self.device_handle is not None:
            USB1CommsBackend.device_handles.remove(self.device_handle)
            self.device_handle.close()
            self.device_handle = None
