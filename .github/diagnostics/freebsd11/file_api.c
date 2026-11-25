#include <sys/stat.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void seed(void) {
    int fd = open("regular", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    assert(fd >= 0 && write(fd, "sample", 6) == 6 && close(fd) == 0);
}

static void opened(const char *label, const char *path, int flags) {
    errno = 0;
    int fd = open(path, flags, 0600);
    int error = fd < 0 ? errno : 0;
    struct stat info;
    off_t size = stat(path, &info) == 0 && S_ISREG(info.st_mode) ? info.st_size : -1;
    int access = fd < 0 ? -1 : fcntl(fd, F_GETFL) & O_ACCMODE;
    printf("open case=%s fd_ok=%d errno=%d size=%jd access=%d\n", label, fd >= 0, error, (intmax_t)size, access);
    if (fd >= 0) assert(close(fd) == 0);
}

static void file_flags(const char *text, int shared) {
    char *buffer = strdup(text);
    assert(buffer != NULL);
    char *cursor = buffer;
    unsigned long set = ~0UL, clear = ~0UL;
    int rc = strtofflags(&cursor, &set, shared ? &set : &clear);
    printf("strtofflags input=%s shared=%d rc=%d set=%lx clear=%lx offset=%td\n",
           text, shared, rc, set, shared ? set : clear, cursor ? cursor - buffer : -1);
    free(buffer);
}

static clock_t child_ticks(const char *label) {
    struct tms t;
    assert(times(&t) != (clock_t)-1);
    printf("times stage=%s ticks_per_second=%ld child_user=%jd child_system=%jd\n",
           label, sysconf(_SC_CLK_TCK), (intmax_t)t.tms_cutime, (intmax_t)t.tms_cstime);
    return t.tms_cutime + t.tms_cstime;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    assert(geteuid() != 0);
    if (argc == 2 && strcmp(argv[1], "after-exec") == 0) {
        child_ticks("after-exec");
        return 0;
    }
    printf("reference uid=%d\n", (int)geteuid());
    char dir[] = "/tmp/anyps5-file-api.XXXXXX";
    assert(mkdtemp(dir) && chdir(dir) == 0);
    assert(mkdir("directory", 0700) == 0);
    seed();
    opened("plain-read", "regular", O_RDONLY);
    opened("shared-read", "regular", O_RDONLY | O_SHLOCK | O_NONBLOCK);
    opened("shared-read-trunc", "regular", O_RDONLY | O_TRUNC | O_SHLOCK | O_NONBLOCK);
    seed();
    opened("exclusive-read-trunc", "regular", O_RDONLY | O_TRUNC | O_EXLOCK | O_NONBLOCK);
    seed();
    int holder = open("regular", O_RDWR | O_EXLOCK);
    assert(holder >= 0);
    opened("contended-read-trunc", "regular", O_RDONLY | O_TRUNC | O_SHLOCK | O_NONBLOCK);
    assert(close(holder) == 0);
    opened("exclusive-writable-trunc", "regular", O_RDWR | O_TRUNC | O_EXLOCK | O_NONBLOCK);
    opened("directory-read-trunc", "directory", O_RDONLY | O_TRUNC | O_SHLOCK | O_NONBLOCK);
    opened("missing-read-trunc", "missing", O_RDONLY | O_TRUNC | O_SHLOCK | O_NONBLOCK);
    for (int shared = 0; shared <= 1; ++shared) {
        file_flags("uchg,nohidden", shared);
        file_flags("uchg,nohidden,unknown", shared);
    }
    child_ticks("initial");
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        clock_t start = clock();
        while (clock() - start < CLOCKS_PER_SEC / 3) {}
        _exit(0);
    }
    int status;
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    assert(child_ticks("after-wait") > 0);
    assert(unlink("regular") == 0 && rmdir("directory") == 0 && chdir("/tmp") == 0 && rmdir(dir) == 0);
    execl(argv[0], argv[0], "after-exec", (char *)NULL);
    perror("exec");
    return 1;
}
