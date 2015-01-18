#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#include <mach/mach_port.h>
#include <mach/mach_interface.h>
#include <mach/mach_init.h>

#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>
#include "schedule.h"
#include "timeRoutines.h"

io_connect_t  root_port; // a reference to the Root Power Domain IOService
CFRunLoopTimerRef timerRef;

// Prototypes
void installTimer(scheduledExec *task); 
  
// Display Timer callback - Just fire a display update on the callback

static void timerCallBack(CFRunLoopTimerRef timer, void *info) 
{
    CFAbsoluteTime startTime = CFAbsoluteTimeGetCurrent();
    CFAbsoluteTime fireTime; 
    scheduledExec *currentTask = (scheduledExec *)info;
    #ifdef DEBUG
    printf("In Timer Call Back: %f\n", startTime);
    #endif DEBUG
    executeScheduledEntry(currentTask);
    CFRunLoopTimerContext context;

    scheduledExec *task = calcNextTaskAlarm();

    // Set up the timer. Convert from unix time to CoreFoundation time.
    fireTime = task->absTime - kCFAbsoluteTimeIntervalSince1970;

    CFRunLoopTimerGetContext(timer, &context);
    // Copy the new values to the scheduledExec within the context as the 
    // info pointer cannot be changed.
    currentTask = (scheduledExec *)context.info;
    currentTask->taskHead = task->taskHead;
    currentTask->absTime = task->absTime;
    free(task);
    
    CFRunLoopTimerSetNextFireDate(timer, fireTime);
}


void installTimer(scheduledExec *task) 
{
    CFAbsoluteTime fireTime;
    // Convert from unix time to CoreFoundation time.
    fireTime = task->absTime - kCFAbsoluteTimeIntervalSince1970;
    #ifdef DEBUG
    printf("In Timer Install: %f\n", fireTime);
    #endif DEBUG

    CFRunLoopTimerContext context = {0, task, NULL, NULL, NULL};
    timerRef =  CFRunLoopTimerCreate(NULL, fireTime, 10000000000000.0, 
                     0, 0, timerCallBack, &context);
    // TODO: May need to release timerRef at a later point
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timerRef, kCFRunLoopCommonModes);

}


void
powerCallBack( void * refCon, io_service_t service, natural_t messageType, void * messageArgument )
{
    switch ( messageType )
    {

        case kIOMessageCanSystemSleep:
            // Idle sleep is about to kick in. This message will not be 
            // sent for forced sleep. 
            IOAllowPowerChange( root_port, (long)messageArgument );
            break;

        case kIOMessageSystemWillSleep:
            // The system WILL go to sleep. Call IOAllowPowerChange to 
            // acknowledge. Otherwise, system will delay sleep by 30 seconds.
            
            IOAllowPowerChange( root_port, (long)messageArgument );
            break;

        case kIOMessageSystemWillPowerOn:
            //System has started the wake up process...
            break;

        case kIOMessageSystemHasPoweredOn:
            //System has finished waking up...
            // Cancel old timer and create new one with next scheduled task.  
            CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), timerRef, 
                    kCFRunLoopCommonModes);
            scheduledExec *task = calcNextTaskAlarm();
            installTimer(task);
        break;

        default:
            break;

    }
}


/**
 * Main entry point for setting up the timer.  
 */
int waitForTask(scheduledExec *task)
{
    // notification port allocated by IORegisterForSystemPower
    IONotificationPortRef notifyPortRef; 
    io_object_t notifierObject; // notifier object, used to deregister later
    void* refCon; // this parameter is passed to the callback

    // register to receive system sleep notifications if we have not 
    // already done so.
    if (root_port == 0) {
        root_port = IORegisterForSystemPower(refCon, &notifyPortRef, 
                powerCallBack, &notifierObject );
        if ( root_port == 0 )
        {
            printf("IORegisterForSystemPower failed\n");
            return 1;
        }
    }
    
    // Add the notification port to the application runloop
    CFRunLoopAddSource( CFRunLoopGetCurrent(),
             IONotificationPortGetRunLoopSource(notifyPortRef), 
             kCFRunLoopCommonModes ); 

    // Set up the timer. 
    installTimer(task);

    // Start the run loop to receive sleep notifications. 
    CFRunLoopRun();

    //Not reached, CFRunLoopRun doesn't return in this case.
    return (0);
}

