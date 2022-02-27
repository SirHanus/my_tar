// #define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <locale.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#define UNUSED(x) ((void) x)

/** @struct meta_struct
 *  @brief Strukture for metadata
 * All ints are in base 8
 * All empty space is filled with null
 */
typedef struct meta_struct
{
    char filename[100];         /**< File/Folder name (0-100) */
    char mode[8];               /**< Access rights (100-108) */
    char owner_id[8];           /**< owner_ID (108-116) */
    char group_id[8];           /**< group_ID (116-124) */
    char size[12];              /**< File size (124-136) */
    char mtime[12];             /**< Last modification date (136-148)*/
    char checksum[8];           /**< Sum of metahead size for controll (148-156) */
    char sign_type[1];          /**< Item type (156-157) */
    char linked_file_name[100]; /**< Name of linked file(157-257) */
    char magic_number[6];       /**< Magic number (257-263) */
    char version[2];            /**< Version (263-265) */
    char owner_name[32];        /**< String owner name (265-297) */
    char group_name[32];        /**< String group name (297-329) */
    char major[8];              /**< Major order number (329-337) */
    char minor[8];              /**< Minor order number (337-345) */
    char prefix[155];           /**< Prefix of file/folder name (345-600) */
} meta_struct;

int get_k_bits(int number, int k)
{
    return (((1 << k) - 1) & (number));
}

void print_head(meta_struct *head)
{
    assert(head);
    printf("\n----------\n");
    printf("   filename: '%s'\n", head->filename);
    printf("   Mod: '%s'\n", head->mode);
    printf("   Uid: '%s'\n", head->owner_id);
    printf("   Gid: '%s'\n", head->group_id);
    printf("   Size: '%s'\n", head->size);
    printf("   Last_m: '%s'\n", head->mtime);
    printf("   Sum: '%s'\n", head->checksum);
    printf("   Type: '%c'\n", head->sign_type[0]);
    printf("   Link_name '%s'\n", head->linked_file_name);
    printf("   Magic: '%s'\n", head->magic_number);
    printf("   Version: '%c%c'\n", head->version[0], head->version[1]);
    printf("   O_name: '%s'\n", head->owner_name);
    printf("   G_name: '%s'\n", head->group_name);
    printf("   Major: '%s'\n", head->major);
    printf("   Minor: '%s'\n", head->minor);
    printf("   Prefix: '%s'\n", head->prefix);
    printf("----------\n\n");
}

void free_and_err(char *error, meta_struct *x, FILE *f)
{
    if (x != NULL) {
        free(x);
        x = NULL;
    }

    if (f != NULL) {
        fclose(f);
        f = NULL;
    }
    if (error != NULL) {
        fprintf(stderr, "%s", error);
    }
}

int sum_struct(meta_struct *meta)
{
    int sum = 256;
    char *met = (char *) meta;
    size_t length = sizeof(*meta);
    for (size_t i = 0; i < length; i++) {
        if (i >= 148 && i <= 155) {
            continue;
        }
        sum += met[i];
    }
    return sum;
}
void generate_prefix(char *prefix, meta_struct *x, char *name)
{
    bool skip_first = false;
    if (*name == '.') {
        skip_first = true;
    }
    char *str = strdup(name);
    char *token = strtok(str, "/");
    int curr_length = 1;
    while ((token != NULL)) {
        curr_length += strlen(token) + 1;
        if (!skip_first) {
            strcat(prefix, "/");
        }
        skip_first = false;
        strcat(prefix, token);
        if ((strlen(name) - curr_length) <= 99) {
            char *rest = strtok(NULL, "");
            sprintf(x->filename, "%s", rest);
            break;
        }
        token = strtok(NULL, "/");
    }

    free(str);
}

