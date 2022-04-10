#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

// Store audio tracks in a linked list, like a playlist
typedef struct Track
{
    int track_number;   // Track number
    char *name;         // File name
    char *path;         // Full path and file name
    struct Track *pre;  // Pointer to previous track
    struct Track *next; // Pointer to next track
}
Track;

// Prototypes
void print_welcome();
void print_help();
void delete(Track *track);

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
                    printf("Check your source directory: -r (path to source directory must and with '/')\nTo see the help page type ./medley -h\n\n");
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
                    printf("Check your in-marker: -i (start in seconds)\nTo see the help page type ./medley -h\n\n");
                    return 1;
                }
                break;

            case 'o':
                oflag = atoi(optarg);
                if (oflag <= 0)
                {
                    printf("Check your out-marker: -o (end in seconds)\nTo see the help page type ./medley -h\n\n");
                    return 1;
                }
                break;

            case 'x':
                xflag = atoi(optarg);
                if (oflag <= 0)
                {
                    printf("Check your crossfade: -x (length in seconds)\nTo see the help page type ./medley -h\n\n");
                    return 1;
                }                
                break;

            case '?':
                printf("Wrong command line arguments found.\nTo see the help page type ./medley -h\n\n");
                return 1;

            default:
                printf("Wrong command line arguments found.\nTo see the help page type ./medley -h\n\n");
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
        printf("Couldn't open the directory: %sTo see the help page type ./medley -h\n\n", rflag);
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
                printf("Couldn't allocate memory for track number %i\nTo see the help page type ./medley -h\n\n", track_count);
                return 1;
            }
            new->track_number = track_count;
            new->name = direntry->d_name;
            new->next = NULL;

            // If the new track is the first track
            if (track_count == 1)
            {
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
    closedir (dir);

    // Play (aka loop) playlist, start at track number 1
    Track *play = playlist;
    while (play != NULL)
    {
        play = play->next;
    }

    // Clear playlist and free memory
    delete(playlist);
}


// Delete track from playlist
void delete(Track *track)
{
    if (track->next != NULL)
    {
        delete(track->next);
    }
    free(track);
}


// Print welcome message
void print_welcome()
{
    printf("\n.---------------.\n");
    printf(  "| medley v1.0.0 |\n");
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

