#include <stdio.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

struct MemoryStruct {
    char *memory;
    size_t size;
};

struct Playlist {
    char *url;
    char *name;
    int resolution;
    struct Playlist *next;
};

extern int errno;

static size_t

/*
 * This method writes content of packet to the memory
 * I took it from examples of curl
 */
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *) userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) {
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}


int StartsWith(const char *pre, const char *str) {
    return strncmp(pre, str, strlen(pre)) == 0;
}

/*
 * This method checks that if a string ends with a specific suffix.
 * I got it from stackoverflow
 */
int EndsWith(const char *str, const char *suffix) {
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

//returns line count of playlist
int line_count(char *memory, int size) {
    int result = 0;
   // if(size == 0){

    //}
    for (int i = 0; i < size; i++) {
        if (memory[i] == '\n') {
            result++;
        }
    }
    if(memory[size-1] != '\n'){
        result++;
    }

    return result;
}

void convert_to_lines(char *memory, char **result) {
    int i = 0;
    result[i] = strtok(memory, "\n");
    while (result[i] != NULL) {
        i++;
        result[i] = strtok(NULL, "\n");
    }
}

//this method deletes last word of url with replacing end of char ar.
void delete_end_of_url(char *url) {
    int i = strlen(url);
    while (i > 0) {
        if (url[i] == '/') {
            url[i + 1] = '\0';
            return;
        }
        i--;
    }
}

void create_url_with_name(char **target_url, char *url, char *name) {
    int url_length = strlen(url) + strlen(name);
    *target_url = (char *) malloc(sizeof(char) * url_length);
    strcpy(*target_url, url);
    strcat(*target_url, name);
}

/* This methods takes a line and returns resolution if this line is belongs to a playlist description
 * else it returns -1
 * example line: #EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=1322303,BANDWIDTH=1326366,CODECS="avc1.64001e,ec-3",RESOLUTION=768x432,FRAME-RATE=30.000,CLOSED-CAPTIONS="cc1",AUDIO="aud3",SUBTITLES="sub1"
 */
int get_resolution(char *line) {
    char delim[] = ":";
    char *line_definer = strtok(line, delim);
    if (strcmp(line_definer, "#EXT-X-STREAM-INF") == 0) {
        while (line_definer != NULL) {
            if (StartsWith("RESOLUTION=", line_definer)) {
                char *res = strtok(line_definer, "=");
                res = strtok(NULL, "x");
                int r1 = atoi(res);
                res = strtok(NULL, "x");
                int r2 = atoi(res);
                return r1 * r2;
            }
            line_definer = strtok(NULL, ",");
        }
    } else {
        return -1;
    }
    return -1;
}

/*
 * This method extracts playlists from master playlist.
 */
void find_playlists(char **result, int N, struct Playlist **head, char *url) {
    int i;
    delete_end_of_url(url);

    for (i = 0; i < N - 1; i++) {
        if (result[i] == NULL) {
            break;
        }
        int res = get_resolution(result[i]);
        if (res != -1) {
            struct Playlist *temp = (struct Playlist *) malloc(sizeof(struct Playlist));

            if (temp == NULL) {
                int errnum = errno;
                fprintf(stderr, "Value of errno: %d\n", errno);
                perror("Error printed by perror");
                fprintf(stderr, "Error allocating memory for playlist: %s\n", strerror(errnum));
            } else {
                temp->name = result[i + 1];
                temp->resolution = res;
                create_url_with_name(&temp->url, url, temp->name);
                temp->next = *head;
                *head = temp;
            }

        }
    }
    free(result);
}

/*
 * This method finds the first variant playlist with highest resolution in master playlist
 */
struct Playlist *find_target_playlist(struct Playlist *head) {
    struct Playlist *current = head;
    struct Playlist *target_playlist = head;
    while (current != NULL) {
        if (current->resolution >= target_playlist->resolution) {
            target_playlist = current;
        }
        current = current->next;
    }
    return target_playlist;
}


/*
 * This method gets pocket from given url.
 * I copied this method from examples of curl and modified.
 */
void get_with_curl(char *input_url, struct MemoryStruct *chunk) {
    printf("%s\n", input_url);
    CURL *curl_handle;
    CURLcode res;

    chunk->memory = malloc(1);  /* will be grown as needed by the realloc above */

    if (chunk->memory == NULL) {
        int errnum = errno;
        fprintf(stderr, "Value of errno: %d\n", errno);
        perror("Error printed by perror");
        fprintf(stderr, "Error allocating memory for playlist: %s\n", strerror(errnum));
    }

    chunk->size = 0;    /* no data at this point */

    curl_global_init(CURL_GLOBAL_ALL);

    /* init the curl session */
    curl_handle = curl_easy_init();

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, input_url);

    /* send all data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, chunk);

    /* get it! */
    res = curl_easy_perform(curl_handle);

    /* check for errors */
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
    } else {
        /*
         * Now, our chunk.memory points to a memory block that is chunk.size
         * bytes big and contains the remote file.
         *
         * Do something nice with it!
         */

        printf("%lu bytes retrieved\n", (unsigned long) chunk->size);
    }
    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);
    /* we're done with libcurl, so clean it up */
    curl_global_cleanup();
}

