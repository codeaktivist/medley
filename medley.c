// ----------------------------------------------------------
// m e l o d y   v 1 . 0 . 0
// martin.ulm@googlemail.com
// ----------------------------------------------------------


#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>



// ----------------------------------------------------------
// D E C L A R A T I O N S
// Types, structures, function prototypes, global variables
// ----------------------------------------------------------


// Aliases for primitive data types as described for MS RIFF standard
typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;


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
    int track_number;       // Track number
    float duration;         // Track duration in seconds
    char *name;             // File name
    char *path;             // Full path incl. file name
    FILE *readfile;         // Pointer to file for read and copy
    struct Track *prev;     // Pointer to previous track
    struct Track *next;     // Pointer to next track
    struct RiffChunk riff;  // RIFF Chunk, file info
    struct FmtChunk fmt;    // Format Chunk, meta data
    struct DataChunk data;  // Data Chunk, audio data
}
Track;


// Prototypes
void print_welcome();
void print_help();
void number_tracks(Track *playlist);
void delete(Track *track);


// Global variables
int samples_in;             // sample position of in mark
int samples_out;            // sample position of out mark
int samples_track;          // sample length of each track slice
int samples_fade;           // sample length of crossfade
int file_count = 0;         // audio files found in directory
int track_count = 0;        // count of valid tracks added to playlist


// Big endian encoded 4 character identifiers to check against
const DWORD RIFF = 0x46464952;
const DWORD WAVE = 0x45564157;
const DWORD FMT  = 0x20746d66;
const DWORD DATA = 0x61746164;
const DWORD DS64 = 0x34367364;



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


    // Define allowed command line flags and default values
    int flag;
    char *flags = "hr:w:i:o:x:";
    char *rflag = "audio/";     // (r)ead source directory
    char *wflag = "medley.wav"; // (w)rite to output file
    float  iflag = 10;            // (i)n-marker in seconds
    float  oflag = 20;            // (o)ut-marker in seconds
    float  xflag = 2;             // (x)fade duration in seconds

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
                    printf("\033[0;31m[ERROR]\033[0m Check your source directory: -r\nPath to source directory must end with '/'\n\nTo see the help page type ./medley -h\n\n");
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
                    printf("\033[0;31m[ERROR]\033[0m Check your in-marker: -i (start in seconds)\n\nTo see the help page type ./medley -h\n\n");
                    return 1;
                }
                break;

            case 'o':
                oflag = atoi(optarg);
                if (oflag <= 0)
                {
                    printf("\033[0;31m[ERROR]\033[0m Check your out-marker: -o (end in seconds)\n\nTo see the help page type ./medley -h\n\n");
                    return 1;
                }
                break;

            case 'x':
                xflag = atoi(optarg);
                if (oflag <= 0)
                {
                    printf("\033[0;31m[ERROR]\033[0m Check your crossfade: -x (length in seconds)\n\nTo see the help page type ./medley -h\n\n");
                    return 1;
                }
                break;

            case '?':
                printf("\033[0;31m[ERROR]\033[0m Wrong command line arguments found\n\nTo see the help page type ./medley -h\n\n");
                return 1;

            default:
                printf("\033[0;31m[ERROR]\033[0m Wrong command line arguments found.\n\nTo see the help page type ./medley -h\n\n");
                return 1;
        }
    }

    // Check in and out marker
    if (iflag >= oflag)
    {
        printf("\033[0;31m[ERROR]\033[0m In marker (%.2f seconds) must be located before out marker (%.2f seconds)\n\nTo see the help page type ./medley -h\n\n", iflag, oflag);
        return 1;
    }

    // Check crossfade length
    if (xflag > (oflag - iflag) / 2)
    {
        printf("\033[0;31m[ERROR]\033[0m Your crossfade (%.2f seconds) is too long for the specified duration of %.2f seconds\n\nTo see the help page type ./medley -h\n\n", xflag, oflag - iflag);
        return 1;
    }