meta_struct *init_metadata(struct stat *stat, char *name, bool log)
{
    char prefix[155] = { '\0' };
    meta_struct *x = NULL;
    int y = posix_memalign((void **) &x, 512, 512);
    if (y != 0) {
        free_and_err("Memory alloc failed", NULL, NULL);
        return NULL;
    }
    memset(x, '\0', 512);
    if (strlen(name) < 99) {
        sprintf(x->filename, "%s", name);
    } else {
        generate_prefix(prefix, x, name);
        sprintf(x->prefix, "%s", prefix);
    }

    sprintf(x->mode, "%07o", get_k_bits(stat->st_mode, 9));
    sprintf(x->owner_id, "%07o", stat->st_uid); //if not uid not checked
    sprintf(x->group_id, "%07o", stat->st_gid);
    sprintf(x->size, "%011lo", (S_ISREG(stat->st_mode) ? stat->st_size : 0));
    sprintf(x->mtime, "%011lo", stat->st_mtim.tv_sec);
    sprintf(x->sign_type, "%c", (S_ISREG(stat->st_mode) ? '0' : '5'));
    sprintf(x->magic_number, "%s", "ustar");
    sprintf(x->version, "%c%c", '0', '0');
    struct passwd *pw = getpwuid(stat->st_uid);
    struct group *gr = getgrgid(stat->st_gid);
    sprintf(x->owner_name, "%s", (pw != NULL) ? pw->pw_name : "");
    sprintf(x->group_name, "%s", (gr != NULL) ? gr->gr_name : "");
    sprintf(x->major, "%07d", 0);
    sprintf(x->minor, "%07d", 0);
    sprintf(x->checksum, "%06o", sum_struct(x));
    x->checksum[7] = ' ';
    if (log) {
        print_head(x);
    }
    return x;
}

void append_footer(char *output)
{
    FILE *target = fopen(output, "ab");
    if (target == NULL) {
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < 1024; i++) {
        fputc('\0', target);
    }
    fclose(target);
}

void append_head(meta_struct *f, int target)
{
    char *met = (char *) f;
    write(target, met, 512);
}

bool append_file(meta_struct *f, char *output, char *path, bool only_head)
{
    int source, target;
    target = open(output, O_APPEND | O_CREAT | O_RDWR, 0666); //práva
    if (target == -1) {
        return false;
    }

    append_head(f, target);

    if (only_head) {
        close(target);
        return true;
    }
    source = open(path, O_RDONLY);
    if (source == -1) {
        close(target);
        return false;
    }

    int numr = 1;
    char buffer[512];
    int sum = 0;
    while (1) {
        numr = read(source, buffer, sizeof(buffer));
        if (numr <= 0) {
            break;
        }
        write(target, buffer, numr);
        sum += numr;
    }
    while (sum % 512 != 0) {
        write(target, "\0", 1);
        sum++;
    }
    close(source);
    close(target);

    return true;
}

typedef enum node_type
{
    data_file,
    data_folder,
    data_skip
} node_type;

node_type check_path_and_data(char *path, char *output_path, int *end_value, bool log)
{
    struct stat st;
    if (stat(path, &st) != 0) { //if link will check the file link points to
        fprintf(stderr, "Filedoesnotexist: %s", path);
        *end_value = 1;
        return data_skip;
    }

    if (S_ISDIR(st.st_mode)) {
        if (*(path + strlen(path) - 1) != '/') {
            strcat(path, "/");
        }
    }

    meta_struct *x = NULL;
    x = init_metadata(&st, path, log);
    if (S_ISREG(st.st_mode)) {
        if (!append_file(x, output_path, path, false)) {
            *end_value = 1;
        }
        free(x);
        return data_file;
    }
    if (S_ISDIR(st.st_mode)) {
        if (!append_file(x, output_path, NULL, true)) {
            *end_value = 1;
        }
        free(x);
        return data_folder;
    }
    free(x);
    return data_skip;
}

