/*
 * AWS IoT Over-the-air Update v3.3.0
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * @file ota_os_freertos.c
 * @brief Example implementation of the OTA OS Functional Interface for
 * FreeRTOS.
 */

/* FreeRTOS includes. */
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/queue.h"

/* OTA OS POSIX Interface Includes.*/
#include "ota_os_freertos.h"

#include "ota_config.h"

/* OTA Event queue attributes.*/
#define MAX_MESSAGES    20
#define MAX_MSG_SIZE    sizeof( OtaEventMsg_t )

/* Backing store for the static queue: MAX_MESSAGES elements, not MAX_MESSAGES
 * *bytes*' worth of them.
 *
 * F-OTA-019: this array is typed OtaEventMsg_t, so multiplying by MAX_MSG_SIZE
 * (which is sizeof( OtaEventMsg_t )) sized it in units of the element size a
 * second time - 20 * 12 = 240 elements, 2,880 bytes, for a queue that can only
 * ever hold 20 of them. xQueueCreateStatic() below asks for
 * MAX_MESSAGES * MAX_MSG_SIZE *bytes*, which this declaration provides exactly,
 * and the extra 2,640 bytes sat unused in internal BSS for the life of the
 * image. Internal RAM is the scarce pool on this gateway. */
static OtaEventMsg_t queueData[ MAX_MESSAGES ];

/* The storage area is the one thing xQueueCreateStatic() cannot check for
 * itself: too small and it writes past the array. State the requirement here so
 * a future edit to either MAX_MESSAGES or the element type fails the build
 * instead of the field. */
_Static_assert( sizeof( queueData ) >= ( MAX_MESSAGES * MAX_MSG_SIZE ),
                "OTA event queue storage is smaller than the queue it backs" );

/* The queue control structure.  .*/
static StaticQueue_t staticQueue;

/* The queue control handle.  .*/
static QueueHandle_t otaEventQueue;

OtaOsStatus_t OtaInitEvent_FreeRTOS( void )
{
    OtaOsStatus_t otaOsStatus = OtaOsSuccess;

    otaEventQueue = xQueueCreateStatic( ( UBaseType_t ) MAX_MESSAGES,
                                        ( UBaseType_t ) MAX_MSG_SIZE,
                                        ( uint8_t * ) queueData,
                                        &staticQueue );

    if( otaEventQueue == NULL )
    {
        otaOsStatus = OtaOsEventQueueCreateFailed;

        LogError( ( "Failed to create OTA Event Queue: "
                    "xQueueCreateStatic returned error: "
                    "OtaOsStatus_t=%i ",
                    otaOsStatus ) );
    }
    else
    {
        LogDebug( ( "OTA Event Queue created." ) );
    }

    return otaOsStatus;
}

OtaOsStatus_t OtaSendEvent_FreeRTOS( const void * pEventMsg )
{
    OtaOsStatus_t otaOsStatus = OtaOsSuccess;
    BaseType_t retVal = pdFALSE;

    /* Send the event to OTA event queue.*/
    retVal = xQueueSendToBack( otaEventQueue, pEventMsg, ( TickType_t ) 0 );

    if( retVal == pdTRUE )
    {
        LogDebug( ( "OTA Event Sent." ) );
    }
    else
    {
        otaOsStatus = OtaOsEventQueueSendFailed;

        LogError( ( "Failed to send event to OTA Event Queue: "
                    "xQueueSendToBack returned error: "
                    "OtaOsStatus_t=%i ",
                    otaOsStatus ) );
    }

    return otaOsStatus;
}

OtaOsStatus_t OtaReceiveEvent_FreeRTOS( void * pEventMsg )
{
    OtaOsStatus_t otaOsStatus = OtaOsSuccess;
    BaseType_t retVal = pdFALSE;

    /* Temp buffer.*/
    uint8_t buff[ sizeof( OtaEventMsg_t ) ];

    retVal = xQueueReceive( otaEventQueue, &buff, portMAX_DELAY );

    if( retVal == pdTRUE )
    {
        /* copy the data from local buffer.*/
        memcpy( pEventMsg, buff, MAX_MSG_SIZE );
        LogDebug( ( "OTA Event received" ) );
    }
    else
    {
        otaOsStatus = OtaOsEventQueueReceiveFailed;

        LogError( ( "Failed to receive event from OTA Event Queue: "
                    "xQueueReceive returned error: "
                    "OtaOsStatus_t=%i ",
                    otaOsStatus ) );
    }

    return otaOsStatus;
}

OtaOsStatus_t OtaDeinitEvent_FreeRTOS( void )
{
    OtaOsStatus_t otaOsStatus = OtaOsSuccess;

    /* Remove the event queue.*/
    if( otaEventQueue != NULL )
    {
        vQueueDelete( otaEventQueue );

        LogDebug( ( "OTA Event Queue Deleted." ) );
    }

    return otaOsStatus;
}

void * Malloc_FreeRTOS( size_t size )
{
    return pvPortMalloc( size );
}

void Free_FreeRTOS( void * ptr )
{
    vPortFree( ptr );
}
