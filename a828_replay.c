#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <libusb-1.0/libusb.h>

#define VENDOR_ID    0x07ca
#define PRODUCT_ID   0xa828

void get_data(struct libusb_transfer *transfer)
{
	int i = 0;
	static int print_got_data = 1;
	
	FILE *fp = fopen("/tmp/out.ts", "a");
	for (i = 0; i < transfer->num_iso_packets; i++) {
		struct libusb_iso_packet_descriptor *descr = &transfer->iso_packet_desc[i]; 
		if (descr->actual_length == 0) {
			continue;
		}
		if (descr->status != LIBUSB_TRANSFER_COMPLETED) {
			printf("get_data transfer was not completed\n");
			continue;
		}
		
		if (print_got_data) {
			printf("Got stream\n");
			print_got_data = 0;
		}

		unsigned char *data = libusb_get_iso_packet_buffer_simple(transfer, i);
		fwrite(data, descr->actual_length, 1, fp);
	}
	fclose(fp);
}

int change_alt_setting(libusb_device_handle *dev_handle, int alt_setting)
{
	int rc = libusb_set_interface_alt_setting(dev_handle, 0, alt_setting);
	if (rc < 0) {
		printf("%s\n", libusb_strerror(rc));
		return -1;
	}

	rc = libusb_clear_halt(dev_handle, 1);
	if (rc < 0) {
		printf("%s\n", libusb_strerror(rc));
		return -1;
	}

	rc = libusb_clear_halt(dev_handle, 129);
	if (rc < 0) {
		printf("%s\n", libusb_strerror(rc));
		return -1;
	}
	return 0;
}

int process_loop(libusb_device_handle *dev_handle, char *filename, int alt_setting)
{
	FILE *fp = fopen(filename, "r");
	if (!fp) {
		printf("%m\n");
		return -1;
	}
	int rc = change_alt_setting(dev_handle, alt_setting);
	if (rc < 0) {
		return -1;
	}

	char line[2048];
	while (fgets(line, sizeof(line), fp)) {
		unsigned char data[1024];
		char *ind = line;
		int len = 0; int val = 0;
		if (strstr(line, "#"))
			continue;
		if (strstr(line, "CF") == line) {
			printf("CLEAR FEATURE\n");
			rc = libusb_clear_halt(dev_handle, 1);
			if (rc < 0) {
				printf("%s\n", libusb_strerror(rc));
				return -1;
			}

			rc = libusb_clear_halt(dev_handle, 129);
			if (rc < 0) {
				printf("%s\n", libusb_strerror(rc));
				return -1;
			}
			continue;
		}

		if (strchr(line, '[') == line) {
			double period;
			int ret = sscanf(line, "[%lf]", &period);
			if (ret == 1) {
				struct timespec tv = {
					.tv_sec = (time_t)period,
					.tv_nsec = (long)((period - (long)period)*1000000000)
				};
				printf("Sleep for %fs.\n", period);
				nanosleep(&tv, NULL);
			}
			continue;
		}
		
		while (sscanf(ind, "%02x", &val) == 1) {
			data[len++] = val;
			ind = ind + 3;
		}
		int i;
		printf(">> ");
		for (i = 0; i < len; i++)
			printf("%02x ", data[i]);
		printf("\n");
		int transferred = 0;
		int rc = libusb_bulk_transfer(dev_handle, 1, data, len, &transferred, 0);
		if (rc < 0) {
			printf("%s\n", libusb_strerror(rc));
			fclose(fp);
			return -1;
		}

		if (data[0] != 0xde && data[0] != 0xdc && data[0] != 0x36) {
			rc = libusb_interrupt_transfer(dev_handle, 0x81, data, sizeof(data), &transferred, 0);
			if (rc < 0) {
				printf("%s\n", libusb_strerror(rc));
				fclose(fp);
				return -1;
			}
			if (transferred > 0) {
				printf("<< ");
				int i;
				for (i = 0; i < transferred; i++)
					printf("%02x ", data[i]);
				printf("\n");
			}
		}
	}
	fclose(fp);
	return 0;
}

int main(void) {
	int ret = EXIT_SUCCESS;
	int rc = libusb_init(NULL);
	if (rc)
		return EXIT_FAILURE;

	libusb_set_debug(NULL, 3);
	libusb_device_handle *dev_handle = libusb_open_device_with_vid_pid(NULL, VENDOR_ID, PRODUCT_ID);
	if (dev_handle == NULL)
		return EXIT_FAILURE;

	rc = libusb_claim_interface(dev_handle, 0);
	if (rc < 0) {
		printf("%s\n", libusb_strerror(rc));
		ret = EXIT_FAILURE;
		goto exit;
	}

	rc = process_loop(dev_handle, "bulk_data_1.txt", 1);
	if (rc < 0) {
		printf("Processing loop failure for alt 1\n");
	}

	struct timespec tv = {
			.tv_sec = 1
	};
	nanosleep(&tv, NULL);
	
	rc = process_loop(dev_handle, "bulk_data_6.txt", 6);
	if (rc < 0) {
		printf("Processing loop failure for alt 6\n");
		libusb_release_interface(dev_handle, 0);
		ret = EXIT_FAILURE;
		goto exit;
	}

	unsigned char *endpoint_buf = calloc(1, 8192);
	if (!endpoint_buf) {
		printf("Memory allocation failure\n");
		libusb_release_interface(dev_handle, 0);
		ret = EXIT_FAILURE;
		goto exit;
	}

	int endpoint_buf_len = 8192;
	struct libusb_transfer *tf = libusb_alloc_transfer(8);
	if (!tf) {
		printf("Memory allocation failure\n");
		free(endpoint_buf);
		libusb_release_interface(dev_handle, 0);
		ret = EXIT_FAILURE;
		goto exit;
	}

	unlink("/tmp/out.ts");
	libusb_fill_iso_transfer(tf, dev_handle, 0x82, endpoint_buf, endpoint_buf_len,
				 8, get_data, NULL, 0);
	libusb_set_iso_packet_lengths(tf, 940);

	while (1) {
		rc = libusb_submit_transfer(tf);
		if (rc < 0) {
			printf("%s\n", libusb_strerror(rc));
			ret = EXIT_FAILURE;
			break;
		}
		libusb_handle_events(NULL);
	}

	libusb_free_transfer(tf);
	free(endpoint_buf);

	libusb_release_interface(dev_handle, 0);
exit:
	libusb_close(dev_handle);
	libusb_exit(NULL);
	return ret;
}
