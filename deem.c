/*
    deem version 1.0 - Key-Value ar Archive
    Copyright (c) 2015 Avner Herskovits

    MIT License

    Permission  is  hereby granted, free of charge, to any person  obtaining  a
    copy of this  software and associated documentation files (the "Software"),
    to deal in the Software  without  restriction, including without limitation
    the rights to use, copy, modify, merge,  publish,  distribute,  sublicense,
    and/or  sell  copies of  the  Software,  and to permit persons to whom  the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this  permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT  WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE  WARRANTIES  OF  MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR  ANY  CLAIM,  DAMAGES  OR  OTHER
    LIABILITY, WHETHER IN AN  ACTION  OF  CONTRACT,  TORT OR OTHERWISE, ARISING
    FROM,  OUT  OF  OR  IN  CONNECTION WITH THE SOFTWARE OR THE  USE  OR  OTHER
    DEALINGS IN THE SOFTWARE.

*/

#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* http://stackoverflow.com/a/14199731   */
int make_directory( const char* name )
{
#if defined( __linux__ ) || defined( __unix__ )
    return mkdir( name, 0777 );
#else
    return _mkdir( name );
#endif
}

void help_message() {
    fputs( "deem, verion 1.0: Key-Value ar Archive.\n",                                   stderr );
    fputs( "Usage:\n",                                                                    stderr );
    fputs( "    deem [options] <archive>\n",                                              stderr );
    fputs( "Options:\n",                                                                  stderr );
    fputs( "    -h --help              This help message\n",                              stderr );
    fputs( "    -q --quiet             Suppress stats, new archive, test-mode notices\n", stderr );
    fputs( "    -t --test              Test for changes but do not touch the archive\n",  stderr );
    fputs( "    -i --input <filename>  Read from filename rather than from stdin\n",      stderr );
    fputs( "Exit status 0 if okay or minor problems, 1 otherwise.\n",                     stderr );
    fputs( "More info in the README file enclosed with the source code.\n",               stderr );
    fputs( "Copyright (c) 2015 Avner Herskovits, licensed under the MIT License.\n",      stderr );
}

/* Put content in named file    */
void fputcontent( const char* filename, const char* content ){
    FILE* fileh = fopen( filename, "wb" );
    if( NULL == fileh ) {
        perror( "Cannot create file" );
        exit( EXIT_FAILURE );
    }
    if( EOF == fputs( content, fileh )) {
        perror( "Cannot write to file" );
        exit( EXIT_FAILURE );
    }
    fclose( fileh );
}

