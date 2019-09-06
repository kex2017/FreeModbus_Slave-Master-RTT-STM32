/*
 * FreeModbus Libary: RT-Thread Port
 * Copyright (C) 2013 Armink <armink.ztl@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * File: $Id: portevent_m.c v 1.60 2013/08/13 15:07:05 Armink add Master Functions$
 */

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mb_m.h"
#include "mbport.h"

#include "semaphore.h"
#include "msg.h"
#include "sched.h"

#include "port.h"
#include "thread.h"

#include <stdio.h>
#include "xtimer.h"

static kernel_pid_t wait_resp_pid;

#if MB_MASTER_RTU_ENABLED > 0 || MB_MASTER_ASCII_ENABLED > 0
/* ----------------------- Defines ------------------------------------------*/
#define RCV_QUEUE_SIZE  (8)
/* ----------------------- Variables ----------------------------------------*/
//static struct rt_semaphore xMasterRunRes;
//static struct rt_event     xMasterOsEvent;
static msg_t rcv_queue[RCV_QUEUE_SIZE];
static msg_t post_msg;
static msg_t get_msg;
static sem_t xMasterRunRes;
/* ----------------------- Start implementation -----------------------------*/
BOOL
xMBMasterPortEventInit( void )
{
    msg_init_queue(rcv_queue, RCV_QUEUE_SIZE);
//    rt_event_init(&xMasterOsEvent,"master event",RT_IPC_FLAG_PRIO);
    return TRUE;
}

BOOL
xMBMasterPortEventPost( eMBMasterEventType eEvent )
{
//    rt_event_send(&xMasterOsEvent, eEvent);
    post_msg.type = eEvent;
    if ((eEvent == EV_MASTER_PROCESS_SUCESS) || (eEvent == EV_MASTER_ERROR_RESPOND_TIMEOUT) || (eEvent == EV_MASTER_ERROR_RECEIVE_DATA)
                    || (eEvent == EV_MASTER_ERROR_EXECUTE_FUNCTION)) {

        msg_send(&post_msg, wait_resp_pid);
    }
    else {
        msg_try_send(&post_msg, get_master_poll_pid());
    }

    return TRUE;
}

//BOOL
//xMBMasterPortEventGet( eMBMasterEventType * eEvent )
//{
//    int ret = msg_try_receive(&get_msg);
////    rt_uint32_t recvedEvent;
//    /* waiting forever OS event */
////    rt_event_recv(&xMasterOsEvent,
////            EV_MASTER_READY | EV_MASTER_FRAME_RECEIVED | EV_MASTER_EXECUTE |
////            EV_MASTER_FRAME_SENT | EV_MASTER_ERROR_PROCESS,
////            RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER,
////            &recvedEvent);
//    /* the enum type couldn't convert to int type */
//    switch (get_msg.type)
//    {
//    case EV_MASTER_READY:
//        *eEvent = EV_MASTER_READY;
//        break;
//    case EV_MASTER_FRAME_RECEIVED:
//        *eEvent = EV_MASTER_FRAME_RECEIVED;
//        break;
//    case EV_MASTER_EXECUTE:
//        *eEvent = EV_MASTER_EXECUTE;
//        break;
//    case EV_MASTER_FRAME_SENT:
//        *eEvent = EV_MASTER_FRAME_SENT;
//        break;
//    case EV_MASTER_ERROR_PROCESS:
//        *eEvent = EV_MASTER_ERROR_PROCESS;
//        break;
//    }
//    return ret > 0 ? TRUE : FALSE;
//}
/**
 * This function is initialize the OS resource for modbus master.
 * Note:The resource is define by OS.If you not use OS this function can be empty.
 *
 */
void vMBMasterOsResInit( void )
{
    sem_init(&xMasterRunRes, 0, 0x01);
    wait_resp_pid = thread_getpid();
//    rt_sem_init(&xMasterRunRes, "master res", 0x01 , RT_IPC_FLAG_PRIO);
}

/**
 * This function is take Mobus Master running resource.
 * Note:The resource is define by Operating System.If you not use OS this function can be just return TRUE.
 *
 * @param lTimeOut the waiting time.
 *
 * @return resource taked result
 */
BOOL xMBMasterRunResTake( LONG lTimeOut )
{
    BOOL stat;
    uint64_t start;

    /*If waiting time is -1 .It will wait forever */
    if (-1 == lTimeOut) {
        return sem_wait(&xMasterRunRes) ? FALSE : TRUE;
    }
    else {
        struct timespec abs;
        abs.tv_sec = lTimeOut;
        start = xtimer_now_usec64();
        abs.tv_sec = (time_t)((start / US_PER_SEC) + lTimeOut);
        abs.tv_nsec = (long)((start % US_PER_SEC) * 1000);
        stat = sem_timedwait(&xMasterRunRes, &abs) ? FALSE : TRUE;
        return stat;
    }
//    return rt_sem_take(&xMasterRunRes, lTimeOut) ? FALSE : TRUE ;
}

/**
 * This function is release Mobus Master running resource.
 * Note:The resource is define by Operating System.If you not use OS this function can be empty.
 *
 */