// ----------------------------------------------------------
// T R A C K S   &   P L A Y L I S T
// Initializing output track and input playlist
// ----------------------------------------------------------


    // Initialize playlist as head of track linked list
    Track *playlist = NULL;


    // Initialize output track
    Track *output = malloc(sizeof(Track));
    if (output == NULL)
    {
        printf("\033[0;31m[ERROR]\033[0m Couldn't allocate memory for output track\n\nAbort! Let Martin know about this\n\n");
        return 2;
    }



// ----------------------------------------------------------
// R E A D I N G   D I R E C T O R Y   ( P R E F L I G H T )
// Reading directory to sorted singly linked list
// ----------------------------------------------------------


    // Get pointer to source directory
    DIR *dir;
    dir = opendir (rflag);
    if (dir == NULL)
    {
        printf("\033[0;31m[ERROR]\033[0m Couldn't open the directory: %s\n\nTo see the help page type ./medley -h\n\n", rflag);
        return 1;
    }

    // Struct representing entry in directory
    struct dirent *direntry;

    // Read dir entry and move to next
    while ((direntry = readdir (dir)))
    {
        // Check for correct file extension: .wav, .wave, .bfw
        char *ext = strrchr(direntry->d_name, '.');
        if (ext && (!strcasecmp(ext, ".wav") || !strcasecmp(ext, ".wave") || !strcasecmp(ext, ".bwf")))
        {
            // Add track to playlist
            file_count++;
            Track *new = malloc(sizeof(Track));

            if (new == NULL)
            {
                printf("\033[0;31m[ERROR]\033[0m Couldn't allocate memory for track number %i\n\nAbort! Let Martin know about this\n\n", file_count);
                return 2;
            }

            // Fill Track structure with data
            new->name = direntry->d_name;
            new->prev = NULL;
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



// ----------------------------------------------------------
// S O R T I N G   D I R E C T O R Y
// Platform independent sorting: 0->9->A->Z
// ----------------------------------------------------------


            // Adjust next and prev pointer
            if (file_count == 1)
            {
                // If this is the first file, set playlist (head of list) to point to it, no next
                playlist = new;
            }
            else
            {
                // Search pointer travels thru playlist
                Track *search = playlist;
                while (search != NULL)
                {
                    // Sort case-insensitive by name
                    if (strcasecmp(new->name, search->name) < 0)
                    {
                        // Put new before search
                        if (search->prev != NULL)
                        {
                            search->prev->next = new;
                            new->prev = search->prev;
                            search->prev = new;
                            new->next = search;
                        }
                        // Put new to head of playlist
                        else
                        {
                            playlist->prev = new;
                            new->next = playlist;
                            playlist = new;
                        }
                        break;
                    }
                    // Put new to end of playlist
                    if (search->next == NULL)
                    {
                        search->next = new;
                        new->prev = search;
                        break;
                    }
                    search = search->next;
                }
            }
        }
    }

    // Renumber tracks after sorting
    number_tracks(playlist);