/* Create a temporary file/directory name (due to mkdtemp issues on mingw)     */
char* temporary_name() {
    char template[] = "tmp_deem_XXXXXX";
    char* result = malloc( 16 );                                /* strlen( template ) + 1   */
    char base36[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    int count;
    strcpy( result, template );
    for( count = 9; count < 15; count++ )
        result[ count ] = base36[ rand() % 36 ];
    return result;
}

void _die_if( int condition, char* message ) {
    if( ! condition ) return;
    if( errno )
        perror( message );
    else {
        fputs( message, stderr );
        fputs( "\n", stderr );
    }
    exit( EXIT_FAILURE );
}

void _warn_if( int condition, char* message ) {
    if( ! condition ) return;
    if( errno )
        perror( message );
    else {
        fputs( message, stderr );
        fputs( "\n", stderr );
    }
}

int main( int argc, char* argv[] ) {

    /* CLI arguments management */
    static struct option long_options[] = {
        { "help",  no_argument,       0, 'h'},
        { "quiet", no_argument,       0, 'q'},
        { "test",  no_argument,       0, 't'},
        { "input", required_argument, 0, 'i'},
        { 0, 0, 0, 0 }
    };
    int      option_char  = 0;
    int      option_index = 0;
    int      opt_quiet    = 0;
    int      opt_test     = 0;
    char*    opt_input    = NULL;
    char*    arg_archive  = NULL;

    /* Tree declarations */
    typedef struct { char k[ 260 ], v[ 260 ]; } pair;
    pair*    inp;
    pair**   found;
    void*    troot = NULL;
    int compair( const void* lv, const void* rv ) {
        const pair* l = lv;
        const pair* r = rv;
        return strcmp( l->k, r->k );
    }
    
    /* Misc buffers */
    char     tmp[ 600 ];
    char     trash[ 10 ];
    char*    tmpdir_name;
    char*    tmpfile_name;
    DIR*     dirh;
    FILE*    fileh;
    struct dirent* dirc;

    /* Counters */
    unsigned count_current   = 0;
    unsigned count_input     = 0;
    unsigned count_unchanged = 0;
    unsigned count_add       = 0;
    unsigned count_remove    = 0;
    unsigned count_update    = 0;

    /* Init random seed */
    srand ( time ( 0 ));

    /* Parse input arguments    */
    while( -1 != ( option_char = getopt_long( argc, argv, "hqti:", long_options, &option_index ))) {
        switch( option_char ) {
            case '?': help_message(); exit( EXIT_FAILURE );
            case 'h': help_message(); exit( 0 );
            case 'q': opt_quiet = 1; break;
            case 't': opt_test = 1; break;
            case 'i': opt_input = optarg; break;
            default: fputs( "ERROR: Unexpected option, aborting.\n", stderr ); exit( EXIT_FAILURE );
        }
    }

    if( optind == argc ) {
        fputs( "ERROR: Missing archive name.\n\n", stderr );
        help_message();
        exit( EXIT_FAILURE );
    } else if( optind + 1 != argc ) {
        fputs( "ERROR: Too many parameters.\n\n", stderr );
        help_message();
        exit( EXIT_FAILURE );
    }

    arg_archive = argv[ optind ];

    /* Read current state of archive    */
    if( NULL == ( fileh = fopen( arg_archive, "rb" ))) {
        if( ! opt_quiet )
            fputs( "Creating a new archive\n", stderr );
    } else {
        fclose( fileh );
        sprintf( tmp, "ar -pv %s", arg_archive );
        _die_if( NULL == ( fileh = popen( tmp, "r" )), tmp );
        inp = malloc( sizeof( pair ));
        while ( -1 != fscanf( fileh, "%10[ <\n\r\f\v\t] %259s %259s", trash, inp->k, inp->v )) {
            inp->k[ strlen( inp->k ) - 1 ] = 0;                    /* rid enclosing angle brackets */
            _die_if( NULL == tsearch( inp, &troot, compair ), "Error inserting a pair to tree" );
            inp = malloc( sizeof( pair ));
            count_current++;
        }
        free( inp );
        pclose( fileh );
    }

    /* Create a temporary directory */
    tmpdir_name = temporary_name();
    if( ! opt_test ) {
        _die_if( make_directory( tmpdir_name ), "Error creating temporary directory" );
        _die_if( chdir( tmpdir_name ), "Error changing into temporary directory" );
    }

    /* Get the new list of items and compare against the archive    */
    fileh = stdin;
    if( NULL != opt_input )
        _die_if( NULL == ( fileh = fopen( opt_input, "rb" )), "Error opening input file" );
    inp = malloc( sizeof( pair ));
    while ( -1 != fscanf( fileh, "%259s %259s", inp->k, inp->v )) {
        count_input++;
        found = tfind( inp, &troot, compair );
        if( NULL == found ) {
            count_add++;
            if( ! opt_test )
                fputcontent( inp->k, inp->v );
        } else if( strcmp( inp->v, ( *found )->v )) {
            count_update++;
            tdelete( inp, &troot, compair );
            if( ! opt_test )
                fputcontent( inp->k, inp->v );
        } else {
            tdelete( inp, &troot, compair );
            count_unchanged++;
        }
    }
    if( NULL != opt_input )
        _warn_if( fclose( fileh ), "Error closing input file" );

    count_remove = count_current - count_update - count_unchanged;

    if( ! opt_test )
        _die_if( chdir( ".." ), "Error changing back from temporary directory" );

    /* Print stats    */
    if( ! opt_quiet ) {
        fprintf( stderr, "Currently %u items, got %u, inserting %u, removing %u, updating %u.\n", count_current, count_input, count_add, count_remove, count_update );
        if( opt_test )
            fputs( "Test mode - nothing done.\n", stderr );
    }
    
    if( opt_test ) exit( 0 );

    /* Remove from archive deprecated items */
    void removr( const void* nodepv, VISIT value, int level ) {
        pair** nodep = ( void* ) nodepv;
        if( postorder == value || leaf == value )
            fprintf( fileh, " %s", ( *nodep )->k );
    }
    if( count_remove ) {
        tmpfile_name = temporary_name();   /* hold options and list of names for ar */
        _warn_if( NULL == ( fileh = fopen( tmpfile_name, "wb" )), "problem opening a temporary file for ar -d" );
        twalk( troot, removr );
        _warn_if( fclose( fileh ), "Error closing temporary file" );
        sprintf( tmp, "ar -d %s @%s", arg_archive, tmpfile_name );
        _warn_if( system( tmp ), "Error calling ar -d" );
        _warn_if( unlink( tmpfile_name ), "Error unlinking temporary ar -d file" );
    }

    /* Insert/update new/changed items  */
    if( count_add || count_update ) {
        sprintf( tmp, "ar -rc %s %s/*", arg_archive, tmpdir_name );
        _warn_if( system( tmp ), "Error calling ar -rc" );
    }

    /* Remove temporary directory   */
    _warn_if( NULL == ( dirh = opendir ( tmpdir_name )), "Error listing the temporary directory" );
    while( dirc = readdir( dirh )) {
        if( strcmp( ".", dirc->d_name ) && strcmp( "..", dirc->d_name )) {
            sprintf( tmp, "%s/%s", tmpdir_name, dirc->d_name );
            _warn_if( unlink( tmp ), "Error unlinking a file in the temporary directory" );
        }
    }
    _warn_if( closedir( dirh ), "Error closing temporary directory" );
    _warn_if( rmdir( tmpdir_name ), "Error removing temporary directory" );

    exit( 0 );
}
