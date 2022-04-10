#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <stdint.h>


// Error codes
// 1: Wrong user input
// 2: Failed to allocate memory
// 3: File not found
// 4: File corruption in read

// Aliases for primitive data types as described for MS RIFF standard
typedef uint8_t  BYTE;
typedef uint8_t  CHAR;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef int32_t  LONG;

// Format Chunk (ckID = "fmt "): The part of a wave file containing meta data
typedef struct FmtChunk
{
    WORD wFormatTag;        // Format category
    WORD nChannels;         // Number of channels
    DWORD nSamplesPerSec;   // Sampling rate
    DWORD nAvgBytesPerSec;  // For buffer estimation
    WORD nBlockAlign;       // Data block size
    WORD wBitsPerSample;    // Number of bits per sample
} FmtChunk;

// Data Chunk: The part of a wave file containing audio data
typedef struct DataChunk
{
    DWORD ckID;             // Chunk type identifier: "data"
    DWORD ckSize;           // Chunk size field (size of ckData)
    BYTE ckData[];          // Chunk data, flexible array member
} DataChunk;

// Store audio tracks in a linked list, like a playlist
typedef struct Track
{
    struct FmtChunk fmt;    // Meta data from Format Chunk
    DWORD fileSize;         // File size excluding RIFF header and size (8 Bytes)
    int track_number;       // Track number
    char *name;             // File name
    char *path;             // Full path and file name
    struct Track *pre;      // Pointer to previous track
    struct Track *next;     // Pointer to next track
}
Track;

// Prototypes
void print_welcome();
void print_help();
void delete(Track *track);

// Global variables
WORD outChannels;           // Number of channels
DWORD outSamplesPerSec;     // Sample rat
WORD outBlockAlign;         // Bit depth