void recursion(char *base_path, char *output_path, bool has_v, int *end_value, bool log)
{
    if (strlen(base_path) > 248) {
        fprintf(stderr, "Path is too long");
        *end_value = 1;
        return;
    }
    struct dirent **namelist;
    int n = 0;

    n = scandir(base_path, &namelist, NULL, alphasort);
    if (n == -1) {
        fprintf(stderr, "Failed scandir %s\n", base_path);
        *end_value = 1;
        return;
    }
    for (int i = 0; i < n; i++) {
        char path[1000] = { '\0' };
        if (strcmp(namelist[i]->d_name, ".") != 0 && strcmp(namelist[i]->d_name, "..") != 0) {
            strcpy(path, base_path);

            if (*(base_path + strlen(base_path) - 1) != '/') {
                strcat(path, "/");
            }

            strcat(path, namelist[i]->d_name);

            if (has_v) {
                fprintf(stderr, "%s\n", path);
            }
            switch (check_path_and_data(path, output_path, end_value, log)) {
            case data_folder:
                recursion(path, output_path, has_v, end_value, log);
                break;

            case data_skip:
                continue;
                break;
            case data_file:
                break;
            }
        }
        free(namelist[i]);
    }
    free(namelist);
}

static int cmpstringp(const void *p1, const void *p2)
{
    return strcmp(*(char *const *) p1, *(char *const *) p2);
}

bool file_exists(char *filename)
{
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}
bool file_empty(char *filename)
{
    struct stat buffer;
    if (stat(filename, &buffer) != 0) {
        fprintf(stderr, "File_Empty: Stat failed\n");
        return false;
    }

    return (buffer.st_size <= 1);
}
bool file_only_null(char *file_path)
{
    FILE *f = fopen(file_path, "rb");
    if (f == NULL) {
        fprintf(stderr, "File opening failed\n");
        return false;
    }
    char c;
    while ((c = fgetc(f)) != EOF) {
        if (c != '\0') {
            fclose(f);
            return false;
        }
    }
    fclose(f);
    return true;
}

meta_struct *load_to_struct(char *str)
{
    meta_struct *x = NULL;
    int y = posix_memalign((void **) &x, 512, 512);
    if (y != 0) {
        free_and_err("Memory alloc failed", NULL, NULL);
        return NULL;
    }
    char *met = (char *) x;
    for (size_t i = 0; i < 512; i++) {
        met[i] = str[i];
    }
    return x;
}

int make_whole_path(char *file_path, mode_t mode)
{
    for (char *occurence = strchr(file_path + 1, '/'); occurence != NULL; occurence = strchr(occurence + 1, '/')) {
        *occurence = '\0';

        if (mkdir(file_path, mode) == -1) {
            if (errno != EEXIST) {
                *occurence = '/';
                return -1;
            }
        }
        *occurence = '/';
    }
    return 0;
}

bool check_string_only_null(char *str, int size)
{
    for (int i = 0; i < size; i++) {
        if (str[i] != '\0') {
            return false;
        }
    }
    return true;
}

bool file_unpacking(char *fullpath, char *buffer, struct utimbuf *new_times, meta_struct *x, FILE *f, mode_t mod_in_dec)
{
    if (file_exists(fullpath)) {
        fprintf(stderr, "Fileexists:");
        return false;
    }

    FILE *curr = fopen(fullpath, "wb");
    if (curr == NULL) {
        if (errno == EEXIST) {
            errno = 0;
            return 1;
        }
        if (errno == 2) {
            make_whole_path(fullpath, 511);
            curr = fopen(fullpath, "wb");
            if (curr == NULL) {
                return 1;
            }
        }
    }
    chmod(fullpath, mod_in_dec);
    int size_of_file = 0;
    size_of_file = strtol(x->size, NULL, 8);

    if (size_of_file != 0) {
        while (1) {
            if (512 != fread(buffer, sizeof(unsigned char), 512, f)) {
                fclose(curr);
                return false;
            }
            size_of_file -= 512;
            if (size_of_file <= 0) {
                break;
            }
            for (int i = 0; i < 512; i++) {
                fputc(buffer[i], curr);
            }
        }
        for (int i = 0; i < (512 + size_of_file); i++) {
            fputc(buffer[i], curr);
        }
    }

    fclose(curr);
    utime(fullpath, new_times);
    return true;
}

