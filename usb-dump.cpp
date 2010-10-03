#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <iostream>
#include <iomanip>
#include <libusb-1.0/libusb.h>
using namespace std;

#define VENDOR_ID 0x0507
#define PRODUCT_ID 0x0029

//#define DEBUG

struct dev_info {
	libusb_context *ctx;	
	libusb_device_handle *handle;
	uint8_t ep;
};

void block_signal(bool block)
{
	sigset_t sigset;  
	sigemptyset(&sigset);  
	sigaddset(&sigset, SIGINT);
	sigaddset(&sigset, SIGTERM);
	sigprocmask(block?SIG_BLOCK:SIG_UNBLOCK, &sigset, NULL);
}

void dump(unsigned char *msg, unsigned int addr, unsigned int size)
{
	for (int i = 0; i < size; i++) {
		if (i % 16 == 0)
			printf("%04x: ", addr + i);
		printf("%02x ", msg[i]);
		if (i % 16 == 15 || i == size - 1)
			printf("\n");
	}
	cout << endl;
}

void calc_chksum(unsigned char msg[8], int begin, int end)
{
	msg[end] = 0xff;
	for (int i = begin; i < end; i++)
		msg[end] ^= msg[i];
}

void make_msg(unsigned char msg[8], unsigned char bank,
	      unsigned short addr, unsigned char size, bool write=false)
{
	msg[0] = 0xe0;
	msg[1] = 0xa5;
	msg[2] = write ? 0x02 : 0x01;
	msg[3] = bank;
	msg[4] = (addr>>8) & 0xff;
	msg[5] = addr & 0xff;
	msg[6] = size;
	calc_chksum(msg, 2, 7);
}

void write_status(dev_info *d, unsigned char status[7])
{
	unsigned char msg[8]; // {0xe0, 0xa5, 0x02, 0x00, 0xf7, 0x41, 0x07, 0x4c};
	make_msg(msg, 0, 0xf741, 7, true);
	block_signal(true);
	int ret = libusb_control_transfer(d->handle, 0x21, 0x09, 0x0200, 0x0000, msg, sizeof(msg), 2000);
	if (ret < 0)
		throw "libusb_control_transfer failed";

	int actual;
	ret = libusb_bulk_transfer(d->handle, d->ep, msg, sizeof(msg), &actual, 2000);
	block_signal(false);
	if (ret < 0)
		throw "Read error";
#ifdef DEBUG2
	cerr << ">> "; dump(msg, 0, 8);
#endif
	if (msg[0] != 0xe0 || msg[1] != 0xa5 || msg[2] != 0x03)
		throw "Invalid response on write";

	memcpy(msg, status, 7);
	calc_chksum(msg, 0, 7);
	block_signal(true);
	ret = libusb_control_transfer(d->handle, 0x21, 0x09, 0x0200, 0x0000, msg, sizeof(msg), 2000);
	if (ret < 0)
		throw "libusb_control_transfer failed";

	ret = libusb_bulk_transfer(d->handle, d->ep, msg, sizeof(msg), &actual, 2000);
	block_signal(false);
	if (ret < 0)
		throw "Read error";
#ifdef DEBUG2
	cerr << ">> "; dump(msg, 0, 8);
#endif
	if (msg[0] != 0xe0 || msg[1] != 0xa5 || msg[2] != 0x02)
		throw "Invalid response after write";
}

void lcd_test(dev_info *d)
{
	unsigned char status[7];
	memset(status, 0, 7);
	for (int i = 0; i < 7; i++) {
		for (int j = 0; j < 8; j++) {
			status[i] |= 1 << j;
			write_status(d, status);
			usleep(100000);
		}
	}
}

void disp_num(dev_info *d, int num)
{
	unsigned char status[7] = {0xcc, 0x63, 0xcc, 0x63, 0xcc, 0, 0};
	const unsigned char dig[11] = {0x5f, 0x06, 0x3d, 0x2f, 0x66, 0x6b, 0x7b, 0x0e, 0x7f, 0x6f, 0};
	if (num >= 0 && num < 100) {
		int tens = (num / 10) % 10;
		tens = !tens ? 10 : tens;
		status[5] = dig[num % 10];
		status[6] = dig[tens];
	}
	write_status(d, status);
}