// ----------------------------------------------------------
// R E A D I N G   T R A C K S
// Adding tracks to playlist, track by track, store in RAM
// ----------------------------------------------------------


    // DEBUG
    printf("\n\nORDER 1:\n");
    Track *temp1 = playlist;
    while (temp1 != NULL)
    {
        if (temp1->name != NULL)
            printf("No %i: %s | prev: %s | next: %s\n", temp1->track_number, temp1->name, temp1->prev == NULL?"NULL":temp1->prev->name, temp1->next == NULL?"NULL":temp1->next->name);
        else
            printf("No Name\n");
        temp1 = temp1->next;
    }
    printf("\n\n");


    // Play (aka loop) playlist, start at track number 1
    Track *play = playlist;

    // None valid files get removed from list
    int skipFlag = 0;

    while (play != NULL)
    {
        // Open file for reading
        play->readfile = fopen(play->path, "r");

        // Helper loop for error handling (break on skipFlag)
        do
        {
            // STATUS PRINT: ID and name
            printf("No %i - %s ", play->track_number, play->name);

            // Handle file write access error
            if (play->readfile == NULL)
            {
                printf("\033[0;31m[ERROR]\033[0m Could not open file at %s\n", play->path);
                skipFlag = 1;
                break;
            }

            // Handle RIFF header
            fread(&play->riff, sizeof(RiffChunk), 1, play->readfile);
            if (play->riff.ckID != RIFF)
            {
                printf("\033[0;33m[SKIPPED]\033[0m Only RIFF Files are supported\n");
                skipFlag = 1;
                break;
            }

            // Handle WAVE header
            if (play->riff.riffType != WAVE)
            {
                printf("\033[0;33m[SKIPPED]\033[0m Only PCM Files are supported\n");
                skipFlag = 1;
                break;
            }



// ----------------------------------------------------------
// R E A D I N G   C H U N K S
// Read chunks (ckID, chSize) till fmt & data chunk is found
// Ignore and skip all other chunks, e.g. BEXT chunk (BWF)
// ----------------------------------------------------------


            do
            {
                // Handle padding, skip NUL character(s)
                char check_padding;
                fread(&check_padding, sizeof(char), 1, play->readfile);
                if (check_padding == 0) // Probe for NUL
                {
                    continue;
                }
                else    // Rewind last fread and continue if not NUL
                {
                    fseek(play->readfile, -sizeof(char), SEEK_CUR);
                }

                // Check for chunkID
                DWORD check_Id;
                fread(&check_Id, sizeof(DWORD), 1, play->readfile);

                // Check for chunkSize
                DWORD ckSize;
                fread(&ckSize, sizeof(DWORD), 1, play->readfile);

// ----------------------------------------------------------
// F O R M A T   C H U N K
// ----------------------------------------------------------

                if (check_Id == FMT)
                {
                    fseek(play->readfile, -sizeof(DWORD) * 2, SEEK_CUR);
                    fread(&play->fmt, sizeof(FmtChunk), 1, play->readfile);
                    
                    // Reject floating point wave files (for now) because of mantissa exponent handling
                    if (play->fmt.wBitsPerSample == 32)
                    {
                        printf("\033[0;33m[SKIPPED]\033[0m 32 Bit floating point not supported\n");
                        skipFlag = 1;
                        break;
                    }

                    // Reject surround wave files
                    if (play->fmt.nChannels > 2)
                    {
                        printf("\033[0;33m[SKIPPED]\033[0m Multichannel files are not supported\n");
                        skipFlag = 1;
                        break;
                    }

                    // Reject low-res, 8 bit and below (for now) because of amplitude range: 0 - 255 positive integer
                    if (play->fmt.wBitsPerSample <= 8)
                    {
                        printf("\033[0;33m[SKIPPED]\033[0m Low-res / 8-bit files are not supported\n");
                        skipFlag = 1;
                        break;
                    }

                    // Set fmt data to master if this is the first track
                    if (track_count == 0)
                    {
                        output->fmt = play->fmt;
                    }
                    // Validate all other tracks against the master and reject if format does not match
                    else
                    {
                        if (output->fmt.nChannels != play->fmt.nChannels)
                        {
                            printf("\033[0;33m[SKIPPED]\033[0m Channel count (%hu Ch) does not match output track (%hu Ch)\n", play->fmt.nChannels, output->fmt.nChannels);
                            skipFlag = 1;
                            break;
                        }
                        else if (output->fmt.nSamplesPerSec != play->fmt.nSamplesPerSec)
                        {
                            printf("\033[0;33m[SKIPPED]\033[0m Samplerate (%u Hz) does not match output track (%u Hz)\n", play->fmt.nSamplesPerSec, output->fmt.nSamplesPerSec);
                            skipFlag = 1;
                            break;
                        }
                        else if (output->fmt.wBitsPerSample != play->fmt.wBitsPerSample)
                        {
                            printf("\033[0;33m[SKIPPED]\033[0m Bit depth (%hu bit) does not match output track (%hu bit)\n", play->fmt.wBitsPerSample, output->fmt.wBitsPerSample);
                            skipFlag = 1;
                            break;
                        }
                    }

                    // Move *readfile to end of fmt chunk (skipping padding bytes if there are any)
                    fseek(play->readfile, -sizeof(FmtChunk) + 2 * sizeof(DWORD) + play->fmt.ckSize, SEEK_CUR);

                    // Continue to search for data chunk
                    continue;
                }

// ----------------------------------------------------------
// D A T A   C H U N K
// ----------------------------------------------------------

                if (check_Id == DATA)
                {
                    play->data.ckID = check_Id;
                    play->data.ckSize = ckSize;

                    // Calculate duration in seconds
                    play->duration = (float) play->data.ckSize * 8 / (play->fmt.nChannels * play->fmt.nSamplesPerSec * play->fmt.wBitsPerSample);

                    if (play->duration < oflag)
                    {
                        printf("\033[0;33m[SKIPPED]\033[0m Track duration (%.2f seconds) too short for out marker (%.2f seconds)\n", play->duration, oflag);
                        skipFlag = 1;
                        break;
                    }

                    // Calculate globals as first valid track is found (i.e. valid fmt chunk and valid data chunk)
                    if (track_count == 0)
                    {
                        samples_in = iflag * output->fmt.nSamplesPerSec;
                        samples_out = oflag * output->fmt.nSamplesPerSec;
                        samples_track = samples_out - samples_in;
                        samples_fade = xflag * output->fmt.nSamplesPerSec;
                    }

                    // FF pointer to in marker
                    fseek(play->readfile, samples_in * output->fmt.nChannels * output->fmt.wBitsPerSample / 8, SEEK_CUR);

                    track_count++;
                    break;
                }

                // Reject RF64 BWF (encountered ds64 chunk)
                if (check_Id == DS64)
                {
                    printf("\033[0;33m[SKIPPED]\033[0m RF64 BWF is not supported\n");
                    skipFlag = 1;
                    break;
                }

                // Check for reaching End Of File
                if (feof(play->readfile))
                {
                    printf("\033[0;33m[SKIPPED]\033[0m No audio data found, file corruption\n");
                    skipFlag = 1;
                    break;
                }

                // Skip all other chunks
                fseek(play->readfile, ckSize, SEEK_CUR);

            } while (skipFlag == 0);

            // Break helper loop when track is valid
            break;

        } while (1);



// ----------------------------------------------------------
// R E M O V E   I N V A L I D   F I L E S
// Remove all flagged files from linked list
// ----------------------------------------------------------


        // Handle deletion from list
        if (skipFlag == 1)
        {
            // Reset flag for next iteration
            skipFlag = 0;

            Track *delete = play;

            // Remove if only one track in list
            if (play->next == NULL && play->prev == NULL)
            {
                playlist = NULL;
            }

            // Remove from head of list (no previous)
            else if (play->prev == NULL)
            {
                playlist = play->next;
                playlist->prev = NULL;
            }
            // Remove from end of list (no next)
            else if (play->next == NULL)
            {
                play->prev->next = NULL;
            }
            // Remove from middle of list
            else
            {
                play->next->prev = play->prev;
                play->prev->next = play->next;
            }

            // Close readfile
            fclose(play->readfile);

            // Free memory
            free(delete->path);
            free(delete);
        }
        else
        {
            // STATUS PRINT: Success
            printf("\033[0;32m(%hu Ch, %u Hz, %hu bit, %.2f seconds)\033[0m\n", play->fmt.nChannels, play->fmt.nSamplesPerSec, play->fmt.wBitsPerSample, play->duration);
        }

        // Move play pointer to next track (after success)
        play = play->next;
    }

    // Handle corner case: No valid audio files found
    if (playlist == NULL)
    {
        printf("\033[0;31m[ERROR]\033[0m No valid audio files present at: %s\n\n", rflag);
        free(output);
        closedir (dir);
        return 3;
    }

    number_tracks(playlist);

    // DEBUG
    printf("\n\nORDER 2:\n");
    Track *temp2 = playlist;
    while (temp2 != NULL)
    {
        if (temp2->name != NULL)
        {
            // printf("%sn", temp2->name);
            printf("No %i: %s | prev: %s | next: %s\n", temp2->track_number, temp2->name, temp2->prev == NULL?"NULL":temp2->prev->name, temp2->next == NULL?"NULL":temp2->next->name);
        }
        else
            printf("No Name\n");
        temp2 = temp2->next;
    }
    printf("\n\n");