int main(int argc, char **argv)
{
    // Welcome message
    print_welcome();

    // Define allowed command line flags
    int flag;
    char *flags = "hr:w:i:o:x:";
    char *rflag = "audio/";     // (r)ead source directory
    char *wflag = "medley.wav"; // (w)rite to output file
    int  iflag = 10;            // (i)n-marker in seconds
    int  oflag = 20;            // (o)ut-marker in seconds
    int  xflag = 2;             // (x)fade duration in seconds

    // Get and check user provided flags
    while ((flag = getopt(argc, argv, flags)) != -1)
    {
        switch (flag)
        {
            case 'h':
                print_help();
                return 0;

            case 'r':
                rflag = optarg;
                if (rflag[(strlen(rflag) - 1)] != '/')
                {
                    printf("\033[0;31m[ERROR]\033[0m Check your source directory: -r\nPath to source directory must end with '/'\nTo see the help page type ./medley -h\n\n");
                    return 1;
                }
                break;

            case 'w':
                wflag = optarg;
                break;

            case 'i':
                iflag = atoi(optarg);
                if (iflag <= 0)
                {
                    printf("\033[0;31m[ERROR]\033[0m Check your in-marker: -i (start in seconds)\nTo see the help page type ./medley -h\n\n");
                    return 1;
                }
                break;

            case 'o':
                oflag = atoi(optarg);
                if (oflag <= 0)
                {
                    printf("\033[0;31m[ERROR]\033[0m Check your out-marker: -o (end in seconds)\nTo see the help page type ./medley -h\n\n");
                    return 1;
                }
                break;

            case 'x':
                xflag = atoi(optarg);
                if (oflag <= 0)
                {
                    printf("\033[0;31m[ERROR]\033[0m Check your crossfade: -x (length in seconds)\nTo see the help page type ./medley -h\n\n");
                    return 1;
                }                
                break;

            case '?':
                printf("\033[0;31m[ERROR]\033[0m Wrong command line arguments found\nTo see the help page type ./medley -h\n\n");
                return 1;

            default:
                printf("\033[0;31m[ERROR]\033[0m Wrong command line arguments found.\nTo see the help page type ./medley -h\n\n");
                return 1;
        }
    }

    // Initialize playlist as head of track linked list
    Track *playlist = NULL;
    int track_count = 0;

    // Get pointer to source directory
    DIR *dir;
    dir = opendir (rflag);
    if (dir == NULL)
    {
        printf("\033[0;31m[ERROR]\033[0m Couldn't open the directory: %s\nTo see the help page type ./medley -h\n\n", rflag);
        return 1;
    }

    // Struct representing entry in directory
    struct dirent *direntry;

    // Read dir entry and move to next
    while ((direntry = readdir (dir)))
    {
        // Check for correct file extension
        char *ext = strrchr(direntry->d_name, '.');
        if (ext && (!strcmp(ext, ".wav") || !strcmp(ext, ".wave") || !strcmp(ext, ".bwf")))
        {
            // Add track to playlist
            track_count++;
            Track *new = malloc(sizeof(Track));
            if (new == NULL)
            {
                printf("\033[0;31m[ERROR]\033[0m Couldn't allocate memory for track number %i\nAbort! Let Martin know about this\n\n", track_count);
                return 2;
            }

            // Fill Track structure with data
            new->track_number = track_count;
            new->name = direntry->d_name;
            new->next = NULL;

            // Concat path
            new->path = calloc((strlen(rflag) + strlen(direntry->d_name) + 1), 1);
            if (new->path == NULL)
            {
                printf("\033[0;31m[ERROR]\033[0m Couldn't allocate memory for path to %s\nAbort! Let Martin know this\n\n", direntry->d_name);
                return 2;
            }
            new->path = strcat(new->path, rflag);
            new->path = strcat(new->path, direntry->d_name);
            new->path = strcat(new->path, "\0");


            // If the new track is the first track
            if (track_count == 1)
            {
                // Set playlist as head of list
                playlist = new;
            }
            else
            {
                // Set *track of previous track to this track
                Track *search = playlist;       // Search points to head of list (playlist)
                while (search->next != NULL)    // (I will add any new track to the end of the playlist)
                {                               // As long as the search track has a next track ...
                    search = search->next;      // ... move the search pointer to the next
                }                               // When done, search points to previous track
                search->next = new;             // Set preview track next pointer to the new track
            }
        }
    }

    // Create output track
    Track *output = malloc(sizeof(Track));
    // Set name to NULL as marker for unset
    output->name = NULL;

    // Play (aka loop) playlist, start at track number 1
    Track *play = playlist;
    while (play != NULL)
    {
        // STATUS PRINT 1: ID and name
        printf("No %i - %s ", play->track_number, play->name);

        // Open audiofile
        FILE *audiofile = fopen(play->path, "r");
        if (audiofile == NULL)
        {
            printf("\033[0;31m[ERROR]\033[0m Could not open file at %s\n", play->path);
            play = play->next;
            continue;
        }

        // Read and check file header
        // http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/Docs/riffmci.pdf

        // Check groupID for "RIFF"
        char groupID[5];
        groupID[4] = '\0';
        fread(&groupID, sizeof(char), 4, audiofile);
        if (strcmp(groupID, "RIFF\0") != 0)
        {
            printf("\033[0;33m[SKIPPED]\033[0m Only RIFF Files are supported\n");
            fclose(audiofile);
            play = play->next;
            continue;
        }
        // Get fileSize and store to Track (chunkSize = file length in bytes - 8)
        uint32_t fileSize;
        fread(&fileSize, sizeof(uint32_t), 1, audiofile);
        play->fileSize = fileSize;

        // Check riffType for "WAVE"
        char riffType[5];
        riffType[4] = '\0';
        fread(&riffType, sizeof(char), 4, audiofile);
        if (strcmp(riffType, "WAVE\0") != 0)
        {
            printf("\033[0;33m[SKIPPED]\033[0m Only PCM Files are supported\n");
            fclose(audiofile);
            play = play->next;
            continue;
        }

        // Keep reading chunks (ckID, chSize) from file to retrieve:
        // 1. Format chunk (mandatory first chunk after RIFF header)
        // 2. Data chunk, where the actual audio is stored
        // Ignore and skip all other chunks, e.g. relevant for BWF (BEXT Chunk)
        // Ignore and skip any padding zeros
        int skipFlag = 0;
        do
        {
            // Check for reaching End Of File
            if (feof(audiofile))
            {
                printf("\033[0;33m[SKIPPED]\033[0m No audio data found, file corruption\n");
                skipFlag = 1;
                continue;
            }

            // Skip NUL characters (padding)
            char check;
            fread(&check, sizeof(CHAR), 1, audiofile);
            if (check == 0)
            {
                continue;
            }
            // Rewind last fread (1 char) and continue
            else
            {
                fseek(audiofile, -1, SEEK_CUR);
            }

            // Read chunkID (4 letter code + terminator)
            char ckID[5];
            ckID[4] = '\0';
            fread(&ckID, sizeof(DWORD), 1, audiofile);

            // Read chunkSize
            DWORD ckSize;
            fread(&ckSize, sizeof(DWORD), 1, audiofile);

            // Read format chunk, mandatory first chunk after RIFF header
            if (strcmp(ckID, "fmt \0") == 0)
            {
                fread(&play->fmt, sizeof(FmtChunk), 1, audiofile);
                if (play->fmt.wBitsPerSample == 32)
                {
                    printf("\033[0;33m[SKIPPED]\033[0m 32 Bit floating point not supported\n");
                    skipFlag = 1;
                    continue;
                }
            }
            // Explicitly reject RF64 BWF (encountered ds64 chunk)
            else if (strcmp(ckID, "ds64\0") == 0)
            {
                printf("\033[0;33m[SKIPPED]\033[0m RF64 BWF is not supported\n");
                skipFlag = 1;
                continue;
            }
            
            // Skip all other chunks
            else if (strcmp(ckID, "data\0") != 0)
            {
                // Advance file pointer by chunkSize
                fseek(audiofile, ckSize, SEEK_CUR);
            }
            else
            {
                // printf("Samples start at %lu\n", ftell(audiofile));
                break;
            }
        } while (skipFlag == 0);

        // Skip track if flagged for skip
        if (skipFlag == 1)
        {
            skipFlag = 0;
            fclose(audiofile);
            play = play->next;
            continue;
        }

        // Copy meta data from first valid track (master) to output track
        if (output->name == NULL)
        {
            output->name = wflag;
            output->fmt = play->fmt;
        }
        // Check if all other tracks follow meta data of output track
        else if (output->fmt.nChannels != play->fmt.nChannels)
        {
            printf("\033[0;33m[SKIPPED]\033[0m Channel count (%hu Ch) does not match output track (%hu Ch)\n", play->fmt.nChannels, output->fmt.nChannels);
            play = play->next;
            continue;
        }
        else if (output->fmt.nSamplesPerSec != play->fmt.nSamplesPerSec)
        {
            printf("\033[0;33m[SKIPPED]\033[0m Samplerate (%u Hz) does not match output track (%u Hz)\n", play->fmt.nSamplesPerSec, output->fmt.nSamplesPerSec);
            play = play->next;
            continue;
        }
        else if (output->fmt.wBitsPerSample != play->fmt.wBitsPerSample)
        {
            printf("\033[0;33m[SKIPPED]\033[0m Bit depth (%hu bit) does not match output track (%hu bit)\n", play->fmt.wBitsPerSample, output->fmt.wBitsPerSample);
            play = play->next;
            continue;
        }


        // Start reading samples here! XXX

        // Close audiofile
        fclose(audiofile);

        // STATUS PRINT 2: Metadata
        printf("\033[0;32m(%hu Ch, %u Hz, %hu bit)\033[0m\n", play->fmt.nChannels, play->fmt.nSamplesPerSec, play->fmt.wBitsPerSample);

        // Move play pointer to next track
        play = play->next;
    }


    // Clear playlist and free memory
    delete(playlist);

    // Close directory
    closedir (dir);

    // Free output track
    free(output);
}


// Delete track from playlist
void delete(Track *track)
{
    // Recursive call if this is not the last track
    if (track->next != NULL)
    {
        delete(track->next);
    }

    // Free the malloc'ed path string
    free(track->path);

    // Free the Track struct
    free(track);
}


// Print welcome message
void print_welcome()
{
    printf("\n.---------------.\n");
    printf(  "| \033[0;31mm\033[0;32me\033[0;34md\033[0;36ml\033[0;35me\033[0;33my\033[0m v1.0.0 |\n");
    printf(  "'---------------'\n");
    printf(  "   by Martin Ulm\n\n");
    return;
}


// Print help page
void print_help()
{
    printf("This is the help\n");
    return;
}