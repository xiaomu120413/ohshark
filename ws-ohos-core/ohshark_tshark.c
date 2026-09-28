/* In-process tshark for HarmonyOS.
 *
 * HarmonyOS refuses execve() on app payloads: bundle libs are installed 0644
 * (so execve fails with EACCES) and a copy placed in the app's writable sandbox
 * fails with EPERM from code-signing enforcement. dlopen() of a library that
 * shipped inside the signed HAP *is* allowed, so tshark's main() is compiled
 * into libohsharkcore.so and invoked from the Qt app instead of executed.
 *
 * Two things have to be neutralised to call main() as a library entry point:
 *   - exit() would tear down the whole GUI process, so it is redirected to a
 *     siglongjmp back into ohshark_run_tshark().
 *   - output goes to stdout/stderr, so both are redirected to caller-supplied
 *     files for the duration of the run and restored afterwards. The GUI reads
 *     the NDJSON that tshark -T ek -x writes there.
 */

#include <fcntl.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pcap/pcap.h>

/* Declared before the macro so that <stdlib.h>'s own `void exit(int)` (already
 * included above) is not renamed; only call sites inside tshark.c are. */
void ohshark_exit(int code);

#define exit ohshark_exit
#define main ohshark_tshark_main
#include "../tshark.c"
#undef main
#undef exit

static sigjmp_buf ohshark_jmp;
static int ohshark_exit_code;

/* Wireshark builds with -fvisibility=hidden; these two are the library's ABI. */
#define OHSHARK_API __attribute__((visibility("default")))

void ohshark_exit(int code)
{
    ohshark_exit_code = code;
    siglongjmp(ohshark_jmp, 1);
}

/* Upstream version string, so the GUI can report it without spending an engine
 * run on `tshark -v`. */
OHSHARK_API const char *ohshark_engine_version(void)
{
    return VERSION;
}

static int redirect(int fd, const char *path)
{
    if (!path || !*path)
        return -1;
    int target = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (target < 0)
        return -1;
    int saved = dup(fd);
    if (saved < 0) {
        close(target);
        return -1;
    }
    dup2(target, fd);
    close(target);
    return saved;
}

OHSHARK_API int ohshark_run_tshark(int argc, char **argv, const char *out_path, const char *err_path)
{
    /* Anything the GUI has buffered must not land in the engine's output. */
    fflush(NULL);

    const int saved_out = redirect(STDOUT_FILENO, out_path);
    const int saved_err = redirect(STDERR_FILENO, err_path);
    if (saved_out < 0 || saved_err < 0) {
        if (saved_out >= 0)
            close(saved_out);
        if (saved_err >= 0)
            close(saved_err);
        return -1;
    }

    ohshark_exit_code = 0;
    if (sigsetjmp(ohshark_jmp, 1) == 0)
        ohshark_exit_code = ohshark_tshark_main(argc, argv);

    fflush(NULL);
    dup2(saved_out, STDOUT_FILENO);
    dup2(saved_err, STDERR_FILENO);
    close(saved_out);
    close(saved_err);
    return ohshark_exit_code;
}

/* `tshark -D` is not usable here: it spawns the dumpcap binary, and HarmonyOS
 * refuses execve() for app payloads. List interfaces through libpcap directly,
 * which is exactly what dumpcap does internally. Output format matches
 * `tshark -D`: "<n>. <name>\t<description>".
 */
OHSHARK_API int ohshark_list_interfaces(const char *out_path)
{
    pcap_if_t *alldevs = NULL;
    char errbuf[PCAP_ERRBUF_SIZE];

    if (pcap_findalldevs(&alldevs, errbuf) == -1) {
        int fd = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd >= 0) {
            ssize_t ignored = write(fd, errbuf, strlen(errbuf));
            (void)ignored;
            close(fd);
        }
        return -1;
    }

    FILE *f = fopen(out_path, "w");
    if (!f) {
        pcap_freealldevs(alldevs);
        return -1;
    }
    int n = 1;
    for (pcap_if_t *d = alldevs; d != NULL; d = d->next) {
        if (d->description)
            fprintf(f, "%d. %s\t%s\n", n, d->name, d->description);
        else
            fprintf(f, "%d. %s\n", n, d->name);
        ++n;
    }
    fclose(f);
    pcap_freealldevs(alldevs);
    return n - 1;
}