/*
 * After getting last target this methods gets all pockets and writes them to a single file
 * For this assignment I assumed all files will .ts files and files are ordered in playlist.
 */
void download_files(char **playlist, char *output_filename, char *url) {
    int i = 0;
    FILE *f;
    f = fopen(output_filename, "a");

    if (f == NULL) {
        int errnum = errno;
        fprintf(stderr, "Value of errno: %d\n", errno);
        perror("Error printed by perror");
        fprintf(stderr, "Error opening file: %s\n", strerror(errnum));
    } else {
        delete_end_of_url(url);
        while (playlist[i] != NULL) {
            if (EndsWith(playlist[i], ".ts")) {
                struct MemoryStruct chunk;
                char *target_url;
                create_url_with_name(&target_url, url, playlist[i]);
                get_with_curl(target_url, &chunk);

                for (int i = 0; i < chunk.size; i++) {
                    fwrite(&(chunk.memory[i]), 1, sizeof(chunk.memory[i]), f);
                }
                free(chunk.memory);
            }
            i++;
        }
        fclose(f);
    }
    free(playlist);
}

struct Playlist *read_master_playlist(char *input_url) {
    struct MemoryStruct chunk;
    struct Playlist *playlist_head = NULL;

    get_with_curl(input_url, &chunk);
    if (chunk.size == 0) {
        printf("Couldn't read anything from url\n");
        return NULL;
    } else {
        int N = line_count(chunk.memory, chunk.size);
        char **master_playlist = (char **) malloc(sizeof(char *) * N);

        if (master_playlist == NULL) {
            int errnum = errno;
            fprintf(stderr, "Value of errno: %d\n", errno);
            perror("Error printed by perror");
            fprintf(stderr, "Error allocating memory for master playlist: %s\n", strerror(errnum));
        }

        convert_to_lines(chunk.memory, master_playlist);
        find_playlists(master_playlist, N, &playlist_head, input_url);
        free(chunk.memory);

        return find_target_playlist(playlist_head);
    }
}

void read_target_playlist(char *url, char *output_filename) {
    struct MemoryStruct chunk;
    get_with_curl(url, &chunk);

    int N = line_count(chunk.memory, chunk.size);
    char **playlist = (char **) malloc(sizeof(char *) * N);
    if (playlist == NULL) {
        int errnum = errno;
        fprintf(stderr, "Value of errno: %d\n", errno);
        perror("Error printed by perror");
        fprintf(stderr, "Error allocating memory for playlist: %s\n", strerror(errnum));
    }
    convert_to_lines(chunk.memory, playlist);
    download_files(playlist, output_filename, url);
}

void validate_arguments(int argc, char **argv, char **input_url, char **output_filename) {
    char ch;
    int index = 0;
    int url_length = 2000; // maximum url which supported by browsers
    int filename_length = 255; // maximum filename length is 255 byte at linux
    if (argc < 3) {
        *input_url = (char *) malloc(sizeof(char) * url_length);
        *output_filename = (char *) malloc(sizeof(char) * filename_length);

        if (input_url == NULL || output_filename == NULL) {
            int errnum = errno;
            fprintf(stderr, "Value of errno: %d\n", errno);
            perror("Error printed by perror");
            fprintf(stderr, "Error allocating memory for arguments: %s\n", strerror(errnum));
        } else {
            printf("Please give link and output file correctly\n");
            printf("Please provide the master playlist's url:\n");
            while((ch=getc(stdin)) &&  ch != '\n' && ch != ' ') {
                if(index == url_length) {
                    url_length *= 2;
                    *input_url = realloc(*input_url, sizeof(char)* url_length);
                }
                *input_url[index++] = ch;
            }

            printf("Please provide output file name:\n");
            scanf("%255s", *output_filename);
        }
    } else {
        *input_url = argv[1];
        *output_filename = argv[2];
    }

}

int main(int argc, char **argv) {
    char *input_url;
    char *output_filename;
    validate_arguments(argc, argv, &input_url, &output_filename);

    struct Playlist *target_playlist = read_master_playlist(input_url);
    if (target_playlist != NULL) {
        read_target_playlist(target_playlist->url, output_filename);
    } else {
        printf("Playlist couldn't found\n");
    }
    return 0;
}
