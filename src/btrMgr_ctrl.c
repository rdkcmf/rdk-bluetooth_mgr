/**
 * @file main.c
 *
 * @description This file defines bluetooth manager's main function
 *
 * Copyright (c) 2015  Comcast
 */
#include <stdlib.h>
#include <signal.h>


/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
//static void __terminate_listener(int value);
static void sig_handler(int sig);


/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
#if 0
int main(int argc,char *argv[])
#else
int stub(int argc,char *argv[])
#endif
{
	const char* debugConfigFile = NULL;
	signal(SIGTERM, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGUSR1, sig_handler);
	signal(SIGUSR2, sig_handler);
	signal(SIGSEGV, sig_handler);
	signal(SIGBUS, sig_handler);
	signal(SIGKILL, sig_handler);
	signal(SIGFPE, sig_handler);
	signal(SIGILL, sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGHUP, sig_handler);
	signal(SIGALRM, sig_handler);

	while(1)
        {
           sleep(1);
        }
	return 1;
}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
static void sig_handler(int sig)
{

        signal(sig, SIG_DFL );
        kill(getpid(), sig );

}

