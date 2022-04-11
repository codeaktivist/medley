// ----------------------------------------------------------
// m e l o d y   v 1 . 0 . 0 . 1
// martin.ulm@googlemail.com
// ----------------------------------------------------------


#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <stdint.h>



// ----------------------------------------------------------
// D E C L A R A T I O N S
// Types, structures, function prototypes, global variables
// ----------------------------------------------------------


// Aliases for primitive data types as described for MS RIFF standard
typedef uint8_t  BYTE;
typedef uint8_t  CHAR;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef int32_t  LONG;


// RIFF Chunk: File info
typedef struct RiffChunk
{
    DWORD ckID;             // Chunk type identifier: "RIFF"
    DWORD ckSize;           // Chunk size field: filesize - 8 Byte
    DWORD riffType;         // RIFF type: "WAVE"
} RiffChunk;


// Format Chunk: Meta data
typedef struct FmtChunk
{
    DWORD ckID;             // Chunk type identifier: "fmt "
    DWORD ckSize;           // Chunk size field: 16 Bytes for PCM
    WORD wFormatTag;        // Format category: 1 for PCM
    WORD nChannels;         // Number of channels
    DWORD nSamplesPerSec;   // Sampling rate
    DWORD nAvgBytesPerSec;  // Byterate: SampleRate * NumChannels * BitsPerSample / 8
    WORD nBlockAlign;       // Data block size: NumChannels * BitsPerSample/8
    WORD wBitsPerSample;    // Number of bits per sample
} FmtChunk;


// Data Chunk: Audio data
typedef struct DataChunk
{
    DWORD ckID;             // Chunk type identifier: "data"
    DWORD ckSize;           // Chunk size field: NumSamples * NumChannels * BitsPerSample/8
} DataChunk;


// Store audio tracks in a linked list, like a playlist
typedef struct Track
{
    struct RiffChunk riff;  // RIFF Chunk, file info
    struct FmtChunk fmt;    // Format Chunk, meta data
    struct DataChunk data;  // Data Chunk, audio data
    int track_number;       // Track number
    char *name;             // File name
    char *path;             // Full path incl. file name
    struct Track *next;     // Pointer to next track
}
Track;


// Prototypes
void print_welcome();
void print_help();
void delete(Track *track);


// Global variables
int sample_count;           // sample length of each track slice
int file_count = 0;         // audio files found in directory
int track_count = 0;        // count of valid tracks added to playlist


// ----------------------------------------------------------
// M A I N
// Program starts here
// ----------------------------------------------------------


int main(int argc, char **argv)
{
    // Welcome message
    print_welcome();



// ----------------------------------------------------------
// U S E R   I N P U T
// Retrieving and checking command line arguments
// ----------------------------------------------------------


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



// ----------------------------------------------------------
// T R A C K S   &   P L A Y L I S T
// Initializing output track and input playlist 
// ----------------------------------------------------------


    // Initialize playlist as head of track linked list
    Track *playlist = NULL;


    // Initialize output track
    Track *output = malloc(sizeof(Track));
    // Set name to NULL as marker for unset
    output->name = NULL;



// ----------------------------------------------------------
// D I R E C T O R Y   S T R E A M
// Reading directory to sorted singly linked list
// ----------------------------------------------------------


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
        // Check for correct file extension: .wav, .wave, .bfw
        char *ext = strrchr(direntry->d_name, '.');
        if (ext && (!strcmp(ext, ".wav") || !strcmp(ext, ".wave") || !strcmp(ext, ".bwf")))
        {
            // Add track to playlist
            file_count++;
            Track *new = malloc(sizeof(Track));
            if (new == NULL)
            {
                printf("\033[0;31m[ERROR]\033[0m Couldn't allocate memory for track number %i\nAbort! Let Martin know about this\n\n", file_count);
                return 2;
            }

            // Fill Track structure with data
            new->track_number = file_count;
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


            // Adjust next pointer
            if (file_count == 1)
            {
                // If this is the first file, set playlist (head of list) to point to it, no next
                playlist = new;
            }
            else
            {
                // Set *track of previous track to this track [XXX Implemens sorting via strcomp]
                Track *search = playlist;       // Search points to head of list (playlist)
                while (search->next != NULL)    // (I will add any new track to the end of the playlist)
                {                               // As long as the search track has a next track ...
                    search = search->next;      // ... move the search pointer to the next
                }                               // When done, search points to previous track
                search->next = new;             // Set preview track next pointer to the new track
            }
        }
    }