// ----------------------------------------------------------
// O U T P U T
// Reading of playlist is done, start writing to output file
// ----------------------------------------------------------


    // Set name (optional)
    output->name = wflag;

    // Data chunk: Set Id
    output->data.ckID = DATA;

    // Data chunk: Calculate raw audio size -> samples out = n * length - (n - 1) * fade
    output->data.ckSize = (track_count * samples_track - (track_count - 1) * samples_fade) * output->fmt.nBlockAlign;

    // RIFF chunk: Copy from Playlist
    output->riff = playlist->riff;

    // RIFF chunk: Calculate filesize -> raw audio + data header (8) + fmt chunk (24)
    output->riff.ckSize = output->fmt.ckSize + 8 + 24;

    // Format chunk: Set size to 16 Bytes (standard wave header)
    output->fmt.ckSize = 16;

    // Open Output file for writing
    output->readfile = fopen(output->name, "w");
    if (output->readfile == NULL)
    {
        printf("\033[0;31m[ERROR]\033[0m Could not write to file: %s\n\n", wflag);
        return 5;
    }

    // Write RIFF chunk
    fwrite(&output->riff, sizeof(RiffChunk), 1, output->readfile);

    // Write format chunk
    fwrite(&output->fmt, sizeof(FmtChunk), 1, output->readfile);

    // Write data chunk header
    fwrite(&output->data, sizeof(DataChunk), 1, output->readfile);

    // Write raw audio from playlist
    Track *write = playlist;
    while (write != NULL)
    {
        for (int i = 0; i < samples_track; i++)
        {
            
        }
        write = write->next;
    }



                    // Read samples from file into structure
                    // int sampleCount = 0;
                    // while (sampleCount < samples_track)
                    // {
                    //     for (int channelCount = 1; channelCount <= output->fmt.nChannels; channelCount++)
                    //     {
                    //         fread(&play->data.frame[sampleCount].channel[channelCount], output->fmt.wBitsPerSample / 8, 1, play->readfile);
                    //         sampleCount++;
                    //     }
                    // }


    // // Start reading samples here! XXX
    // BYTE *trans = malloc(sizeof(BYTE));
    // while(fread(trans, sizeof(BYTE), 1, play->readfile) > 0)
    // {
    //     fwrite(trans, sizeof(BYTE), 1, writefile);
    // }

    // TEST
    // return 99;

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

    fclose(output->readfile);

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


// Delete track from playlist
void delete(Track *track)
{
    // Recursive call if this is not the last track
    if (track->next != NULL)
    {
        delete(track->next);
    }

    // Close readfile
    fclose(track->readfile);

    // Free the malloc'ed path string
    free(track->path);

    // Free the Track struct
    free(track);
}


// Number tracks in playlist (ascending)
void number_tracks(Track *playlist)
{
    int number = 1;
    Track *search = playlist;
    while (search != NULL)
    {
        search->track_number = number;
        number++;
        search = search->next;
    }
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
// - (DONE) structure code
// - (DONE) Implemens sorting via strcomp
// - (DONE) No I really need to check for padded 0's? Yes, Adobe adds padding :/
// - Run without agrs to get input ... hmmm?