void vMBMasterRunResRelease( void )
{
    /* release resource */
    sem_post(&xMasterRunRes);
//    rt_sem_release(&xMasterRunRes);
}

/**
 * This is modbus master respond timeout error process callback function.
 * @note There functions will block modbus master poll while execute OS waiting.
 * So,for real-time of system.Do not execute too much waiting process.
 *
 * @param ucDestAddress destination salve address
 * @param pucPDUData PDU buffer data
 * @param ucPDULength PDU buffer length
 *
 */
void vMBMasterErrorCBRespondTimeout(UCHAR ucDestAddress, const UCHAR* pucPDUData,
        USHORT ucPDULength) {
    (void)ucDestAddress;
    (void)pucPDUData;
    (void)ucPDULength;
    /**
     * @note This code is use OS's event mechanism for modbus master protocol stack.
     * If you don't use OS, you can change it.
     */
    xMBMasterPortEventPost(EV_MASTER_ERROR_RESPOND_TIMEOUT);
//    rt_event_send(&xMasterOsEvent, EV_MASTER_ERROR_RESPOND_TIMEOUT);

    /* You can add your code under here. */

}

/**
 * This is modbus master receive data error process callback function.
 * @note There functions will block modbus master poll while execute OS waiting.
 * So,for real-time of system.Do not execute too much waiting process.
 *
 * @param ucDestAddress destination salve address
 * @param pucPDUData PDU buffer data
 * @param ucPDULength PDU buffer length
 *
 */
void vMBMasterErrorCBReceiveData(UCHAR ucDestAddress, const UCHAR* pucPDUData,
        USHORT ucPDULength) {
    (void)ucDestAddress;
    (void)pucPDUData;
    (void)ucPDULength;
    /**
     * @note This code is use OS's event mechanism for modbus master protocol stack.
     * If you don't use OS, you can change it.
     */
    xMBMasterPortEventPost(EV_MASTER_ERROR_RECEIVE_DATA);
//    post_msg.type = EV_MASTER_ERROR_RECEIVE_DATA;
//    msg_send(&post_msg, sched_active_pid);
//    rt_event_send(&xMasterOsEvent, EV_MASTER_ERROR_RECEIVE_DATA);

    /* You can add your code under here. */

}

/**
 * This is modbus master execute function error process callback function.
 * @note There functions will block modbus master poll while execute OS waiting.
 * So,for real-time of system.Do not execute too much waiting process.
 *
 * @param ucDestAddress destination salve address
 * @param pucPDUData PDU buffer data
 * @param ucPDULength PDU buffer length
 *
 */
void vMBMasterErrorCBExecuteFunction(UCHAR ucDestAddress, const UCHAR* pucPDUData,
        USHORT ucPDULength) {
    (void)ucDestAddress;
    (void)pucPDUData;
    (void)ucPDULength;
    /**
     * @note This code is use OS's event mechanism for modbus master protocol stack.
     * If you don't use OS, you can change it.
     */

    xMBMasterPortEventPost(EV_MASTER_ERROR_EXECUTE_FUNCTION);

    /* You can add your code under here. */

}

/**
 * This is modbus master request process success callback function.
 * @note There functions will block modbus master poll while execute OS waiting.
 * So,for real-time of system.Do not execute too much waiting process.
 *
 */
void vMBMasterCBRequestScuuess( void ) {
    /**
     * @note This code is use OS's event mechanism for modbus master protocol stack.
     * If you don't use OS, you can change it.
     */
    xMBMasterPortEventPost(EV_MASTER_PROCESS_SUCESS);

    /* You can add your code under here. */

}

/**
 * This function is wait for modbus master request finish and return result.
 * Waiting result include request process success, request respond timeout,
 * receive data error and execute function error.You can use the above callback function.
 * @note If you are use OS, you can use OS's event mechanism. Otherwise you have to run
 * much user custom delay for waiting.
 *
 * @return request error code
 */
eMBMasterReqErrCode eMBMasterWaitRequestFinish( void ) {
    eMBMasterReqErrCode    eErrStatus = MB_MRE_NO_ERR;
    wait_resp_pid = thread_getpid();
//    rt_uint32_t recvedEvent;
    /* waiting for OS event */
//    rt_event_recv(&xMasterOsEvent,
//            EV_MASTER_PROCESS_SUCESS | EV_MASTER_ERROR_RESPOND_TIMEOUT
//                    | EV_MASTER_ERROR_RECEIVE_DATA
//                    | EV_MASTER_ERROR_EXECUTE_FUNCTION,
//            RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER,
//            &recvedEvent);
    msg_receive(&get_msg);

    switch (get_msg.type)
    {
    case EV_MASTER_PROCESS_SUCESS:
        break;
    case EV_MASTER_ERROR_RESPOND_TIMEOUT:
    {
        eErrStatus = MB_MRE_TIMEDOUT;
        break;
    }
    case EV_MASTER_ERROR_RECEIVE_DATA:
    {
        eErrStatus = MB_MRE_REV_DATA;
        break;
    }
    case EV_MASTER_ERROR_EXECUTE_FUNCTION:
    {
        eErrStatus = MB_MRE_EXE_FUN;
        break;
    }
    }
    return eErrStatus;
}

#endif