int unpack(char *tounpack, bool has_v, bool log)
{
    bool wasmade = false;
    FILE *f = fopen(tounpack, "rb");
    meta_struct *x = NULL;
    char buffer[512] = { '\0' };
    while (fread(buffer, sizeof(char), 512, f) == 512) {
        if (check_string_only_null(buffer, 512)) {
            continue;
        }
        if (wasmade) {
            free_and_err(NULL, x, NULL);
        }
        x = load_to_struct(buffer);
        if (x == NULL) {
            free_and_err("Failed to load struct\n", NULL, f);
            return 1;
        }
        wasmade = true;

        char fullpath[255] = { '\0' };
        strcpy(fullpath, x->prefix);
        strcat(fullpath, x->filename);

        int checksum_int = strtol(x->checksum, NULL, 8);
        if (sum_struct(x) != checksum_int) {
            free_and_err("Checksum was not matching\n", x, f);
            return 1;
        }

        if (log) {
            print_head(x);
        }

        if (has_v) {
            fprintf(stderr, "%s\n", fullpath);
        }

        int mod_in_dec = 0;
        struct utimbuf new_times = { .actime = 0, .modtime = 0 };
        if ((mod_in_dec = strtol(x->mode, NULL, 8)) == 0 || (new_times.modtime = strtol(x->mtime, NULL, 8)) == 0) {
            free_and_err("Failed cnv str to int\n", x, f);
            return 1;
        }
        if (x->sign_type[0] == '5') { //adresář

            if (mkdir(fullpath, mod_in_dec) == -1) {
                if (errno == EEXIST) {
                    errno = 0;
                    continue;
                }
                if (errno == 2) {
                    make_whole_path(fullpath, 511);
                    chmod(fullpath, mod_in_dec);
                    errno = 0;
                    continue;
                }
                free_and_err("Mkdir failed on directory\n", x, f);
                return 1;
            }
            utime(fullpath, &new_times);
        } else if (x->sign_type[0] == '0') { //soubor
            if (!file_unpacking(fullpath, buffer, &new_times, x, f, mod_in_dec)) {
                free_and_err("File unpacking failed\n", x, f);
                return 1;
            }
        } else {
            continue;
        }
    }
    free_and_err(NULL, x, f);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Not enough arguments. Options are 'x','c','v' and 'l'\n");
        return 1;
    }

    int end_value = 0;
    bool has_c = strchr(argv[1], 'c') != NULL;
    bool has_x = strchr(argv[1], 'x') != NULL;
    bool has_v = (strchr(argv[1], 'v') != NULL);
    bool log = (strchr(argv[1], 'l') != NULL);
    if ((has_c && has_x) || (!has_c && !has_x)) {
        fprintf(stderr, "Wrong arguments/argument combination\n");
        return 1;
    }
    if (has_c) {
        if (argc < 4) {
            fprintf(stderr, "Not a single file for packing\n");
            return 1;
        }
        char *output_path = argv[2];
        if (file_exists(output_path)) {
            fprintf(stderr, "Output file already exists\n");
            return 1;
        }
        qsort(&argv[3], argc - 3, sizeof(char *), cmpstringp);
        for (int i = 3; i < argc; i++) {
            if (has_v) {
                fprintf(stderr, "%s\n", argv[i]);
            }
            switch (check_path_and_data(argv[i], output_path, &end_value, log)) {
            case data_folder:
                recursion(argv[i], output_path, has_v, &end_value, false);
                break;
            case data_skip:
                continue;
                break;
            case data_file:
                break;
            }
        }

        append_footer(output_path);
        if (file_empty(argv[2]) || file_only_null(argv[2])) {
            remove(argv[2]);
            fprintf(stderr, "Created empty file\n");
            end_value = 1;
        }
        // } else {
        //     convert_output(argv[2]);
        // }
    } else {
        if (!file_exists(argv[2])) {
            free_and_err("File does not exist\n", NULL, NULL);
            return 1;
        }
        if (unpack(argv[2], has_v, log) != 0) {
            return 1;
        }
    }

    return end_value;
}