// ----------------------------------------------------------
// R E A D I N G   T R A C K S
// Reading into the playlist, track by track, store to RAM
// ----------------------------------------------------------


    // Play (aka loop) playlist, start at track number 1
    Track *play = playlist;
    while (play != NULL)
    {
        // STATUS PRINT 1: ID and name
        printf("No %i - %s ", play->track_number, play->name);

        // Open readfile
        FILE *readfile = fopen(play->path, "r");
        if (readfile == NULL)
        {
            printf("\033[0;31m[ERROR]\033[0m Could not open file at %s\n", play->path);
            play = play->next;
            continue;
        }

        // Read and check file header
        // http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/Docs/riffmci.pdf

        fread(&play->riff, sizeof(RiffChunk), 1, readfile);
        // Check groupID for "RIFF"
        // char groupID[5];
        // groupID[4] = '\0';
        // fread(&groupID, sizeof(char), 4, readfile);
        if (strcmp(play->riff.ckID, "RIFF\0") != 0)
        {
            printf("\033[0;33m[SKIPPED]\033[0m Only RIFF Files are supported\n");
            fclose(readfile);
            play = play->next;
            continue;
        }
        // // Get fileSize and store to Track (chunkSize = file length in bytes - 8)
        // uint32_t fileSize;
        // fread(&fileSize, sizeof(uint32_t), 1, readfile);
        // play->fileSize = fileSize;

        // Check riffType for "WAVE"
        // char riffType[5];
        // riffType[4] = '\0';
        // fread(&riffType, sizeof(char), 4, readfile);
        if (strcmp(play->riff.riffType, "WAVE\0") != 0)
        {
            printf("\033[0;33m[SKIPPED]\033[0m Only PCM Files are supported\n");
            fclose(readfile);
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
            if (feof(readfile))
            {
                printf("\033[0;33m[SKIPPED]\033[0m No audio data found, file corruption\n");
                skipFlag = 1;
                continue;
            }

            // Skip NUL characters aka padding
            // Read a single character (check) from file
            char check;
            fread(&check, sizeof(char), 1, readfile);
            // Skip character if it is 0 (padding zero)
            if (check == 0)
            {
                continue;
            }
            // Rewind last fread (1 char) and continue
            else
            {
                fseek(readfile, -sizeof(char), SEEK_CUR);
            }

            // Read chunkID (4 letter code + manually add terminator for strcmp)
            char ckID[5];
            ckID[4] = '\0';
            fread(&ckID, sizeof(DWORD), 1, readfile);

            // Read chunkSize
            DWORD ckSize;
            fread(&ckSize, sizeof(DWORD), 1, readfile);

            // Check if format chunk, mandatory first chunk after RIFF header
            if (strcmp(ckID, "fmt \0") == 0)
            {
                // Rewind ckSize and ckId, than read entire fmt chunk
                fseek(readfile, -sizeof(DWORD) * 2, SEEK_CUR);
                fread(&play->fmt, sizeof(FmtChunk), 1, readfile);

                // Reject floating point wave files
                if (play->fmt.wBitsPerSample == 32)
                {
                    printf("\033[0;33m[SKIPPED]\033[0m 32 Bit floating point not supported\n");
                    skipFlag = 1;
                    continue;
                }

                // Set fmt data to master if this is the first track
                if (track_count == 0)
                {
                    
                }
                // Validate all other tracks against the master and reject if format does not match
                else
                {
                    if (output->fmt.nChannels != play->fmt.nChannels)
                    {
                        printf("\033[0;33m[SKIPPED]\033[0m Channel count (%hu Ch) does not match output track (%hu Ch)\n", play->fmt.nChannels, output->fmt.nChannels);
                    }
                    else if (output->fmt.nSamplesPerSec != play->fmt.nSamplesPerSec)
                    {
                        printf("\033[0;33m[SKIPPED]\033[0m Samplerate (%u Hz) does not match output track (%u Hz)\n", play->fmt.nSamplesPerSec, output->fmt.nSamplesPerSec);
                    }
                    else if (output->fmt.wBitsPerSample != play->fmt.wBitsPerSample)
                    {
                        printf("\033[0;33m[SKIPPED]\033[0m Bit depth (%hu bit) does not match output track (%hu bit)\n", play->fmt.wBitsPerSample, output->fmt.wBitsPerSample);
                    }
                    skipFlag = 1;
                    continue;
                }
            }
            // Reject RF64 BWF (encountered ds64 chunk)
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
                fseek(readfile, ckSize, SEEK_CUR);
            }
            else
            {
                // date chunk found, leave chunk reading loop
                break;
            }
        } while (skipFlag == 0);


        // Skip track if flagged for skip
        if (skipFlag == 1)
        {
            skipFlag = 0;
            fclose(readfile);
            play = play->next;
            continue;
        }

        // Close readfile
        fclose(readfile);

        // STATUS PRINT 2: Metadata
        printf("\033[0;32m(%hu Ch, %u Hz, %hu bit)\033[0m\n", play->fmt.nChannels, play->fmt.nSamplesPerSec, play->fmt.wBitsPerSample);

        // Move play pointer to next track
        play = play->next;
    }



