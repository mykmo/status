#define _GNU_SOURCE
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <err.h>

#define TOPMEMCOUNT 8
#define TOOLFONT "Terminus"

struct proc_entry {
	char *name;
	int rss;
};

static char SYSPATH[] = "/sys/class/power_supply/BAT0/";
static char charging[] = "↯";

static struct proc_entry *proc_entry_new(const char *name)
{
	struct proc_entry *e = calloc(1, sizeof(*e));

	e->name = strdup(name);

	return e;
}

static void proc_entry_free(struct proc_entry *e)
{
	free(e->name);
	free(e);
}

static int proc_entry_cmp(const void *a, const void *b)
{
#define PROC_ENTRY_CAST(pp) (*(struct proc_entry **) (pp))
	return PROC_ENTRY_CAST(b)->rss - PROC_ENTRY_CAST(a)->rss;
#undef PROC_ENTRY_CAST
}

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

static int isdir(const char *path)
{
	struct stat st;

	return ! stat(path, &st) && S_ISDIR(st.st_mode);
}

static void print_battery_status(void)
{
#define PRINT(...) str += snprintf(str, sizeof(status) - (str - status), __VA_ARGS__)

	static char status[512];

	char *str = status;

	if (! isdir(SYSPATH))
		return;

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

static bool is_proc_entry(struct dirent *de)
{
	if (de->d_type != DT_DIR)
		return false;

	for (int n = 0; de->d_name[n] != '\0'; n++) {
		if (!isdigit(de->d_name[n]))
			return false;
	}

	return true;
}

static char *get_proc_name(const char *pid)
{
	static char name[PATH_MAX];

	char path[PATH_MAX];
	ssize_t n;

	snprintf(path, sizeof(path), "/proc/%s/exe", pid);

	if ((n = readlink(path, name, sizeof(name) - 1)) <= 0)
		return NULL;

	name[n] = '\0';

	return name;
}

static int get_proc_rss(const char *pid)
{
	char path[PATH_MAX];
	FILE *fp;
	int rss = -1;

	char *buf = NULL;
	size_t buflen = 0;

	snprintf(path, sizeof(path), "/proc/%s/status", pid);

	fp = fopen(path, "r");
	if (!fp)
		return -1;

	while (getline(&buf, &buflen, fp) > 0) {
		int val;

		if (sscanf(buf, "VmRSS: %d kB", &val) == 1) {
			rss = val;
			break;
		}
	}

	fclose(fp);
	free(buf);

	return rss;
}

static int proc_entries_print(struct proc_entry **ee, int nelem, int upto, int width)
{
	int maxwidth = 0;
	char buf[512];

	if (upto > nelem)
		upto = nelem;

	for (int n = 0; n < upto; n++) {
		struct proc_entry *e = ee[n];
		char *s = strrchr(e->name, '/');

		if (s)
			s++;
		else
			s = e->name;

		snprintf(buf, sizeof(buf), "%s %d MB", s, e->rss);
		int len = strlen(buf);

		if (maxwidth < len)
			maxwidth = len;

		if (width) {
			if (n)
				printf("\n");

			printf("%s%*s %d MB", s, width - len, "", e->rss);
		}
	}

	if (!width)
		proc_entries_print(ee, nelem, upto, maxwidth);

	return maxwidth;
}

static void print_top_memory_usage(void)
{
	DIR *dir = opendir("/proc");
	struct dirent *de;

	struct proc_entry **entries = NULL;
	int nentry = 0;

	while ((de = readdir(dir)) != NULL) {
		const char *pid = de->d_name;

		if (!is_proc_entry(de))
			continue;

		const char *procname = get_proc_name(pid);
		if (!procname)
			continue;

		int rss = get_proc_rss(pid);
		if (rss < 0)
			continue;

		struct proc_entry *e = NULL;

		for (int n = 0; n < nentry; n++) {
			if (!strcmp(entries[n]->name, procname)) {
				e = entries[n];
				break;
			}
		}

		if (!e) {
			e = proc_entry_new(procname);
			entries = realloc(entries, sizeof(*entries) * (nentry + 1));
			entries[nentry++] = e;
		}

		e->rss += rss;
	}

	qsort(entries, nentry, sizeof(*entries), proc_entry_cmp);

	proc_entries_print(entries, nentry, TOPMEMCOUNT, 0);

	for (int n = 0; n < nentry; n++)
		proc_entry_free(entries[n]);

	free(entries);

	closedir(dir);
}

int main(int argc, char *argv[])
{
	struct sysinfo si;

	setuid(0);

	if (sysinfo(&si) < 0)
		return 1;

	printf("<txt>");

	printf("used: %ld", (si.totalram - si.freeram) * si.mem_unit >> 20);
	printf(", free: %ld", si.freeram * si.mem_unit >> 20);
#if 0
	printf(", swap: %d", (si.totalswap - si.freeswap) * si.mem_unit >> 10);
#endif
	printf(", procs: %d", si.procs);

	print_battery_status();

	printf("</txt>\n");

	printf("<tool><span font_family=\"" TOOLFONT "\">");

	print_top_memory_usage();

	printf("</span></tool>\n");

	return 0;
}
