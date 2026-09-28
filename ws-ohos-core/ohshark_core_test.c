/* Harness for the in-process engine.
 *
 * Usage: ohsharkcore_test [1|2|3]
 *   1  tshark -v
 *   2  tshark -r <device capture> -c 3 -T ek -x   (full dissection, NDJSON)
 *   3  tshark -D                                  (interface list)
 *
 * Each selection runs the engine exactly once, because upstream's main() is not
 * re-entrant: a second call in the same process segfaults inside epan_init.
 * The GUI therefore gives every engine run a fresh process image via fork().
 */
#include <stdio.h>
#include <stdlib.h>

int ohshark_run_tshark(int argc, char **argv, const char *out_path, const char *err_path);
int ohshark_list_interfaces(const char *out_path);
const char *ohshark_engine_version(void);

static const char *const kDir = "/data/local/tmp/ohcore";

static int run(int which)
{
    char out[256], err[256];
    snprintf(out, sizeof out, "%s/o%d.out", kDir, which);
    snprintf(err, sizeof err, "%s/o%d.err", kDir, which);

    switch (which) {
    case 1: {
        char *a[] = { (char *)"tshark", (char *)"-v", NULL };
        return ohshark_run_tshark(2, a, out, err);
    }
    case 2: {
        char *a[] = { (char *)"tshark", (char *)"-r",
                      (char *)"/data/local/tmp/ohshark/cap2.pcap",
                      (char *)"-c", (char *)"3", (char *)"-T", (char *)"ek",
                      (char *)"-x", NULL };
        return ohshark_run_tshark(8, a, out, err);
    }
    case 3: {
        char *a[] = { (char *)"tshark", (char *)"-D", NULL };
        return ohshark_run_tshark(2, a, out, err);
    }
    case 4:
        /* Interface list straight from libpcap: no dumpcap child, so no execve. */
        return ohshark_list_interfaces(out);
    }
    return -1;
}

int main(int argc, char **argv)
{
    const int which = argc > 1 ? atoi(argv[1]) : 2;
    printf("compiled-in engine version: %s\n", ohshark_engine_version());
    fflush(NULL);
    printf("run%d rc=%d\n", which, run(which));
    fflush(NULL);
    printf("DONE\n");
    return 0;
}
