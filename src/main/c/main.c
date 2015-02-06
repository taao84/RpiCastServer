/*
 * Copyright (c) 2014 Netflix, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETFLIX, INC. AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NETFLIX OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <regex.h>

#include "dial_server.h"
#include "dial_options.h"

#define BUFSIZE 256

static char *spAppNetflix = "netflix";      // name of the netflix executable
static char *spDefaultNetflix = "../../../src/platform/qt/netflix";
static char *spDefaultData="../../../src/platform/qt/data";
static char *spNfDataDir = "NF_DATA_DIR=";
static char *defaultLaunchParam = "source_type=12";
static char *spDefaultFriendlyName = "DIAL server sample";
static char *spDefaultModelName = "NOT A VALID MODEL NAME";
static char *spDefaultUuid = "deadbeef-dead-beef-dead-beefdeadbeef";
static char spDataDir[BUFSIZE];
static char spNetflix[BUFSIZE];
static char spFriendlyName[BUFSIZE];
static char spModelName[BUFSIZE];
static char spUuid[BUFSIZE];
static int gDialPort;

static char *spAppYouTube = "chrome";
static char *spAppYouTubeMatch = "chrome.*google-chrome-dial";
static char *spAppYouTubeExecutable = "/opt/google/chrome/google-chrome";
static char *spYouTubePS3UserAgent = "--user-agent="
    "Mozilla/5.0 (PS3; Leanback Shell) AppleWebKit/535.22 (KHTML, like Gecko) "
    "Chrome/19.0.1048.0 LeanbackShell/01.00.01.73 QA Safari/535.22 Sony PS3/ "
    "(PS3, , no, CH)";

// Adding 20 bytes for prepended source_type for Netflix
static char sQueryParam[DIAL_MAX_PAYLOAD+20];

static int doesMatch( char* pzExp, char* pzStr)
{
    regex_t exp;
    int ret;
    int match = 0;
    if ((ret = regcomp( &exp, pzExp, REG_EXTENDED ))) {
        char errbuf[1024] = {0,};
        regerror(ret, &exp, errbuf, sizeof(errbuf));
        fprintf( stderr, "regexp error: %s", errbuf );
    } else {
        regmatch_t matches[1];
        if( regexec( &exp, pzStr, 1, matches, 0 ) == 0 ) {
            match = 1;
        }
    }
    regfree(&exp);
    return match;
}


/* The URL encoding source code was obtained here:
 * http://www.geekhideout.com/urlcode.shtml
 */

