#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "btmgr.h"
#include <fcgi_stdio.h>

void GetNumberOfAdapters ()
{
    unsigned char numOfAdapters = 0;
    if (BTMGR_RESULT_SUCCESS != BTMGR_GetNumberOfAdapters(&numOfAdapters))
        printf ("\n\n Failed to get the count\n\n");
    else
        printf ("\n\n Got the count as %d\n", numOfAdapters);

    return;
}

int main ()
{
#if 0
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    rc = BTMGR_Init();

    if (BTMGR_RESULT_SUCCESS != rc)
    {
        fprintf (stderr, "Failed to init BTMgr.. Quiting.. \n");
        return 0;
    }
#endif
    int initOnce = 0;

    while (FCGI_Accept() >= 0)
    {
        const char* func = getenv("REQUEST_URI");

        if (!func)
        {
            fprintf(stderr, "No No No.. This is totally not good..! :(\n");
        }
        else
        {
            printf("Content-type: application/json\r\n\r\n");
            fprintf (stderr, "The func is ..  %s\n", func);
            if (NULL != strstr (func, "/btmgr/"))
            {
                if (0 == initOnce)
                {
                    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
                    rc = BTMGR_Init();

                    if (BTMGR_RESULT_SUCCESS != rc)
                    {
                        fprintf (stderr, "Failed to init BTMgr.. Quiting.. \n");
                        return 0;
                    }
                    initOnce = 1;
                }


                if (NULL != strstr (func, "GetNumberOfAdapters"))
                {
                    GetNumberOfAdapters();
                }
                else
                {
                    printf ("Seems new.. Lets handle this soon..  %s\n", func);
                }
            }
            else
            {
                fprintf (stderr, "how come it landed here!?!?!\n");
            }
        }
        FCGI_Finish();
    }

    return 0;
}