// ----------------------------------------------------------
// O U T P U T
// Reading of playlist is done, start writing to output file
// ----------------------------------------------------------


    // Create RIFF chunk
    output->riff = playlist->riff;
    // output->riff.ckSize = FILESIZE - 8;

    // Create format chunk
    output->fmt = playlist->fmt;
    output->fmt.nAvgBytesPerSec = output->fmt.nSamplesPerSec * output->fmt.nChannels * output->fmt.wBitsPerSample / 8;
    output->fmt.nBlockAlign = output->fmt.nChannels * output->fmt.wBitsPerSample / 8;

    // Create data chunk
    output->data.ckID = playlist->data.ckID;
    output->data.ckSize = output->fmt.nChannels * file_count * sample_count;

    // Copy meta data from first valid track (master) to output track
    output->name = wflag;
    output->fmt = play->fmt;

    // Open output file for writing
    FILE *writefile = fopen(wflag, "w");
    if (writefile == NULL)
    {
        printf("\033[0;31m[ERROR]\033[0m Could not write to file %s\n", wflag);
        return 5;
    }

    // Start reading samples here! XXX
    BYTE *trans = malloc(sizeof(BYTE));
    while(fread(trans, sizeof(BYTE), 1, readfile) > 0)
    {
        fwrite(trans, sizeof(BYTE), 1, writefile);
    }

    // TEST
    return 99;

    // // Open output file for writing
    // FILE *writefile = fopen(output->name, "w");
    // if (writefile == NULL)
    // {
    //     printf("\033[0;31m[ERROR]\033[0m Could not write to file %s\n", output->name);
    //     delete(playlist);
    //     closedir (dir);
    //     free(output);
    //     return 5;
    // }

    // Write RIFF header (FFIRXXXXEVAW mtf) + XXXX (Subchunk Size)
    // DWORD riff_header [5] = {0x46464952, 0x0050C006, 0x45564157, 0x20746d66, 0x00000012}; 
    // fwrite(&riff_header, sizeof(riff_header), 1, writefile);

    char riff_header [] = {'R', 'I', 'F','F',0x06, 0xC0, 0x50,0x00,'W', 'A', 'V','E','f', 'm', 't','\0', 12, '\0', '\0', '\0'}; 
    fwrite(&riff_header, sizeof(char), 20, writefile);

    // Write Format Chunk
    fwrite(&output->fmt, sizeof(FmtChunk), 1, writefile);

    // Write Padding BYTE ??? XXX
    WORD padding = 0x00000000;
    fwrite(&padding, sizeof(WORD), 1, writefile);

    // Write data chunk id
    DWORD data_id = 0x61746164;
    fwrite(&data_id, sizeof(DWORD), 1, writefile);

    // Write data Chunk size XXX
    DWORD data_length [2] = {0x0050BFE0, 0x00000000};
            fwrite(&data_length, sizeof(DWORD), 2, writefile);


    // Free output track
    free(output);

    // Clear playlist and free memory [XXX Move this up]
    delete(playlist);

    // Close directory [XXX Move this up]
    closedir (dir);
}



// ----------------------------------------------------------
// H E L P E R   F U N C T I O N S
// Functions called from within main
// ----------------------------------------------------------


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



// ----------------------------------------------------------
// A P E N D I X
// Error codes / return codes and notes
// ----------------------------------------------------------


// Error codes
// 1: Wrong user input
// 2: Failed to allocate memory
// 3: File not found
// 4: Error or file corruption in read
// 5: Error or file corruption in write


// Notes / ToDo
// - structure code
// - Implemens sorting via strcomp