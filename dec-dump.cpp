#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
using namespace std;

struct history
{
	unsigned short data[0x4000];
};

int main(int argc, char *argv[])
{
	struct history hist;

	FILE *f = fopen("mem.dat", "r");
	if (!f) {
		perror("fopen");
		exit(1);
	}
	fread(&hist, sizeof(hist), 1, f);
	fclose(f);

	for (int i = 0; i < sizeof(hist)/sizeof(short); i++) {
		if (i % 16 == 0)
			printf("%04x: ",i*sizeof(short));
		printf("%5d ", ntohs(hist.data[i]));
		if (i % 16 == 15)
			printf("\n");
	}

	return 0;
}
