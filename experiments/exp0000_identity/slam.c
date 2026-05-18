/*
 * exp0000_identity — the contract reference implementation.
 *
 * Reads a config file with four keys:
 *   sequence_dir, output_path, timestamps_path, calib_path
 * Reads the timestamps file to count frames N.
 * Writes N lines of identity-pose (camera frame) to output_path.
 *
 * This is NOT a SLAM. Its only purpose is to demonstrate the harness
 * contract end-to-end: build, parse config, count frames, write valid
 * KITTI-format trajectory, exit 0.
 *
 * Scores ~100% trans error. Forking from here would be silly; fork from
 * whatever real SLAM is current best in results.tsv instead.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 4096

/* Find a key's value in a minimal YAML config: "key: value" per line.
 * Returns a freshly allocated string (caller frees) or NULL if not found.
 * Ignores quotes, trims whitespace. */
static char *yaml_get(const char *path, const char *key) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char line[MAX_LINE];
    size_t klen = strlen(key);
    char *result = NULL;
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, key, klen) != 0) continue;
        char *after = p + klen;
        while (*after == ' ' || *after == '\t') after++;
        if (*after != ':') continue;
        after++;
        while (*after == ' ' || *after == '\t' || *after == '"' || *after == '\'') after++;
        char *end = after + strlen(after);
        while (end > after && (end[-1] == '\n' || end[-1] == '\r' ||
                               end[-1] == ' '  || end[-1] == '\t' ||
                               end[-1] == '"'  || end[-1] == '\'')) end--;
        size_t len = (size_t)(end - after);
        result = (char *)malloc(len + 1);
        if (result) { memcpy(result, after, len); result[len] = '\0'; }
        break;
    }
    fclose(f);
    return result;
}

static long count_lines(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    long n = 0;
    int c, last = '\n';
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') n++;
        last = c;
    }
    if (last != '\n' && last != EOF) n++;
    fclose(f);
    return n;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <config_path>\n", argv[0]);
        return 1;
    }
    const char *cfg = argv[1];

    char *timestamps_path = yaml_get(cfg, "timestamps_path");
    char *output_path     = yaml_get(cfg, "output_path");
    if (!timestamps_path || !output_path) {
        fprintf(stderr, "config missing required keys (timestamps_path, output_path)\n");
        return 2;
    }

    long n = count_lines(timestamps_path);
    if (n <= 0) {
        fprintf(stderr, "could not read timestamps from %s\n", timestamps_path);
        return 3;
    }

    FILE *out = fopen(output_path, "w");
    if (!out) {
        fprintf(stderr, "could not open output %s\n", output_path);
        return 4;
    }

    /* Identity 3x4 row-major: [I | 0]. KITTI camera frame. */
    for (long i = 0; i < n; i++) {
        fprintf(out,
            "1 0 0 0 "
            "0 1 0 0 "
            "0 0 1 0\n");
    }
    fclose(out);

    free(timestamps_path);
    free(output_path);
    return 0;
}