/* Converts a hex character to its integer value */
char from_hex(char ch) {
    return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

/* Converts an integer value to its hex character*/
char to_hex(char code) {
    static char hex[] = "0123456789abcdef";
  return hex[code & 15];
}

/* Returns a url-encoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
char *url_encode(const char *str) {
    const char *pstr;
    char *buf, *pbuf;
    pstr = str;
    buf = malloc(strlen(str) * 3 + 1);
    pbuf = buf;
    if( buf )
    {
        while (*pstr) {
            if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~')
                *pbuf++ = *pstr;
            else if (*pstr == ' ')
                *pbuf++ = '+';
            else
                *pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), *pbuf++ = to_hex(*pstr & 15);
            pstr++;
        }
        *pbuf = '\0';
    }
    return buf;
}

/*
 * End of URL ENCODE source
 */

/*
 * This function will walk /proc and look for the application in
 * /proc/<PID>/comm. and /proc/<PID>/cmdline to find it's command (executable
 * name) and command line (if needed).
 * Implementors can override this function with an equivalent.
 */
static int isAppRunning( char *pzName, char *pzCommandPattern ) {
  DIR* proc_fd = opendir("/proc");
  if( proc_fd != NULL ) {
    struct dirent* procEntry;
    while((procEntry=readdir(proc_fd)) != NULL) {
      if( doesMatch( "^[0-9][0-9]*$", procEntry->d_name ) ) {
        char exePath[64] = {0,};
        char link[256] = {0,};
        char cmdlinePath[64] = {0,};
        char buffer[1024] = {0,};
        int len;
        sprintf( exePath, "/proc/%s/exe", procEntry->d_name);
        sprintf( cmdlinePath, "/proc/%s/cmdline", procEntry->d_name);

        if( (len = readlink( exePath, link, sizeof(link)-1)) != -1 ) {
          char executable[256] = {0,};
          strcat( executable, pzName );
          strcat( executable, "$" );
          // TODO: Make this search for EOL to prevent false positivies
          if( !doesMatch( executable, link ) ) {
            continue;
          }
          // else //fall through, we found it
        }
        else continue;

        if (pzCommandPattern != NULL) {
          FILE *cmdline = fopen(cmdlinePath, "r");
          if (!cmdline) {
            continue;
          }
          if (fgets(buffer, 1024, cmdline) == NULL) {
            fclose(cmdline);
            continue;
          }
          fclose(cmdline);

          if (!doesMatch( pzCommandPattern, buffer )) {
            continue;
          }
        }

        closedir(proc_fd);
        return atoi(procEntry->d_name);
      }
    }

    closedir(proc_fd);
  } else {
    printf("/proc failed to open\n");
  }
  return 0;
}

static pid_t runApplication( const char * const args[], DIAL_run_t *run_id ) {
  pid_t pid = fork();
  if (pid != -1) {
    if (!pid) { // child
      putenv(spDataDir);
      printf("Execute:\n");
      for(int i = 0; args[i]; ++i) {
        printf(" %d) %s\n", i, args[i]);
      }
      execv(*args, (char * const *) args);
    } else {
      *run_id = (void *)(long)pid; // parent PID
    }
    return kDIALStatusRunning;
  } else {
    return kDIALStatusStopped;
  }
}


/* Compare the applications last launch parameters with the new parameters.
 * If they match, return false
 * If they don't match, return true
 */
static int shouldRelaunch(
    DIALServer *pServer,
    const char *pAppName,
    const char *args )
{
    return ( strncmp( DIAL_get_payload(pServer, pAppName), args, DIAL_MAX_PAYLOAD ) != 0 );
}

static DIALStatus youtube_start(DIALServer *ds, const char *appname,
                                const char *args, size_t arglen,
                                DIAL_run_t *run_id, void *callback_data) {
    printf("\n\n ** LAUNCH YouTube ** with args %s\n\n", args);

    char url[512] = {0,}, data[512] = {0,};
    sprintf( url, "https://www.youtube.com/tv?%s", args);
    sprintf( data, "--user-data-dir=%s/.config/google-chrome-dial", getenv("HOME") );

    const char * const youtube_args[] = { spAppYouTubeExecutable,
      spYouTubePS3UserAgent,
      data, "--app", url, NULL
    };
    runApplication( youtube_args, run_id );

    return kDIALStatusRunning;
}

static DIALStatus youtube_status(DIALServer *ds, const char *appname,
                                 DIAL_run_t run_id, int *pCanStop, void *callback_data) {
    // YouTube can stop
    *pCanStop = 1;
    return isAppRunning( spAppYouTube, spAppYouTubeMatch ) ? kDIALStatusRunning : kDIALStatusStopped;
}

static void youtube_stop(DIALServer *ds, const char *appname, DIAL_run_t run_id,
                         void *callback_data) {
    printf("\n\n ** KILL YouTube **\n\n");
    pid_t pid;
    if ((pid = isAppRunning( spAppYouTube, spAppYouTubeMatch ))) {
        kill(pid, SIGTERM);
    }
}

static DIALStatus netflix_start(DIALServer *ds, const char *appname,
                                const char *args, size_t arglen,
                                DIAL_run_t *run_id, void *callback_data) {
    int shouldRelaunchApp = 0;
    int payloadLen = 0;
    int appPid = 0;

    // only launch Netflix if it isn't running
    appPid = isAppRunning( spAppNetflix, NULL );
    shouldRelaunchApp = shouldRelaunch( ds, appname, args );

    // construct the payload to determine if it has changed from the previous launch
    payloadLen = strlen(args);
    memset( sQueryParam, 0, DIAL_MAX_PAYLOAD );
    strcat( sQueryParam, defaultLaunchParam );
    if( payloadLen )
    {
        char * pUrlEncodedParams;
        pUrlEncodedParams = url_encode( args );
        if( pUrlEncodedParams )
        {
            strcat( sQueryParam, "&dial=");
            strcat( sQueryParam, pUrlEncodedParams );
            free( pUrlEncodedParams );
        }
    }

    printf("appPid = %s, shouldRelaunch = %s queryParams = %s\n",
          appPid?"TRUE":"FALSE",
          shouldRelaunchApp?"TRUE":"FALSE",
          sQueryParam );

    // if its not running, launch it.  The Netflix application should
    // never be relaunched
    if( !appPid )
    {
        const char * const netflix_args[] = {spNetflix, "-Q", sQueryParam, 0};
        return runApplication( netflix_args, run_id );
    }
    else return kDIALStatusRunning;
}

static DIALStatus netflix_status(DIALServer *ds, const char *appname,
                                 DIAL_run_t run_id, int* pCanStop, void *callback_data) {
    // Netflix application can stop
    *pCanStop = 1;

    waitpid((pid_t)(long)run_id, NULL, WNOHANG); // reap child

    return isAppRunning( spAppNetflix, NULL ) ? kDIALStatusRunning : kDIALStatusStopped;
}

static void netflix_stop(DIALServer *ds, const char *appname, DIAL_run_t run_id,
                         void *callback_data) {
    int pid;
    pid = isAppRunning( spAppNetflix, NULL );
    if( pid )
    {
        printf("Killing pid %d\n", pid);
        kill((pid_t)pid, SIGTERM);
        waitpid((pid_t)pid, NULL, 0); // reap child
    }
}

void run_ssdp(int port, const char *pFriendlyName, const char * pModelName, const char *pUuid);

static void printUsage()
{
    int i, numberOfOptions = sizeof(gDialOptions) / sizeof(dial_options_t);
    printf("usage: dialserver <options>\n");
    printf("options:\n");
    for( i = 0; i < numberOfOptions; i++ )
    {
        printf("        %s|%s [value]: %s\n",
            gDialOptions[i].pOption,
            gDialOptions[i].pLongOption,
            gDialOptions[i].pOptionDescription );
    }
}

static void setValue( char * pSource, char dest[] )
{
    // Destination is always one of our static buffers with size BUFSIZE
    memset( dest, 0, BUFSIZE );
    memcpy( dest, pSource, strlen(pSource) );
}

static void setDataDir(char *pData)
{
    setValue( spNfDataDir, spDataDir );
    strcat(spDataDir, pData);
}

void runDial(void)
{
    DIALServer *ds;
    ds = DIAL_create();
    struct DIALAppCallbacks cb_nf = {netflix_start, netflix_stop, netflix_status};
    struct DIALAppCallbacks cb_yt = {youtube_start, youtube_stop, youtube_status};

    DIAL_register_app(ds, "Netflix", &cb_nf, NULL, 1, "netflix.com");
    DIAL_register_app(ds, "YouTube", &cb_yt, NULL, 1, "youtube.com");
    DIAL_start(ds);

    gDialPort = DIAL_get_port(ds);
    printf("launcher listening on gDialPort %d\n", gDialPort);
    run_ssdp(gDialPort, spFriendlyName, spModelName, spUuid);

    DIAL_stop(ds);
    free(ds);
}

static void processOption( int index, char * pOption )
{
    switch(index)
    {
        case 0: // Data path
            memset( spDataDir, 0, sizeof(spDataDir) );
            setDataDir( pOption );
            break;
        case 1: // Netflix path
            setValue( pOption, spNetflix );
            break;
        case 2: // Friendly name
            setValue( pOption, spFriendlyName );
            break;
        case 3: // Model Name
            setValue( pOption, spModelName );
            break;
        case 4: // UUID
            setValue( pOption, spUuid );
            break;
        default:
            // Should not get here
            fprintf( stderr, "Option %d not valid\n", index);
            exit(1);
    }
}

int main(int argc, char* argv[])
{
    srand(time(NULL));
    int i;
    i = isAppRunning(spAppNetflix, NULL );
    printf("Netflix is %s\n", i ? "Running":"Not Running");
    i = isAppRunning( spAppYouTube, spAppYouTubeMatch );
    printf("YouTube is %s\n", i ? "Running":"Not Running");

    // set all defaults
    setValue(spDefaultFriendlyName, spFriendlyName );
    setValue(spDefaultModelName, spModelName );
    setValue(spDefaultUuid, spUuid );
    setValue(spDefaultNetflix, spNetflix );
    setDataDir(spDefaultData);

    // Process command line options
    // Loop through pairs of command line options.
    for( i = 1; i < argc; i+=2 )
    {
        int numberOfOptions = sizeof(gDialOptions) / sizeof(dial_options_t);
        while( --numberOfOptions >= 0 )
        {
            int shortLen, longLen;
            shortLen = strlen(gDialOptions[numberOfOptions].pOption);
            longLen = strlen(gDialOptions[numberOfOptions].pLongOption);
            if( ( ( strncmp( argv[i], gDialOptions[numberOfOptions].pOption, shortLen ) == 0 ) ||
                ( strncmp( argv[i], gDialOptions[numberOfOptions].pLongOption, longLen ) == 0 ) ) &&
                ( (i+1) < argc ) )
            {
                processOption( numberOfOptions, argv[i+1] );
                break;
            }
        }
        // if we don't find an option in our list, bail out.
        if( numberOfOptions < 0 )
        {
            printUsage();
            exit(1);
        }
    }
    runDial();

    return 0;
}

