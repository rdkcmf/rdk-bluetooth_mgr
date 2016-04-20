/*
 * ============================================================================
 * RDK MANAGEMENT, LLC CONFIDENTIAL AND PROPRIETARY
 * ============================================================================
 * This file (and its contents) are the intellectual property of RDK Management, LLC.
 * It may not be used, copied, distributed or otherwise  disclosed in whole or in
 * part without the express written permission of RDK Management, LLC.
 * ============================================================================
 * Copyright (c) 2016 RDK Management, LLC. All rights reserved.
 * ============================================================================
 */
/**
 * @file btrMgr_streamOutTest.c
 *
 * @description This file implements bluetooth manager's Test App to streaming audio to external BT devices
 *
 * Copyright (c) 2016  Comcast
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* System Headers */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Interface lib Headers */
#include "btrMgr_streamOut.h"


/* Local Defines */
#define IN_BUF_SIZE     4096
#define OUT_MTU_SIZE    1024


#define WAV_HEADER_RIFF_HEX    0x46464952
#define WAV_HEADER_WAV_HEX     0x45564157
#define WAV_HEADER_DC_ID_HEX   0x61746164


typedef struct _stAudioWavHeader {

    unsigned long   ulRiff32Bits;
    unsigned long   ulRiffSize32Bits;
    unsigned long   ulWave32Bits;
    unsigned long   ulWaveFmt32Bits;
    unsigned long   ulWaveHeaderLength32Bits;
    unsigned long   ulSampleRate32Bits;
    unsigned long   ulByteRate32Bits;
    unsigned long   ulMask32Bits;
    unsigned long   ulDataId32Bits;
    unsigned long   ulDataLength32Bits;

    unsigned short  usWaveHeaderFmt16Bits;
    unsigned short  usNumAudioChannels16Bits;
    unsigned short  usBitsPerChannel16Bits;
    unsigned short  usBitRate16Bits;
    unsigned short  usBlockAlign16Bits;
    unsigned short  usBitsPerSample16Bits;

    unsigned char   ucFormatArr16x8Bits[16];
    
} stAudioWavHeader;


static int 
extractWavHeader (
    FILE*               fpInAudioFile,
    stAudioWavHeader*   pstAudioWaveHeader
) {
    if (!fpInAudioFile || !pstAudioWaveHeader)
        return -1;

    if (fread(&pstAudioWaveHeader->ulRiff32Bits, 4, 1, fpInAudioFile) != 1) {
        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }

    if (pstAudioWaveHeader->ulRiff32Bits != WAV_HEADER_RIFF_HEX) {
        fprintf(stderr,"RAW data file detected.");

        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }   

    if (fread(&pstAudioWaveHeader->ulRiffSize32Bits, 4, 1, fpInAudioFile) != 1) {
        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }

    if (fread(&pstAudioWaveHeader->ulWave32Bits, 4, 1, fpInAudioFile) != 1) {
        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }

    if (pstAudioWaveHeader->ulWave32Bits != WAV_HEADER_WAV_HEX) {
        fprintf(stderr,"Not a WAV file.");

        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }   

    if (fread(&pstAudioWaveHeader->ulWaveFmt32Bits, 4, 1, fpInAudioFile) != 1) {
        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }

    if (fread(&pstAudioWaveHeader->ulWaveHeaderLength32Bits,4,1, fpInAudioFile) != 1) {
        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }

    if (fread(&pstAudioWaveHeader->usWaveHeaderFmt16Bits, 2, 1, fpInAudioFile) != 1) {
        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }


    if (fread(&pstAudioWaveHeader->usNumAudioChannels16Bits, 2, 1, fpInAudioFile) != 1) {
        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }


    if (pstAudioWaveHeader->usNumAudioChannels16Bits > 2) {
        fprintf(stderr,"Invalid number of usNumAudioChannels16Bits (%u) specified.", pstAudioWaveHeader->usNumAudioChannels16Bits);

        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }   

    if (fread(&pstAudioWaveHeader->ulSampleRate32Bits, 4, 1, fpInAudioFile) != 1) {
        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }


    if (fread(&pstAudioWaveHeader->ulByteRate32Bits, 4, 1, fpInAudioFile) != 1) {
        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }


    if (fread(&pstAudioWaveHeader->usBitsPerChannel16Bits, 2, 1, fpInAudioFile) != 1) {
        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }

    if (fread(&pstAudioWaveHeader->usBitRate16Bits, 2, 1, fpInAudioFile) != 1) {
        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }


    if (pstAudioWaveHeader->ulWaveHeaderLength32Bits == 40 && pstAudioWaveHeader->usWaveHeaderFmt16Bits == 0xfffe) {

        if (fread(&pstAudioWaveHeader->usBlockAlign16Bits,2,1, fpInAudioFile) != 1) {
            fseek( fpInAudioFile, 0, SEEK_SET);
            return -1; 
        }

        if (fread(&pstAudioWaveHeader->usBitsPerSample16Bits, 2, 1, fpInAudioFile) != 1) {
            fseek( fpInAudioFile, 0, SEEK_SET);
            return -1; 
        }

        if (fread(&pstAudioWaveHeader->ulMask32Bits, 4, 1, fpInAudioFile) != 1) {
            fseek( fpInAudioFile, 0, SEEK_SET);
            return -1; 
        }

        fread(&pstAudioWaveHeader->ucFormatArr16x8Bits, 16, 1, fpInAudioFile);
    }   
    else if (pstAudioWaveHeader->ulWaveHeaderLength32Bits == 18 && pstAudioWaveHeader->usWaveHeaderFmt16Bits == 1) {

        if (fread(&pstAudioWaveHeader->usBlockAlign16Bits, 2, 1, fpInAudioFile) != 1) {
            fseek( fpInAudioFile, 0, SEEK_SET);
            return -1; 
        }

    }   
    else if (pstAudioWaveHeader->ulWaveHeaderLength32Bits != 16 && pstAudioWaveHeader->usWaveHeaderFmt16Bits != 1) {
        fprintf(stderr,"No PCM data in WAV file. ulWaveHeaderLength32Bits = %lu, Format 0x%x", pstAudioWaveHeader->ulWaveHeaderLength32Bits,pstAudioWaveHeader->usWaveHeaderFmt16Bits);
    }   

    do {
        if (fread(&pstAudioWaveHeader->ulDataId32Bits, 4, 1, fpInAudioFile) != 1) {
            fseek( fpInAudioFile, 0, SEEK_SET);
            return -1; 
        }

        if (fread(&pstAudioWaveHeader->ulDataLength32Bits,4,1, fpInAudioFile) != 1) {
            fseek( fpInAudioFile, 0, SEEK_SET);
            return -1; 
        }

        if (pstAudioWaveHeader->ulDataId32Bits == WAV_HEADER_DC_ID_HEX) {
            break;
        }
        if (fseek( fpInAudioFile, pstAudioWaveHeader->ulDataLength32Bits, SEEK_CUR)) {
            fprintf(stderr,"Incomplete chunk found WAV file.");

            fseek( fpInAudioFile, 0, SEEK_SET);
            return -1; 
        }
    } while(1);  

    return 0;
}