int read_addr(dev_info *d, unsigned char *buf, unsigned char bank,
	      unsigned short addr, unsigned char size)
{
#ifdef DEBUG
	cout << "== read data: " << bank << " " << hex <<  addr << " " << hex << (int)size << " ==" << endl;
#endif
	unsigned char msg[8];
	make_msg(msg, bank, addr, size);
	block_signal(true);
	int ret = libusb_control_transfer(d->handle, 0x21, 0x09, 0x0200, 0x0000, msg, sizeof(msg), 2000);
	if (ret < 0)
		throw "libusb_control_transfer failed";

	int loop = (size + 5 + 7)/8;
	unsigned char chk = 0xff;
	for (int i = 0; i < loop; i++) {
		int actual;
		ret = libusb_bulk_transfer(d->handle, d->ep, msg, sizeof(msg), &actual, 2000);
#ifdef DEBUG2
		cout << "ret is " << ret << " : actual is " << actual << endl;
#endif
		if (ret < 0)
			throw "Read error";
#ifdef DEBUG2
		cout << ">> "; dump(msg, 0, actual);
#endif
		if (actual != 8)
			throw "invalid response size";
		int begin = 0, begin_chk = 0, end = 8, end_chk = 8;
		if (i == 0) {
			if (msg[0] != 0xe0 || msg[1] != 0xa5 ||
			    msg[2] != 0x01 || msg[3] != size)
				throw "invalid response";
			begin = 4;
			begin_chk = 2;
		}
		if (i == loop-1) {
			end = (size + 4) % 8;
			end_chk = end + 1;
		}
		memcpy(buf + begin - 4 + i*8, msg + begin, end - begin);
		for (int j = begin_chk; j < end_chk; j++)
			chk ^= msg[j];
	}
	block_signal(false);
	if (chk)
		throw "Checksum not match";
#ifdef DEBUG
	dump(buf, addr, size);
#endif
}

void open_device(dev_info *d)
{
	int ret;
	d->ctx = NULL;
	ret = libusb_init(&d->ctx);
	if (ret < 0) 
		throw "libusb_init failed";
#if DEBUG
	libusb_set_debug(d->ctx, 3);
#endif
	d->handle = libusb_open_device_with_vid_pid(
		NULL, VENDOR_ID, PRODUCT_ID);
	if (!d->handle)
		throw "open failed";
	
	//Detach driver if kernel driver is attached.
	if (libusb_kernel_driver_active(d->handle, 0) == 1) { 
		cerr << "Kernel Driver Active" << endl;
		
		if(libusb_detach_kernel_driver(d->handle, 0) == 0) {
			cerr << "Kernel Driver Detached!" << endl;
			ret = true;
		} else {
			ret = false;
		}
	}

	//claim interface 0 (the first) of device (mine had jsut 1)
	ret = libusb_claim_interface(d->handle, 0); 
	//cout << "claim interface 0: " << ret << endl;
	if (ret < 0) 
		throw "Claim Interface failed";

	libusb_device *device = libusb_get_device(d->handle);
	libusb_config_descriptor *config;
	const libusb_interface *inter;
	const libusb_interface_descriptor *interdesc;
	const libusb_endpoint_descriptor *epdesc;

	libusb_get_config_descriptor(device, 0, &config);
	
	// cout << "find endpoint" << endl;
	for(int i = 0; i < config->bNumInterfaces; i++) {
		inter = &config->interface[i];
		//cout << "Number of alternate settings: "<< inter->num_altsetting << " | ";

		for(int j = 0; j < inter->num_altsetting; j++) {
			interdesc = &inter->altsetting[j];

			//cout << "Interface Number: " << (int) interdesc->bInterfaceNumber << " | ";
			//cout << "Number of endpoints: "<< (int) interdesc->bNumEndpoints << " | ";
			
			for(int k = 0; k < (int) interdesc->bNumEndpoints; k++) {
				epdesc = &interdesc->endpoint[k];
				//cout << "Descriptor Type: " << (int) epdesc->bDescriptorType << " | ";
				d->ep = epdesc->bEndpointAddress;
				//cout <<"EP Address: " << (int) d->ep << " | " << endl;
				break;
			}
		}
	}
}

void close_device(dev_info *d)
{
	if (d->handle) {
		int ret = libusb_release_interface(d->handle, 0); //release the claimed interface
		if(ret != 0)
			cerr << "Cannot Release Interface" << endl;
		libusb_close(d->handle);
	}
	if (d->ctx)
		libusb_exit(d->ctx);
}


void read_mem(dev_info *d)
{
	static unsigned char mem[0x8000];
	for (unsigned int addr = 0; addr < sizeof(mem); addr+=0x3b) {
		int pcnt = addr*100/sizeof(mem), last = -1;
		cerr << "reading " << hex << addr << " " << dec << pcnt << "%\r";
		int len = addr + 0x3b > sizeof(mem) ? sizeof(mem) - addr : 0x3b;
		read_addr(d, mem + addr, 1, addr, len);
		if (pcnt != last) {
			disp_num(d, pcnt);
			last = pcnt;
		}
	}
	disp_num(d, 100);
	dump(mem, 0, sizeof(mem));
	FILE *f = fopen("mem.dat", "w");
	if (!f) {
		perror("fopen");
		return;
	}
	fwrite(mem, sizeof(mem), 1, f);
	fclose(f);
}

void read_clock(dev_info *d)
{
	unsigned char buf[6];
	read_addr(d, buf, 0, 0xfb80, 6);
	printf("%4d/%02d/%02d %2d:%02d:%02d\n",
	       2000 + buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
}

int main(int argc, char **argv)
{
	dev_info d;

	try {
		open_device(&d);
		read_clock(&d);
		//lcd_test(&d);
		read_mem(&d);
	} catch (const char* err) {
		block_signal(false);
		cerr << err << endl;
	}

	try {
		close_device(&d);
	} catch (const char* err) {
		cerr << err << endl;
	}

	//cout << "Done." << endl;
	return 0;
}
