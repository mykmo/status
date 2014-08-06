#define _GNU_SOURCE
#include <sys/sysinfo.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <err.h>

static char SYSPATH[] = "/sys/class/power_supply/BAT0/";
static char charging[] = "↯";

static char *readline(char *dir, char *file)
{
	static char 	*line = NULL;
	static size_t 	nbytes = 0;

	char path[512];
	ssize_t n;
	FILE *fp;

	if (! dir && ! file) {
		free(line);
		return line = NULL;
	}

	snprintf(path, sizeof(path), "%s%s", dir, file);

	if (! (fp = fopen(path, "r"))) {
		warn("fopen %s", path);
		_exit(1);
	}

	if ((n = getline(&line, &nbytes, fp)) < 0) {
		warn("getline");
		_exit(1);
	}

	fclose(fp);

	assert(line);

	if (n && line[n - 1] == '\n')
		line[n - 1] = '\0';

	return line;
}

static void print_battery_status(void)
{
#define PRINT(...) str += snprintf(str, sizeof(status) - (str - status), __VA_ARGS__)

	static char status[512];

	char *str = status;

	if (! strcasecmp("charging", readline(SYSPATH, "status")))
		PRINT("%s ", charging);

	int capacity = atoi(readline(SYSPATH, "capacity"));

	if (capacity == 100)
		PRINT("full");
	else
		PRINT("%d%%", capacity);

	printf(", bat: %s", status);

	readline(NULL, NULL);
}

int main(int argc, char *argv[])
{
	struct sysinfo si;

	if (sysinfo(&si) < 0)
		return 1;

	printf("used: %ld", (si.totalram - si.freeram) * si.mem_unit >> 20);
	printf(", free: %ld", si.freeram * si.mem_unit >> 20);
#if 0
	printf(", swap: %d", (si.totalswap - si.freeswap) * si.mem_unit >> 10);
#endif
	printf(", procs: %d", si.procs);

	print_battery_status();

	printf("\n");

	return 0;
}
