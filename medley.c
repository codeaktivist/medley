// ----------------------------------------------------------
// m e d l e y   v 1 . 0 . 0
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
#include <math.h>



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
    int trackNumber;       // Track number
    long sampleCount;      // Pointer position within audio data
    float trackDuration;    // Track trackDuration in seconds
    char *name;             // File name
    char *path;             // Full path incl. file name
    FILE *audiofile;        // Pointer to file for read and copy
    struct Track *prev;     // Pointer to previous track
    struct Track *next;     // Pointer to next track
    struct RiffChunk riff;  // RIFF Chunk, file info
    struct FmtChunk fmt;    // Format Chunk, meta data
    struct DataChunk data;  // Data Chunk, audio data
}
Track;


// Prototypes
void printWelcome();
void printHelp();
void printTracks(Track *playlist);
void numberTracks(Track *playlist);
void deleteTrack(Track *track);


// Global variables
long samplesIn;             // sample position of in mark
long samplesPart;           // sample length of each track slice
long samplesFade;           // sample length of crossfade
int fileCount = 0;          // audio files found in directory
int trackCount = 0;         // count of valid tracks added to playlist


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
    printWelcome();



// ----------------------------------------------------------
// U S E R   I N P U T
// Retrieving and checking command line arguments
// ----------------------------------------------------------


    // Define allowed command line flags and default values
    int flag;
    char *flags = "hr:w:i:d:x:";
    char *rflag = "audio/";     // (r)ead source directory
    char *wflag = "medley.wav"; // (w)rite to output file
    float  iflag = 1;           // (i)n-marker in seconds
    float  dflag = 2;           // (d)uration of track in seconds
    float  xflag = 0.5;         // (x)fade trackDuration in seconds

    // Get and check user provided flags
    while ((flag = getopt(argc, argv, flags)) != -1)
    {
        switch (flag)
        {
            case 'h':
                printHelp();
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
                iflag = atof(optarg);
                if (iflag < 0)
                {
                    printf("\033[0;31m[ERROR]\033[0m Check your in-marker: -i (start in seconds)\n\nTo see the help page type ./medley -h\n\n");
                    return 1;
                }
                break;

            case 'd':
                dflag = atof(optarg);
                if (dflag <= 0)
                {
                    printf("\033[0;31m[ERROR]\033[0m Check your duration: -d (duration in seconds)\n\nTo see the help page type ./medley -h\n\n");
                    return 1;
                }
                break;

            case 'x':
                xflag = atof(optarg);
                if (xflag < 0)
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

    // Check crossfade length
    if (xflag > dflag / 2)
    {
        printf("\033[0;31m[ERROR]\033[0m Your crossfade (%.2f seconds) is too long for the specified duration of %.2f seconds\n\nTo see the help page type ./medley -h\n\n",
               xflag, dflag);
        return 1;
    }



// ----------------------------------------------------------
// T R A C K S   &   P L A Y L I S T
// Initializing output track and input playlist
// ----------------------------------------------------------


    // Initialize playlist as head of track linked list
    Track *playlist = NULL;


    // Initialize output track
    Track *output = calloc(sizeof(Track), 1);
    if (output == NULL)
    {
        printf("\033[0;31m[ERROR]\033[0m Couldn't allocate memory for output track.\n\nAbort! Let Martin know about this...\n\n");
        return 2;
    }



// ----------------------------------------------------------
// R E A D I N G   D I R E C T O R Y   ( P R E F L I G H T )
// Reading directory to sorted singly linked list
// ----------------------------------------------------------


    // Get pointer to source directory
    DIR *dir;
    dir = opendir(rflag);
    if (dir == NULL)
    {
        printf("\033[0;31m[ERROR]\033[0m Couldn't open the directory: %s\n\nTo see the help page type ./medley -h\n\n", rflag);
        free(output);
        return 3;
    }

    // Struct representing entry in directory
    struct dirent *direntry;

    // Read dir entry and move to next
    while ((direntry = readdir(dir)))
    {
        // Check for correct file extension: .wav, .wave, .bfw
        char *ext = strrchr(direntry->d_name, '.');
        if (ext && (!strcasecmp(ext, ".wav") || !strcasecmp(ext, ".wave") || !strcasecmp(ext, ".bwf")))
        {
            // Add track to playlist
            fileCount++;
            Track *new = calloc(sizeof(Track), 1);
            if (new == NULL)
            {
                printf("\033[0;31m[ERROR]\033[0m Couldn't allocate memory for track number %i.\n\nAbort! Let Martin know about this...\n\n",
                       fileCount);
                deleteTrack(playlist);
                free(output);
                closedir(dir);
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
                deleteTrack(playlist);
                free(output);
                closedir(dir);
                return 2;
            }
            new->path = strcat(new->path, rflag);
            new->path = strcat(new->path, direntry->d_name);
            new->path = strcat(new->path, "\0");



// ----------------------------------------------------------
// S O R T I N G   F I L E S   I N   P L A Y L I S T
// Platform independent sorting: 0->9->A->Z
// ----------------------------------------------------------


            // Adjust next and prev pointer
            if (fileCount == 1)
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
    numberTracks(playlist);



// ----------------------------------------------------------
// R E A D I N G   A U D I O   T R A C K S
// Adding tracks to playlist, track by track
// ----------------------------------------------------------


    // Play (aka loop) playlist, start at track number 1
    Track *play = playlist;

    // None valid files get removed from list
    int skipFlag = 0;

    while (play != NULL)
    {
        // Open file for reading
        play->audiofile = fopen(play->path, "r");

        // Helper loop for error handling (break on skipFlag)
        do
        {
            // STATUS PRINT: ID and name
            printf("No %i - %s ", play->trackNumber, play->name);

            // Handle file write access error
            if (play->audiofile == NULL)
            {
                printf("\033[0;31m[ERROR]\033[0m Could not open file at %s\n", play->path);
                skipFlag = 1;
                break;
            }

            // Handle RIFF header
            fread(&play->riff, sizeof(RiffChunk), 1, play->audiofile);
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
                fread(&check_padding, sizeof(char), 1, play->audiofile);
                if (check_padding == 0) // Probe for NUL
                {
                    continue;
                }
                else    // Rewind last fread and continue if not NUL
                {
                    fseek(play->audiofile, -sizeof(char), SEEK_CUR);
                }

                // Check for chunkID
                DWORD check_Id;
                fread(&check_Id, sizeof(DWORD), 1, play->audiofile);

                // Check for chunkSize
                DWORD ckSize;
                fread(&ckSize, sizeof(DWORD), 1, play->audiofile);

// ----------------------------------------------------------
// F O R M A T   C H U N K
// ----------------------------------------------------------

                if (check_Id == FMT)
                {
                    fseek(play->audiofile, -sizeof(DWORD) * 2, SEEK_CUR);
                    fread(&play->fmt, sizeof(FmtChunk), 1, play->audiofile);

                    // Only allow 16 bit files -> sample allways holds int16_t
                    // 8 Bit: require unsigned integer uint8_t
                    // 24 bit: require non-primitive datatype int24_t
                    // 32 bit: require floating point arithmethic
                    if (play->fmt.wBitsPerSample != 16)
                    {
                        printf("\033[0;33m[SKIPPED]\033[0m Only 16 bit files are supported, %i bit are not\n", play->fmt.wBitsPerSample);
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

                    // Set fmt data to master if this is the first track
                    if (trackCount == 0)
                    {
                        output->fmt = play->fmt;
                    }
                    // Validate all other tracks against the master and reject if format does not match
                    else
                    {
                        if (output->fmt.nChannels != play->fmt.nChannels)
                        {
                            printf("\033[0;33m[SKIPPED]\033[0m Channel count (%hu Ch) does not match first track (%hu Ch)\n", play->fmt.nChannels,
                                   output->fmt.nChannels);
                            skipFlag = 1;
                            break;
                        }
                        else if (output->fmt.nSamplesPerSec != play->fmt.nSamplesPerSec)
                        {
                            printf("\033[0;33m[SKIPPED]\033[0m Samplerate (%u Hz) does not match first track (%u Hz)\n", play->fmt.nSamplesPerSec,
                                   output->fmt.nSamplesPerSec);
                            skipFlag = 1;
                            break;
                        }
                        else if (output->fmt.wBitsPerSample != play->fmt.wBitsPerSample)
                        {
                            printf("\033[0;33m[SKIPPED]\033[0m Bit depth (%hu bit) does not match first track (%hu bit)\n", play->fmt.wBitsPerSample,
                                   output->fmt.wBitsPerSample);
                            skipFlag = 1;
                            break;
                        }
                    }

                    // Move *audiofile to end of fmt chunk (skipping padding bytes if there are any)
                    fseek(play->audiofile, -sizeof(FmtChunk) + 2 * sizeof(DWORD) + play->fmt.ckSize, SEEK_CUR);

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

                    // Calculate trackDuration in seconds
                    play->trackDuration = (float) play->data.ckSize * 8 / (play->fmt.nChannels * play->fmt.nSamplesPerSec * play->fmt.wBitsPerSample);

                    // Calculate globals as first valid track is found (i.e. valid fmt chunk and valid data chunk)
                    if (trackCount == 0)
                    {
                        samplesIn = iflag * output->fmt.nSamplesPerSec;
                        samplesPart = dflag * output->fmt.nSamplesPerSec;
                        samplesFade = xflag * output->fmt.nSamplesPerSec;
                    }

                    // FF pointer to in marker
                    fseek(play->audiofile, samplesIn * output->fmt.nChannels * output->fmt.wBitsPerSample / 8, SEEK_CUR);

                    // Valid track, no skipFlag
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
                if (feof(play->audiofile))
                {
                    printf("\033[0;33m[SKIPPED]\033[0m No audio data found, file corruption\n");
                    skipFlag = 1;
                    break;
                }

                // Skip all other chunks
                fseek(play->audiofile, ckSize, SEEK_CUR);

            }
            while (skipFlag == 0);

            // Break helper loop when track is valid
            break;

        }
        while (1);



// ----------------------------------------------------------
// R E M O V E   I N V A L I D   F I L E S
// Remove all flagged files from linked list
// ----------------------------------------------------------


        // INVALID TRACK, remove from playlist
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

            // Close audiofile
            fclose(delete->audiofile);

            // Move play pointer to next track (on invalid track found)
            play = play->next;

            // Free memory
            free(delete->path);
            free(delete);
        }
        else
        {
            // VALID TRACK, keep in playlist
            play->sampleCount = 0;
            trackCount++;


            // STATUS PRINT: Success
            printf("\033[0;32m(%hu Ch, %u Hz, %hu bit, %.2f seconds)\033[0m\n", play->fmt.nChannels, play->fmt.nSamplesPerSec,
                   play->fmt.wBitsPerSample, play->trackDuration);

            // Move play pointer to next track (on valid track found)
            play = play->next;
        }
    }

    // Handle corner case: No valid audio files found
    if (playlist == NULL)
    {
        printf("\033[0;31m[ERROR]\033[0m No valid audio files present at: %s\n\n", rflag);
        free(output);
        closedir(dir);
        return 3;
    }

    // Renumber tracks after removing invalid tracks
    numberTracks(playlist);

    // Close directory
    closedir(dir);



// ----------------------------------------------------------
// O U T P U T
// Reading of playlist is done, start writing to output file
// ----------------------------------------------------------

    printf("\n\nCreating medley from %i audio files:\n",  trackCount);
    printf("\n0%%                  50%%                100%%");
    printf("\n┣━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━┫");
    printf("\n ");

    // Data chunk: Set Id
    output->data.ckID = DATA;

    // Data chunk: Calculate raw audio size -> samples out = n * length - (n - 1) * fade
    output->data.ckSize = ((trackCount * samplesPart) - (trackCount - 1) * samplesFade) * output->fmt.nBlockAlign;

    // RIFF chunk: Copy from Playlist
    output->riff = playlist->riff;

    // Format chunk: Set size to 16 Bytes (standard wave header)
    output->fmt.ckSize = 16;

    // RIFF chunk: Calculate filesize -> 36 + output->data.ckSize;
    output->riff.ckSize = sizeof(WAVE) + sizeof(RIFF) + 4 + output->fmt.ckSize + sizeof(DATA) + 4 + output->data.ckSize;

    // Set name (optional)
    output->name = wflag;

    // Set trackDuration
    output->trackDuration = (float) output->data.ckSize * 8 / (output->fmt.nChannels * output->fmt.nSamplesPerSec *
                            output->fmt.wBitsPerSample);

    // Open Output file for writing
    output->audiofile = fopen(output->name, "w");
    if (output->audiofile == NULL)
    {
        printf("\033[0;31m[ERROR]\033[0m Could not write to file: %s\n\n", wflag);
        deleteTrack(playlist);
        free(output);
        closedir(dir);
        return 5;
    }

    // Write RIFF chunk
    fwrite(&output->riff, sizeof(RiffChunk), 1, output->audiofile);

    // Write format chunk
    fwrite(&output->fmt, sizeof(FmtChunk), 1, output->audiofile);

    // Write data chunk header
    fwrite(&output->data, sizeof(DataChunk), 1, output->audiofile);

    // Transfer one frame (bit depth * channel) from audiofile -> writefile
    int16_t transfer_main[output->fmt.nChannels];
    int16_t transfer_fade[output->fmt.nChannels];

    // Read raw audio from playlist
    Track *copy = playlist;

    // Total samples and count of medley for process bar
    long total = output->data.ckSize / output->fmt.nBlockAlign;
    long total_count = 0;
    int total_division = 40;
    int total_process = 0;

    // Loop thru playlist and copy audio data to output
    while (copy != NULL)
    {

        for (long i = 0; i < samplesPart; i++)
        {
            // Skip fade in on all but first track
            if (copy->trackNumber > 1 && i == 0)
            {
                i += samplesFade;
            }
            // FADE IN
            if (i < samplesFade)
            {
                fread(&transfer_main, output->fmt.wBitsPerSample / 8, output->fmt.nChannels, copy->audiofile);

                for (int j = 0; j < output->fmt.nChannels; j++)
                {
                    // Linear Fade In
                    // transfer_main[j] = transfer_main[j] * (float)i / samplesFade;

                    // Squareroot Fade In
                    transfer_main[j] = transfer_main[j] * sqrt((float)i / samplesFade);
                }

                // DEBUG
                // printf("\n[%li] %li / %li : %i @ %li IN ", total_count, i, samplesPart, copy->trackNumber, copy->sampleCount);
            }
            else if (i > samplesPart - samplesFade)
            {
                // CROSSFADE
                if (copy->next != NULL)
                {
                    fread(&transfer_main, output->fmt.wBitsPerSample / 8, output->fmt.nChannels, copy->audiofile);
                    fread(&transfer_fade, output->fmt.wBitsPerSample / 8, output->fmt.nChannels, copy->next->audiofile);

                    for (int j = 0; j < output->fmt.nChannels; j++)
                    {
                        // Linear cross fade
                        // transfer_main[j] = transfer_main[j] * (1 - (float)(i - samplesPart + samplesFade) / samplesFade);
                        // transfer_fade[j] = transfer_fade[j] * (float)(i - samplesPart + samplesFade) / samplesFade;

                        // Squareroot fade out
                        transfer_main[j] = transfer_main[j] * sqrt(1 - (float)(i - samplesPart + samplesFade) / samplesFade);
                        transfer_fade[j] = transfer_fade[j] * sqrt((float)(i - samplesPart + samplesFade) / samplesFade);

                        // Summing
                        transfer_main[j] = transfer_main[j] + transfer_fade[j];
                    }

                    // DEBUG
                    // printf("\n[%li] %li / %li : %i @ %li XX %i @ %li", total_count, i, samplesPart, copy->trackNumber, copy->sampleCount, copy->next->trackNumber, copy->next->sampleCount);

                    // Forward X-fade file
                    copy->next->sampleCount++;
                }
                // FADE OUT
                else
                {
                    fread(&transfer_main, output->fmt.wBitsPerSample / 8, output->fmt.nChannels, copy->audiofile);

                    for (int j = 0; j < output->fmt.nChannels; j++)
                    {
                        // Linear fade out
                        // transfer_main[j] = transfer_main[j] * (1 - (float)(i - samplesPart + samplesFade) / samplesFade);

                        // Squareroot fade out
                        transfer_main[j] = transfer_main[j] * sqrt(1 - (float)(i - samplesPart + samplesFade) / samplesFade);
                    }

                    // DEBUG
                    // printf("\n[%li of %li] %li / %li : %i @ %li OUT", total_count, total, i, samplesPart, copy->trackNumber, copy->sampleCount);
                }
            }
            // SOLO TRACK
            else
            {
                fread(&transfer_main, output->fmt.wBitsPerSample / 8, output->fmt.nChannels, copy->audiofile);

                // DEBUG
                // printf("\n[%li] %li / %li : %i @ %li --", total_count, i, samplesPart, copy->trackNumber, copy->sampleCount);
            }

            fwrite(&transfer_main, output->fmt.wBitsPerSample / 8, output->fmt.nChannels, output->audiofile);

            copy->sampleCount++;
            total_count++;

            // Process bar
            if (total_count > (total / total_division) * total_process)
            {
                total_process++;
                printf("█");
                fflush(stdout);
            }
        }
        copy = copy->next;
    }

    printf("\n\nEnjoy your %.0f second \033[0;31mm\033[0;32me\033[0;34md\033[0;36ml\033[0;35me\033[0;33my\033[0m: ./%s\n\n",
           output->trackDuration, wflag);

    // Close output file
    fclose(output->audiofile);

    // Free output track
    free(output);

    // Clear playlist and free memory
    deleteTrack(playlist);

    // End of main
    return 0;
}



// ----------------------------------------------------------
// H E L P E R   F U N C T I O N S
// Functions called from within main
// ----------------------------------------------------------


// Print welcome message
void printWelcome()
{
    printf("\e[1;1H\e[2J"); // Clear console for most platforms
    printf("\n.---------------.\n");
    printf("| \033[0;31mm\033[0;32me\033[0;34md\033[0;36ml\033[0;35me\033[0;33my\033[0m v1.0.0 |\n");
    printf("'---------------'\n");
    printf("   by Martin Ulm\n\n");
    return;
}


// Print help page
void printHelp()
{
    FILE *helpTxt = fopen("help.txt", "r");
    if (helpTxt == NULL)
    {
        printf("\033[0;31m[ERROR]\033[0m Help file (help.text) not found\n\nAbort! Let Martin know about this...\n\n");
    }
    // Read contents from help.txt and print to console
    char c = fgetc(helpTxt);
    while (c != EOF)
    {
        printf("%c", c);
        c = fgetc(helpTxt);
    }
    fclose(helpTxt);
}


// Delete track from playlist
void deleteTrack(Track *track)
{
    // Recursive call if this is not the last track
    if (track->next != NULL)
    {
        deleteTrack(track->next);
    }

    // Close audiofile
    fclose(track->audiofile);

    // Free the malloc'ed path string
    free(track->path);

    // Free the Track struct
    free(track);
}


// Number tracks in playlist (ascending)
void numberTracks(Track *playlist)
{
    int number = 1;
    Track *search = playlist;
    while (search != NULL)
    {
        search->trackNumber = number;
        number++;
        search = search->next;
    }
}


// DEBUG: Print order of playlist
void printTracks(Track *playlist)
{
    printf("\n\nORDER 1:\n");
    Track *temp = playlist;
    while (temp != NULL)
    {
        if (temp->name != NULL)
        {
            printf("No %i: %s | prev: %s | next: %s\n", temp->trackNumber, temp->name, temp->prev == NULL ? "NULL" : temp->prev->name,
                   temp->next == NULL ? "NULL" : temp->next->name);
        }
        else
        {
            printf("No Name\n");
        }
        temp = temp->next;
    }
    printf("\n\n");
}



// ----------------------------------------------------------
// R E T U R N   C O D E S
// ----------------------------------------------------------


// 0: Success
// 1: Wrong user input
// 2: Failed to allocate memory
// 3: File not found
// 4: Error or file corruption in read
// 5: Error or file corruption in write