static void
helpMenu (
    void
) {
        fprintf(stderr,"\nbtrMgrStreamOutTest <Mode: 0=BTDev/1=FWrite> <Input Wav File> <BTDev FD/Output File> <Out BTDevMTU/FileWriteBlock Size>\n");
        return;
}


int main (
    int     argc,
    char*   argv[]
) {

    char    *inDataBuf      = NULL;

    int     inFileBytesLeft = 0;
    int     inBytesToEncode = 0;
    int     inBufSize       = IN_BUF_SIZE;

    int     streamOutMode   = 0;
    FILE*   inFileFp        = NULL;
    FILE*   outFileFp       = NULL;
    int     outFileFd       = 0;
    int     outMTUSize      = OUT_MTU_SIZE;


    stAudioWavHeader lstAudioWavHeader;

    if (argc != 5) {
        fprintf(stderr,"Invalid number of arguments\n");
        helpMenu();
        return -1;
    }

    streamOutMode = atoi(argv[1]);

    inFileFp    = fopen(argv[2], "rb");
    if (!inFileFp) {
        fprintf(stderr,"Invalid input file\n");
        helpMenu();
        return -1;
    }

    if (streamOutMode == 0) {
        outFileFd = atoi(argv[3]);
    }
    else if (streamOutMode == 1) {
        outFileFp   = fopen(argv[3], "wb");
        if (!outFileFp) {
            fprintf(stderr,"Invalid output file\n");
            helpMenu();
            return -1;
        }

        outFileFd = fileno(outFileFp);
    }
    else {
        helpMenu();
        return -1;
    }

    outMTUSize  = atoi(argv[4]);

    fseek(inFileFp, 0, SEEK_END);
    inFileBytesLeft = ftell(inFileFp);
    fseek(inFileFp, 0, SEEK_SET);

#if defined(DISABLE_AUDIO_ENCODING)
    (void)extractWavHeader;
#else
    memset(&lstAudioWavHeader, 0, sizeof(lstAudioWavHeader));
    if (extractWavHeader(inFileFp, &lstAudioWavHeader)) {
        fprintf(stderr,"Invalid output file\n");
        return -1;
    }
#endif

    BTRMgr_SO_Init();

    inDataBuf = (char*)malloc(inBufSize * sizeof(char));
    inBytesToEncode = inBufSize;

    BTRMgr_SO_Start(inBytesToEncode, outFileFd, outMTUSize);


    while (inFileBytesLeft) {

        if (inFileBytesLeft < inBufSize)
            inBytesToEncode = inFileBytesLeft;

        usleep((float)inBytesToEncode * 1000000.0/1764000.0);

        fread (inDataBuf, 1, inBytesToEncode, inFileFp);
        BTRMgr_SO_SendBuffer(inDataBuf, inBytesToEncode);
        inFileBytesLeft -= inBytesToEncode;

    }


    BTRMgr_SO_SendEOS();

    BTRMgr_SO_Stop();

    free(inDataBuf);

    BTRMgr_SO_DeInit();

    if (streamOutMode == 1)
        fclose(outFileFp);

    fclose(inFileFp);

	return 0;
}
