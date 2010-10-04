#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>
using namespace std;

struct history {
	struct {
		unsigned short hour[24][2];
		unsigned char year, mon;
		unsigned short footer[15];
	} day[32];

	struct {
		unsigned short total[2];
	} date[16][32];
	unsigned char year[16][32];
};

int eod(int year, int mon)
{
	const int eod[] = {/*12*/31, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	return mon == 2 ? eod[2] + (year%4 == 0 && year%100 != 0 || year%400 == 0) : eod[mon];
}

int main(int argc, char *argv[])
{
	struct history hist;
	struct stat st;

	int ret = stat("mem.dat", &st); 
	if (ret < 0) {
		perror("mem.dat");
		return 1;
	}
	struct tm *t = localtime(&st.st_mtime);
	int year = t->tm_year+1900;
	int mon = t->tm_mon+1;
	int day = t->tm_mday;
	printf("#mem.dat mod time: %4d/%02d/%02d %2d:%02d:%02d\n",
	       year, mon, day, t->tm_hour, t->tm_min, t->tm_sec);

	FILE *f = fopen("mem.dat", "r");
	if (!f) {
		perror("fopen");
		return 1;
	}
	fread(&hist, sizeof(hist), 1, f);
	fclose(f);

	printf("\n# HOURLY LOG\n");
	int m = mon-1;
	for (int d = day+1 > eod(year,mon-1) ? 1 : day+1;
	     m != mon || d <= day;
	     d = d+1 > eod(year,mon-1) ? 1 : d+1) {
		if (d == 1)
			m++;
		int m2 = !m ? 12 : m;
		if (hist.day[d].mon != m2)
			continue;
		int wsum = 0, rsum = 0;
		for (int i = 0; i < 24; i++) {
			int walk = ntohs(hist.day[d].hour[i][0]);
			int run  = ntohs(hist.day[d].hour[i][1]);
			printf("%4d/%02d/%02d %02d: %5d %5d %5d\n", year, m2, d, i, walk, run, walk+run);
			wsum += walk;
			rsum += run;
		}
		printf("# %4d/%02d/%02d total: %6d %6d %6d\n", year, m2, d, wsum, rsum, wsum+rsum);
	}

	printf("\n\n# DAILY LOG\n");
	int y = year-1;
	for (int m = mon; y != year || m <= mon; m = m+1 == 13 ? 1 : m+1) {
		if (m == 1)
			y++;
		for (int d = 1; d <= eod(y,m); d++) {
			if (hist.year[m][d] != y-2000)
				continue;
			int walk = ntohs(hist.date[m][d].total[0]);
			int run  = ntohs(hist.date[m][d].total[1]);
			printf("%04d/%02d/%02d %5d %5d %5d\n", y, m, d, walk, run, walk+run);
		}
	}

	return 0;
}
